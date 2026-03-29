#include <Arduino.h>
#include <Wire.h>
#include <hardware/watchdog.h>

#include "config.h"
#include "Dryer.h"
#include "ModbusSensors.h"
#include "IndicatorLEDs.h"
#include "VoltmeterOutputs.h"
#include "InputHandler.h"
#include "TimeManager.h"
#include "SessionMonitor.h"
#include "Logger.h"

// ========== GLOBAL OBJECTS ==========

// I2C bus (shared by MCP23017 and RTC DS1307)
TwoWire i2c_bus_1(i2c1, I2C_BUS_1_SDA_PIN, I2C_BUS_1_SCL_PIN);

// RS485 Modbus sensors
ModbusSensors modbus_sensors;

// Physical I/O
IndicatorLEDs indicator_leds;
VoltmeterOutputs voltmeters;
InputHandler input_handler;

// RTC
TimeManager time_manager(&i2c_bus_1);

// Main controller
Dryer dryer;

// Session data logger
SessionMonitor session_monitor(&dryer, &time_manager);

// ========== TIMING STATE ==========

static uint32_t last_sensor_update = 0;
static uint32_t last_input_update = 0;
static uint32_t last_settings_save = 0;
static bool was_running = false;

static constexpr uint32_t kMemoryCheckInterval = 30000; // 30 s
static constexpr uint32_t kHeartbeatInterval = 10000;   // 10 s
static uint32_t last_memory_check = 0;
static uint32_t last_heartbeat = 0;
static uint32_t loop_count = 0;

// ========== SETUP HELPERS ==========

static void SetupI2C()
{
  // Single I2C bus – MCP23017 LED expander + RTC DS1307
  pinMode(I2C_BUS_1_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_BUS_1_SCL_PIN, INPUT_PULLUP);
  i2c_bus_1.begin();
  i2c_bus_1.setClock(100000);
  i2c_bus_1.setTimeout(1000);
  Logger::Info("I2C bus ready (MCP23017 + RTC, 100kHz)");
}

static void SetupPins()
{
  // Hydraulic circulator PWM
  pinMode(WATER_CIRCULATOR_PWM_PIN, OUTPUT);
  analogWrite(WATER_CIRCULATOR_PWM_PIN, 0);
}

static void SetupLEDs()
{
  if (!indicator_leds.Begin(LED_EXPANDER_ADDRESS, i2c_bus_1))
  {
    Logger::Error("Indicator LEDs: MCP23017 not found — continuing without LEDs");
  }
}

static void SetupRTC()
{
  if (!time_manager.Begin())
  {
    Logger::Error("RTC initialization failed");
  }
  else
  {
    if (time_manager.HasLostPower())
    {
      Logger::Warning("RTC lost power — time may be incorrect");
    }
    Logger::Info("RTC ready: %s", time_manager.GetDateTimeString());
  }
}

static void StartupSelfTest()
{
  Logger::Info("Startup self-test: all outputs ON for 2 s");

  // All Port A indicator LEDs on
  indicator_leds.UpdateAll(0xFF);

  // Button LEDs on Port B
  indicator_leds.SetOutput(MCP_BTN_START_LED_PIN, true);
  indicator_leds.SetOutput(MCP_BTN_STOP_LED_PIN, true);

  // All voltmeters at full scale
  voltmeters.SetTemperature(VOLTMETER_TEMP_MAX);
  voltmeters.SetHumidity(VOLTMETER_HUM_MAX);
  voltmeters.SetTotalDuration(VOLTMETER_TOTAL_DUR_H * 3600.0f);
  voltmeters.SetPhaseDuration(VOLTMETER_PHASE_DUR_MIN * 60.0f);

  delay(2000);

  // Reset all outputs
  indicator_leds.Clear();
  indicator_leds.SetOutput(MCP_BTN_START_LED_PIN, false);
  indicator_leds.SetOutput(MCP_BTN_STOP_LED_PIN, false);
  voltmeters.SetTemperature(0.0f);
  voltmeters.SetHumidity(0.0f);
  voltmeters.SetTotalDuration(0.0f);
  voltmeters.SetPhaseDuration(0.0f);
}

