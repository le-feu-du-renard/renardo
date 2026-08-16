#include <Arduino.h>
#include <Wire.h>
#include <hardware/watchdog.h>

#include "config.h"
#include "Dryer.h"
#include "Rs485Bus.h"
#include "ModbusSensors.h"
#include "HydraulicRemote.h"
#include "SharedSensorState.h"
#include "InputHandler.h"
#include "TimeManager.h"
#include "Logger.h"

// ========== GLOBAL OBJECTS ==========

// I2C bus (optional RTC DS1307)
TwoWire i2c_bus_1(i2c1, I2C_BUS_1_SDA_PIN, I2C_BUS_1_SCL_PIN);

// RS485 — single Modbus bus (probes @1 @2 + hydraulic module @10),
// owned exclusively by Core 1
Rs485Bus rs485(Serial2, RS485_TX_PIN, RS485_RX_PIN, RS485_DE_PIN, "rs485");
ModbusSensors modbus_sensors(&rs485);
HydraulicRemote hydraulic_remote(&rs485);

// Physical I/O
InputHandler input_handler;

// RTC — optional module; absence disables ECO mode
TimeManager time_manager(&i2c_bus_1);
static bool g_rtc_available = false;

// Main controller
Dryer dryer;

// ========== CORE 1 ==========

// Core 1 owns the RS485 bus exclusively: both probes and the hydraulic module.
// It publishes a coherent snapshot that Core 0 reads without blocking.

static SharedSensorState g_sensor_state;

// Hydraulic command travels the other way, Core 0 -> Core 1. A single bool and
// a float are each written by one core and read by the other, so they need no
// seqlock: a torn read simply means the command applies one cycle later.
static volatile bool  g_hydraulic_request = false;
static volatile float g_water_target = WATER_TARGET_DEFAULT;

static volatile bool g_core0_ready = false;

void setup1()
{
  while (!g_core0_ready)
  {
  } // Wait for Core 0 to finish setup

  rs485.Begin(MODBUS_BAUDRATE);
  modbus_sensors.Begin();
  hydraulic_remote.Begin();
}

void loop1()
{
  modbus_sensors.Poll(ModbusSensors::kInlet);
  modbus_sensors.Poll(ModbusSensors::kOutlet);

  hydraulic_remote.SetState(g_hydraulic_request);
  hydraulic_remote.SetWaterTarget(g_water_target);
  hydraulic_remote.Update();

  const SensorReading &inlet  = modbus_sensors.GetReading(ModbusSensors::kInlet);
  const SensorReading &outlet = modbus_sensors.GetReading(ModbusSensors::kOutlet);

  SensorSnapshot snapshot;
  snapshot.inlet_temperature  = inlet.temperature;
  snapshot.inlet_humidity     = inlet.humidity;
  snapshot.inlet_updated_ms   = inlet.last_success_ms;
  snapshot.inlet_valid        = inlet.valid;
  snapshot.outlet_temperature = outlet.temperature;
  snapshot.outlet_humidity    = outlet.humidity;
  snapshot.outlet_updated_ms  = outlet.last_success_ms;
  snapshot.outlet_valid       = outlet.valid;
  snapshot.water_temperature  = hydraulic_remote.GetWaterTemperature();
  snapshot.tank_temperature   = hydraulic_remote.GetTankTemperature();
  snapshot.hydraulic_available = hydraulic_remote.IsAvailable();

  g_sensor_state.Publish(snapshot);

  delay(SENSOR_UPDATE_INTERVAL);
}

// ========== TIMING STATE ==========

static uint32_t last_input_update = 0;

static constexpr uint32_t kMemoryCheckInterval = 30000; // 30 s
static constexpr uint32_t kHeartbeatInterval = 10000;   // 10 s
static uint32_t last_memory_check = 0;
static uint32_t last_heartbeat = 0;
static uint32_t loop_count = 0;

// ========== SETUP HELPERS ==========

