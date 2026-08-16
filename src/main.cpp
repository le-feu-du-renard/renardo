#include <Arduino.h>
#include <Wire.h>
#include <hardware/watchdog.h>

#include "config.h"
#include "Dryer.h"
#include "Rs485Bus.h"
#include "ModbusSensors.h"
#include "HydraulicRemote.h"
#include "SharedSensorState.h"
#include "OutputDriver.h"
#include "SettingsStore.h"
#include "TftDisplay.h"
#include "MenuSystem.h"
#include "MenuRenderer.h"
#include "LoraLink.h"
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
OutputDriver fan_output(OUT_FAN_PIN, OUT_FAN_ACTIVE_LOW, "fan");
OutputDriver damper_output(OUT_DAMPER_PIN, OUT_DAMPER_ACTIVE_LOW, "damper");
OutputDriver electric_output(OUT_ELECTRIC_PIN, OUT_ELECTRIC_ACTIVE_LOW, "electric");
InputHandler input_handler;
TftDisplay display;
MenuSystem menu;
LoraLink lora;

// Declared by MenuSystem.cpp so the ECO entries can grey themselves out.
void MenuSetRtcAvailable(bool available);

// RTC — optional module; absence disables ECO mode
TimeManager time_manager(&i2c_bus_1);
static bool g_rtc_available = false;

// Persistence on internal flash
SettingsStore settings_store;
DryerSettings settings;

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
static uint32_t last_session_save = 0;
static bool     was_running = false;

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
  fan_output.Begin();
  damper_output.Begin();
  electric_output.Begin();
}

static void SetupAnalogInputs()
{
  analogReadResolution(12);
  pinMode(DAMPER_FEEDBACK_PIN, INPUT);
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

  // The button is read before the menu and acts whatever is on screen: it is
  // the safety control, not a menu entry. One press toggles the session.
  if (input_handler.IsButtonPressed())
  {
    if (dryer.IsRunning())
    {
      Logger::Info("Button pressed — stopping session");
      dryer.Stop();
    }
    else
    {
      Logger::Info("Button pressed — starting session");
      dryer.Start();
    }
  }

  int32_t detents = input_handler.ConsumeEncoderDelta();
  bool    clicked = input_handler.IsEncoderClicked();

  if (menu.IsOpen())
  {
    menu.HandleRotation(detents);
    if (clicked)
    {
      menu.HandleClick();
    }
  }
  else if (clicked)
  {
    menu.Open();
  }
}

// ========== OUTPUT UPDATE ==========

static void UpdateOutputs()
{
  electric_output.Set(dryer.GetHeaterOutput() > 0.5f);
  fan_output.Set(dryer.GetFanOutput() > 0.0f);
  damper_output.Set(dryer.GetDamperOutput());

  // Hand the hydraulic on/off request to the core that owns the RS485 bus.
  g_hydraulic_request = dryer.GetHydraulicOn();
}

// ========== DAMPER POSITION FEEDBACK ==========

static uint32_t last_damper_sample = 0;

static void UpdateDamperPosition()
{
  uint32_t now = millis();
  if (now - last_damper_sample < DAMPER_SAMPLE_INTERVAL)
    return;
  last_damper_sample = now;

  // Average a few samples: the RP2040 ADC is noisy and this only feeds a
  // display, so a slow, smooth value is what we want.
  constexpr uint8_t kSamples = 8;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < kSamples; i++)
  {
    sum += analogRead(DAMPER_FEEDBACK_PIN);
  }
  dryer.GetAirDamper()->SetRawPosition(static_cast<uint16_t>(sum / kSamples));
}

// ========== DISPLAY ==========

static uint32_t last_display_update = 0;

// Applied whenever the menu commits a value: push the record into the live
// managers, then persist it.
static void OnSettingsChanged()
{
  dryer.ApplySettings(settings, g_rtc_available);
  g_water_target = settings.water_target;
  settings_store.SaveSettings(settings);
}

static void UpdateDisplay()
{
  uint32_t now = millis();
  if (now - last_display_update < DISPLAY_UPDATE_INTERVAL)
    return;
  last_display_update = now;

  // The menu owns the whole panel while it is open; the main screen has to be
  // fully repainted once it hands the panel back.
  static bool menu_was_open = false;
  if (menu.IsOpen())
  {
    menu_was_open = true;
    MenuRenderer::Render(display, menu);
    return;
  }
  if (menu_was_open)
  {
    menu_was_open = false;
    display.Invalidate();
  }

  const TemperatureManager *temperature_manager = dryer.GetTemperatureManager();
  const AirDamper *damper = dryer.GetAirDamper();

  DisplayModel model;
  model.total_elapsed_s = dryer.IsRunning() ? dryer.GetTotalElapsedTime() : 0;
  model.phase_name      = dryer.GetPhaseName();
  model.running         = dryer.IsRunning();
  model.lora_linked     = lora.IsLinked();

  model.inlet_temperature  = g_sensors.inlet_temperature;
  model.inlet_humidity     = g_sensors.inlet_humidity;
  model.target_temperature = temperature_manager->GetEffectiveTargetTemperature();
  model.target_humidity    = dryer.GetHumidityManager()->GetTargetHumidity();

  model.hydraulic_online  = temperature_manager->GetHydraulicOnline();
  model.hydraulic_enabled = temperature_manager->GetHydraulicEnabled();
  model.hydraulic_on      = temperature_manager->GetHydraulicOn();
  model.water_temperature = g_sensors.water_temperature;
  model.tank_temperature  = g_sensors.tank_temperature;

  model.fan_on            = dryer.GetFanOutput() > 0.0f && dryer.IsRunning();
  model.fan_cooling       = !dryer.IsRunning() && dryer.GetFanOutput() > 0.0f;
  model.electric_on       = temperature_manager->GetElectricOn();
  model.electric_enabled  = temperature_manager->GetElectricEnabled();
  model.damper_open       = damper->IsOpen();
  model.damper_position   = damper->GetPositionPercent();
  model.damper_moving     = damper->IsMoving();

  model.sensor_fault = !temperature_manager->GetHeatingPermitted();

  display.RenderMain(model);
}