static void SetupSessionMonitor()
{
  if (!session_monitor.Begin())
  {
    Logger::Warning("Session monitor: SD card not available, logging disabled");
  }
  else
  {
    Logger::Info("Session monitor ready");
  }
}

// ========== SENSOR UPDATE ==========

static void UpdateSensors()
{
  uint32_t now = millis();
  if (now - last_sensor_update < SENSOR_UPDATE_INTERVAL)
    return;
  last_sensor_update = now;

  float inlet_temp = dryer.GetInletTemperature();
  float inlet_hum = dryer.GetInletHumidity();

  if (modbus_sensors.ReadSensor(MODBUS_INLET_ADDRESS, inlet_temp, inlet_hum))
  {
    dryer.SetInletTemperature(inlet_temp);
    dryer.SetInletHumidity(inlet_hum);
    Logger::Debug("Inlet: %F C %F%%RH", inlet_temp, inlet_hum);
  }
  else
  {
    Logger::Warning("Inlet sensor read failed (errors=%d)", modbus_sensors.GetErrorCount());
  }

  delay(50); // Allow RS485 bus to settle between back-to-back requests

  float outlet_temp = dryer.GetOutletTemperature();
  float outlet_hum = dryer.GetOutletHumidity();

  if (modbus_sensors.ReadSensor(MODBUS_OUTLET_ADDRESS, outlet_temp, outlet_hum))
  {
    dryer.SetOutletTemperature(outlet_temp);
    dryer.SetOutletHumidity(outlet_hum);
    Logger::Debug("Outlet: %F C %F%%RH", outlet_temp, outlet_hum);
  }
  else
  {
    Logger::Warning("Outlet sensor read failed (errors=%d)", modbus_sensors.GetErrorCount());
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

  // Push potentiometer readings to dryer
  dryer.SetTargetTemperature(input_handler.GetTargetTemperature());

  // Push mode selector to dryer
  OperatingMode mode = input_handler.IsEcoMode()
                           ? OperatingMode::ECO
                           : OperatingMode::PERFORMANCE;
  dryer.SetOperatingMode(mode);

  // Handle START button
  if (input_handler.IsStartPressed() && !dryer.IsRunning())
  {
    Logger::Info("START button pressed — starting session");
    dryer.Start();
  }

  // Handle STOP button
  if (input_handler.IsStopPressed() && dryer.IsRunning())
  {
    Logger::Info("STOP button pressed — stopping session");
    dryer.Stop();
  }

  // Update button LEDs: START LED = running, STOP LED = not running
  input_handler.SetStartLed(dryer.IsRunning());
  input_handler.SetStopLed(!dryer.IsRunning());
}

// ========== OUTPUT UPDATE ==========

static void UpdateOutputs()
{
  static bool last_heater = false;
  static bool last_fan = false;
  static uint8_t last_pwm = 0;
  static bool first_run = true;

  bool heater_state = dryer.GetHeaterOutput() > 0.5f;
  bool fan_state = dryer.GetFanOutput() > 0.0f;
  // GetCirculatorOutput() returns a mapped duty (0.0-1.0); invert for PNP transistor
  uint8_t pwm_val = static_cast<uint8_t>((1.0f - dryer.GetCirculatorOutput()) * 255.0f);

  if (first_run || heater_state != last_heater)
  {
    indicator_leds.SetOutput(MCP_ELECTRIC_HEATER_PIN, heater_state);
    last_heater = heater_state;
    Logger::Info("Electric heater: %s", heater_state ? "ON" : "OFF");
  }

  if (first_run || fan_state != last_fan)
  {
    indicator_leds.SetOutput(MCP_FAN_PIN, fan_state);
    last_fan = fan_state;
    Logger::Info("Fan: %s", fan_state ? "ON" : "OFF");
  }

  if (first_run || abs((int)pwm_val - (int)last_pwm) > 1)
  {
    analogWrite(WATER_CIRCULATOR_PWM_PIN, pwm_val);
    last_pwm = pwm_val;
    Logger::Debug("Circulator PWM: %d/255 (%F%%)",
                  pwm_val, dryer.GetCirculatorOutput() * 100.0f);
  }

  first_run = false;
}

// ========== INDICATOR LED UPDATE ==========

static void UpdateLEDs()
{
  bool running = dryer.IsRunning();
  bool eco = input_handler.IsEcoMode();
  DryerPhase phase = dryer.GetCurrentPhase();

  uint8_t mask = 0;
  if (eco)
    mask |= (1 << (uint8_t)LedId::kEcoMode);
  if (running && phase == DryerPhase::kInit)
    mask |= (1 << (uint8_t)LedId::kPhaseInit);
  if (running && phase == DryerPhase::kBrassage)
    mask |= (1 << (uint8_t)LedId::kPhaseBrassage);
  if (running && phase == DryerPhase::kExtraction)
    mask |= (1 << (uint8_t)LedId::kPhaseExtraction);
  if (dryer.GetHeaterOutput() > 0.5f)
    mask |= (1 << (uint8_t)LedId::kElectricHeater);
  if (dryer.GetCirculatorOutput() > 0.05f)
    mask |= (1 << (uint8_t)LedId::kHydroHeater);
  if (dryer.GetFanOutput() > 0.0f)
    mask |= (1 << (uint8_t)LedId::kFan);

  indicator_leds.UpdateAll(mask);
}

// ========== VOLTMETER UPDATE ==========

static void UpdateVoltmeters()
{
  voltmeters.SetTemperature(dryer.GetInletTemperature());
  voltmeters.SetHumidity(dryer.GetInletHumidity());
  voltmeters.SetTotalDuration(static_cast<float>(dryer.GetTotalElapsedTime()));
  voltmeters.SetPhaseDuration(static_cast<float>(dryer.GetPhaseElapsedTime()));
}

// ========== SESSION MONITOR UPDATE ==========

static void UpdateSessionMonitor()
{
  bool is_running = dryer.IsRunning();

  if (is_running && !was_running)
  {
    Logger::Info("Dryer started — starting session logging");
    session_monitor.StartSession();
  }
  else if (!is_running && was_running)
  {
    Logger::Info("Dryer stopped — ending session logging");
    session_monitor.StopSession();
    dryer.SaveSettings();
  }

  was_running = is_running;
  session_monitor.Update();

  if (!session_monitor.IsReady())
  {
    session_monitor.RetryInitialization();
  }
}

// ========== SETTINGS SAVE ==========

static void UpdateSettings()
{
  uint32_t now = millis();
  if (!dryer.IsRunning())
    return;
  if (now - last_settings_save < SETTINGS_SAVE_INTERVAL)
    return;
  last_settings_save = now;
  dryer.SaveSettings();
  Logger::Debug("Settings auto-saved");
}

// ========== DIAGNOSTICS ==========

static void UpdateDiagnostics()
{
  uint32_t now = millis();

  if (now - last_heartbeat >= kHeartbeatInterval)
  {
    last_heartbeat = now;
    Logger::Info("Heartbeat — loops=%lu uptime=%us SD=%s logging=%s",
                 loop_count, now / 1000,
                 session_monitor.IsReady() ? "OK" : "NOK",
                 session_monitor.IsLogging() ? "YES" : "NO");
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
  Logger::Info("    Dryer Controller — startup");
  Logger::Info("========================================");

  if (watchdog_caused_reboot())
  {
    Logger::Warning("!!! Recovered from watchdog reset !!!");
  }

  // Enable 8-second watchdog
  watchdog_enable(8000, 1);
  Logger::Info("Watchdog enabled (8 s timeout)");

  SetupI2C();
  delay(100);

  SetupLEDs();
  delay(50);

  SetupRTC();
  delay(50);

  SetupPins();
  delay(50);

  voltmeters.Begin();
  input_handler.Begin(indicator_leds);

  StartupSelfTest();

  modbus_sensors.Begin(MODBUS_BAUDRATE);
  delay(50);

  SetupSessionMonitor();

  dryer.Begin(indicator_leds);

  was_running = dryer.IsRunning();
  if (was_running && session_monitor.IsReady())
  {
    session_monitor.StartSession();
  }

  Logger::Info("Setup complete — running=%s", was_running ? "YES" : "NO");
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
  UpdateLEDs();
  UpdateVoltmeters();
  UpdateSessionMonitor();
  UpdateSettings();
  UpdateDiagnostics();

  delay(10);
}