static void SetupI2C()
{
  pinMode(I2C_BUS_1_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_BUS_1_SCL_PIN, INPUT_PULLUP);
  i2c_bus_1.begin();
  i2c_bus_1.setClock(10000);
  i2c_bus_1.setTimeout(1000);
  Logger::Info("I2C bus 1 ready (10kHz)");
}

// TODO(v4): move to OutputDriver, which will own polarity and edge detection.
static void WriteOutput(uint8_t pin, bool active_low, bool active)
{
  digitalWrite(pin, (active != active_low) ? HIGH : LOW);
}

static void SetupOutputs()
{
  // Drive each stage to its inactive level before switching the pin to OUTPUT,
  // so no load is energised during the boot window.
  WriteOutput(OUT_FAN_PIN, OUT_FAN_ACTIVE_LOW, false);
  WriteOutput(OUT_DAMPER_PIN, OUT_DAMPER_ACTIVE_LOW, false);
  WriteOutput(OUT_ELECTRIC_PIN, OUT_ELECTRIC_ACTIVE_LOW, false);
  pinMode(OUT_FAN_PIN, OUTPUT);
  pinMode(OUT_DAMPER_PIN, OUTPUT);
  pinMode(OUT_ELECTRIC_PIN, OUTPUT);
}

// The RTC is an optional module: probe it and degrade gracefully when absent.
static void SetupRTC()
{
  g_rtc_available = time_manager.Begin();
  if (!g_rtc_available)
  {
    Logger::Warning("RTC not detected — ECO mode unavailable");
    return;
  }

  if (time_manager.HasLostPower())
  {
    Logger::Warning("RTC lost power — time may be incorrect");
  }
  Logger::Info("RTC ready: %s", time_manager.GetDateTimeString());
}

// ========== SENSOR UPDATE ==========
// Non-blocking: Core 1 handles Modbus reads; Core 0 just copies the latest values.

static uint32_t last_sensor_log = 0;
static SensorSnapshot g_sensors;   // latest snapshot, refreshed every loop

static void UpdateSensors()
{
  uint32_t now = millis();

  g_sensor_state.Read(g_sensors);

  dryer.SetInletTemperature(g_sensors.inlet_temperature);
  dryer.SetInletHumidity(g_sensors.inlet_humidity);
  dryer.SetOutletTemperature(g_sensors.outlet_temperature);
  dryer.SetOutletHumidity(g_sensors.outlet_humidity);

  // Sensor freshness interlock. The inlet probe is the control input: if it
  // goes silent, its last value would otherwise stay frozen forever and the
  // heaters would keep chasing a stale reading. v3 declared both
  // SENSOR_TIMEOUT_MS and SetElectricEnabled() for this and wired neither.
  bool inlet_fresh = g_sensors.inlet_valid &&
                     (now - g_sensors.inlet_updated_ms) < SENSOR_TIMEOUT_MS;

  TemperatureManager *temperature_manager = dryer.GetTemperatureManager();
  temperature_manager->SetHeatingPermitted(inlet_fresh);

  // The hydraulic module is optional at runtime: losing it degrades to
  // electric-only rather than stopping the session.
  temperature_manager->SetHydraulicOnline(g_sensors.hydraulic_available);

  if (now - last_sensor_log >= SENSOR_UPDATE_INTERVAL)
  {
    last_sensor_log = now;
    Logger::Debug("Inlet:  %F C  %F%%RH", g_sensors.inlet_temperature, g_sensors.inlet_humidity);
    Logger::Debug("Outlet: %F C  %F%%RH", g_sensors.outlet_temperature, g_sensors.outlet_humidity);
    Logger::Debug("Water:  %F C  Tank %F C", g_sensors.water_temperature, g_sensors.tank_temperature);
  }
}

// ========== INPUT UPDATE ==========

