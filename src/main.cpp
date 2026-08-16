#include <Arduino.h>
#include <Wire.h>
#include <hardware/watchdog.h>

#include "config.h"
#include "Dryer.h"
#include "ModbusSensors.h"
#include "InputHandler.h"
#include "TimeManager.h"
#include "Logger.h"

// ========== GLOBAL OBJECTS ==========

// I2C bus (optional RTC DS1307)
TwoWire i2c_bus_1(i2c1, I2C_BUS_1_SDA_PIN, I2C_BUS_1_SCL_PIN);

// RS485 bus A — sensors + hydraulic module (Core 1)
ModbusSensors modbus_sensors;

// Physical I/O
InputHandler input_handler;

// RTC — optional module; absence disables ECO mode
TimeManager time_manager(&i2c_bus_1);
static bool g_rtc_available = false;

// Main controller
Dryer dryer;

// ========== CORE 1 ==========

// Core 1 owns RS485 bus A exclusively.
// Core 0 reads these volatile variables without blocking.
// TODO(v4): replace with a seqlock-protected struct carrying freshness timestamps.

static volatile float g_inlet_temp = 0.0f;
static volatile float g_inlet_hum = 0.0f;
static volatile float g_outlet_temp = 0.0f;
static volatile float g_outlet_hum = 0.0f;
static volatile bool g_core0_ready = false;

void setup1()
{
  while (!g_core0_ready)
  {
  } // Wait for Core 0 to finish setup
  modbus_sensors.Begin(MODBUS_BAUDRATE);
}

void loop1()
{
  float temp, hum;

  temp = g_inlet_temp;
  hum = g_inlet_hum;
  if (modbus_sensors.ReadSensor(MODBUS_INLET_ADDRESS, temp, hum))
  {
    g_inlet_temp = temp;
    g_inlet_hum = hum;
  }

  delay(50); // RS485 bus settle between requests

  temp = g_outlet_temp;
  hum = g_outlet_hum;
  if (modbus_sensors.ReadSensor(MODBUS_OUTLET_ADDRESS, temp, hum))
  {
    g_outlet_temp = temp;
    g_outlet_hum = hum;
  }

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

static void SetupOutputs()
{
  // TODO(v4): move to OutputDriver. Drive the 2N2222 stages to their idle level
  // before switching them to OUTPUT so no load is energised at boot.
  const uint8_t idle = OUTPUTS_ACTIVE_LOW ? HIGH : LOW;
  digitalWrite(OUT_FAN_PIN, idle);
  digitalWrite(OUT_DAMPER_PIN, idle);
  digitalWrite(OUT_ELECTRIC_PIN, idle);
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

static void UpdateSensors()
{
  uint32_t now = millis();

  dryer.SetInletTemperature(g_inlet_temp);
  dryer.SetInletHumidity(g_inlet_hum);
  dryer.SetOutletTemperature(g_outlet_temp);
  dryer.SetOutletHumidity(g_outlet_hum);

  if (now - last_sensor_log >= SENSOR_UPDATE_INTERVAL)
  {
    last_sensor_log = now;
    Logger::Debug("Inlet:  %F C  %F%%RH", (float)g_inlet_temp, (float)g_inlet_hum);
    Logger::Debug("Outlet: %F C  %F%%RH", (float)g_outlet_temp, (float)g_outlet_hum);
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

static void WriteOutput(uint8_t pin, bool active)
{
  digitalWrite(pin, (active != OUTPUTS_ACTIVE_LOW) ? HIGH : LOW);
}

static void UpdateOutputs()
{
  static bool last_heater = false;
  static bool last_fan = false;
  static bool last_damper = false;

  bool heater_state = dryer.GetHeaterOutput() > 0.5f;
  bool fan_state = dryer.GetFanOutput() > 0.0f;
  bool damper_state = dryer.GetDamperOutput();

  if (heater_state != last_heater)
  {
    WriteOutput(OUT_ELECTRIC_PIN, heater_state);
    last_heater = heater_state;
    Logger::Info("Electric heater: %s", heater_state ? "ON" : "OFF");
  }

  if (fan_state != last_fan)
  {
    WriteOutput(OUT_FAN_PIN, fan_state);
    last_fan = fan_state;
    Logger::Info("Fan: %s", fan_state ? "ON" : "OFF");
  }

  if (damper_state != last_damper)
  {
    WriteOutput(OUT_DAMPER_PIN, damper_state);
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