// ========== LORA LINK ==========

// Commands arriving from the Commander. START/STOP are applied exactly like a
// button press; setpoint changes go through the same record the menu edits, so
// a remote change is persisted and reflected on screen like any other.
static void ApplyRemoteCommands()
{
  uint8_t command = kLoraCommandNone;
  float   argument = 0.0f;

  while (lora.ConsumeCommand(command, argument))
  {
    switch (command)
    {
    case kLoraCommandStart:
      if (!dryer.IsRunning())
      {
        Logger::Info("LoRa: remote START");
        dryer.Start();
      }
      break;

    case kLoraCommandStop:
      if (dryer.IsRunning())
      {
        Logger::Info("LoRa: remote STOP");
        dryer.Stop();
      }
      break;

    case kLoraCommandSetTemp:
      Logger::Info("LoRa: remote target %F C", argument);
      settings.target_temperature = argument;
      OnSettingsChanged();
      break;

    case kLoraCommandSetHumidity:
      Logger::Info("LoRa: remote target %F %%RH", argument);
      settings.target_humidity = argument;
      OnSettingsChanged();
      break;

    default:
      break;
    }
  }
}

static void UpdateLora()
{
  const TemperatureManager *temperature_manager = dryer.GetTemperatureManager();
  const AirDamper *damper = dryer.GetAirDamper();

  TelemetryData data;
  data.inlet_temperature  = g_sensors.inlet_temperature;
  data.inlet_humidity     = g_sensors.inlet_humidity;
  data.outlet_temperature = g_sensors.outlet_temperature;
  data.outlet_humidity    = g_sensors.outlet_humidity;
  data.water_temperature  = g_sensors.water_temperature;
  data.tank_temperature   = g_sensors.tank_temperature;
  data.target_temperature = temperature_manager->GetEffectiveTargetTemperature();
  data.target_humidity    = dryer.GetHumidityManager()->GetTargetHumidity();
  data.damper_position    = damper->GetPositionPercent();

  data.session_elapsed_s = dryer.IsRunning() ? dryer.GetTotalElapsedTime() : 0;
  data.phase             = static_cast<uint8_t>(dryer.GetCurrentPhase());

  data.running          = dryer.IsRunning();
  data.fan_on           = dryer.GetFanOutput() > 0.0f;
  data.electric_on      = temperature_manager->GetElectricOn();
  data.hydraulic_on     = temperature_manager->GetHydraulicOn();
  data.hydraulic_online = temperature_manager->GetHydraulicOnline();
  data.damper_open      = damper->IsOpen();
  data.sensor_fault     = !temperature_manager->GetHeatingPermitted();

  lora.Update(data, settings.lora_telemetry_interval_ms);
  ApplyRemoteCommands();
}

// ========== SESSION PERSISTENCE ==========

static void SaveSessionNow()
{
  SessionSnapshot snapshot;
  snapshot.Reset();
  dryer.CaptureSession(snapshot);
  settings_store.SaveSession(snapshot);
}

static void UpdateSessionPersistence()
{
  bool is_running = dryer.IsRunning();

  // Persist immediately on both edges so a power cut right after START or STOP
  // does not resume the wrong state.
  if (is_running != was_running)
  {
    was_running = is_running;
    SaveSessionNow();
    last_session_save = millis();
    return;
  }

  if (!is_running)
  {
    return;
  }

  uint32_t now = millis();
  if (now - last_session_save < SETTINGS_SAVE_INTERVAL)
    return;
  last_session_save = now;
  SaveSessionNow();
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
  SetupAnalogInputs();

  SetupI2C();
  delay(100);

  SetupRTC();
  delay(50);

  dryer.Begin();
  input_handler.Begin();
  display.Begin();

  // Settings must be applied before any session is restored, so the restored
  // cycle runs with the phase durations the user actually configured.
  settings_store.Begin();
  settings_store.LoadSettings(settings);
  dryer.ApplySettings(settings, g_rtc_available);
  g_water_target = settings.water_target;

  // The device id lets the Commander tell several dryers apart and makes a
  // frame meant for another one impossible to obey.
  lora.Begin(LORA_DEVICE_ID);

  MenuSetRtcAvailable(g_rtc_available);
  menu.Begin(&settings);
  menu.SetOnChange(OnSettingsChanged);

  SessionSnapshot session;
  if (settings_store.LoadSession(session) && session.running)
  {
    dryer.RestoreSession(static_cast<DryerPhase>(session.phase),
                         session.phase_elapsed_s, session.total_elapsed_s);
  }
  else
  {
    Logger::Info("No session to restore");
  }
  was_running = dryer.IsRunning();
  last_session_save = millis();

  // Sync the last_* tracking variables in UpdateOutputs() with the pin levels
  // established by SetupOutputs().
  UpdateOutputs();

  Logger::Info("Setup complete — running=%s", was_running ? "YES" : "NO");
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
  UpdateDamperPosition();
  UpdateDisplay();
  UpdateLora();
  UpdateSessionPersistence();
  UpdateDiagnostics();

  delay(10);
}