static void UpdateInputs()
{
  uint32_t now = millis();
  if (now - last_input_update < INPUT_UPDATE_INTERVAL)
    return;
  last_input_update = now;

  input_handler.Update();

  // ECO mode needs the wall clock; without an RTC it stays in PERFORMANCE.
  if (g_rtc_available)
  {
    dryer.SetCurrentHour(time_manager.GetNow().hour());
  }
  else
  {
    dryer.SetOperatingMode(OperatingMode::PERFORMANCE);
  }

  if (input_handler.IsStartPressed() && !dryer.IsRunning())
  {
    Logger::Info("START button pressed — starting session");
    dryer.Start();
  }

  if (input_handler.IsStopPressed() && dryer.IsRunning())
  {
    Logger::Info("STOP button pressed — stopping session");
    dryer.Stop();
  }
}

// ========== OUTPUT UPDATE ==========

static void UpdateOutputs()
{
  static bool last_heater = false;
  static bool last_fan = false;
  static bool last_damper = false;

  bool heater_state = dryer.GetHeaterOutput() > 0.5f;
  bool fan_state = dryer.GetFanOutput() > 0.0f;
  bool damper_state = dryer.GetDamperOutput();

  // Hand the hydraulic on/off request to the core that owns the RS485 bus.
  g_hydraulic_request = dryer.GetHydraulicOn();

  if (heater_state != last_heater)
  {
    WriteOutput(OUT_ELECTRIC_PIN, OUT_ELECTRIC_ACTIVE_LOW, heater_state);
    last_heater = heater_state;
    Logger::Info("Electric heater: %s", heater_state ? "ON" : "OFF");
  }

  if (fan_state != last_fan)
  {
    WriteOutput(OUT_FAN_PIN, OUT_FAN_ACTIVE_LOW, fan_state);
    last_fan = fan_state;
    Logger::Info("Fan: %s", fan_state ? "ON" : "OFF");
  }

  if (damper_state != last_damper)
  {
    WriteOutput(OUT_DAMPER_PIN, OUT_DAMPER_ACTIVE_LOW, damper_state);
    last_damper = damper_state;
    Logger::Info("Air damper: %s", damper_state ? "EXTRACTION" : "RECIRCULATION");
  }
}

// ========== DIAGNOSTICS ==========

static void UpdateDiagnostics()
{
  uint32_t now = millis();

  if (now - last_heartbeat >= kHeartbeatInterval)
  {
    last_heartbeat = now;
    Logger::Info("Heartbeat — loops=%lu uptime=%us running=%s",
                 loop_count, now / 1000, dryer.IsRunning() ? "YES" : "NO");
  }

  if (now - last_memory_check >= kMemoryCheckInterval)
  {
    last_memory_check = now;
    uint32_t free_heap = rp2040.getFreeHeap();
    uint32_t total_heap = rp2040.getTotalHeap();
    Logger::Info("Memory: free=%u/%u bytes (%u%%)",
                 free_heap, total_heap, (free_heap * 100) / total_heap);
    if (free_heap < total_heap / 10)
    {
      Logger::Warning("Low memory — less than 10%% free");
    }
  }
}

// ========== SETUP ==========

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Logger::Init(LOG_LEVEL_VERBOSE);

  Logger::Info("========================================");
  Logger::Info("    Dryer Controller v4 — startup");
  Logger::Info("========================================");

  if (watchdog_caused_reboot())
  {
    Logger::Warning("!!! Recovered from watchdog reset !!!");
  }

  // Enable 8-second watchdog
  watchdog_enable(8000, 1);
  Logger::Info("Watchdog enabled (8 s timeout)");

  SetupOutputs();

  SetupI2C();
  delay(100);

  SetupRTC();
  delay(50);

  dryer.Begin();
  input_handler.Begin();

  // Sync the last_* tracking variables in UpdateOutputs() with the pin levels
  // established by SetupOutputs().
  UpdateOutputs();

  Logger::Info("Setup complete");
  g_core0_ready = true; // Signal Core 1 to start Modbus initialization
}

// ========== LOOP ==========

void loop()
{
  watchdog_update();

  loop_count++;

  UpdateSensors();
  UpdateInputs();
  dryer.Update();
  UpdateOutputs();
  UpdateDiagnostics();

  delay(10);
}
