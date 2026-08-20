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
#include "StatusIndicator.h"
#include "StatusLed.h"
#include "SettingsStore.h"
#include "TftDisplay.h"
#include "MenuSystem.h"
#include "MenuRenderer.h"
#include "InputHandler.h"
#include "TimeManager.h"
#include "Logger.h"

// ========== GLOBAL OBJECTS ==========

// I2C bus (optional RTC DS1307). On i2c0 rather than i2c1 since the two damper
// feedbacks took GP26/GP27, the only ADC-capable pins the Pico brings out.
TwoWire rtc_i2c(i2c0, RTC_I2C_SDA_PIN, RTC_I2C_SCL_PIN);

// RS485 — single Modbus bus (inlet probe @1 + hydraulic module @10),
// owned exclusively by Core 1
Rs485Bus rs485(Serial2, RS485_TX_PIN, RS485_RX_PIN, RS485_DE_PIN, "rs485");
ModbusSensors modbus_sensors(&rs485);
HydraulicRemote hydraulic_remote(&rs485);

// Physical I/O
OutputDriver fan_output(OUT_FAN_PIN, OUT_FAN_ACTIVE_LOW, "fan");
OutputDriver damper_output(OUT_DAMPER_PIN, OUT_DAMPER_ACTIVE_LOW, "damper");
OutputDriver electric_output(OUT_ELECTRIC_PIN, OUT_ELECTRIC_ACTIVE_LOW, "electric");
StatusLed status_led(LED_RUN_PIN, LED_FAULT_PIN);
InputHandler input_handler;
TftDisplay display;
MenuSystem menu;

// RTC — optional module; absence disables ECO mode
TimeManager time_manager(&rtc_i2c);
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
  modbus_sensors.Poll();

  hydraulic_remote.SetState(g_hydraulic_request);
  hydraulic_remote.SetWaterTarget(g_water_target);
  hydraulic_remote.Update();

  const SensorReading &inlet = modbus_sensors.GetReading();

  SensorSnapshot snapshot;
  snapshot.inlet_temperature  = inlet.temperature;
  snapshot.inlet_humidity     = inlet.humidity;
  snapshot.inlet_updated_ms   = inlet.last_success_ms;
  snapshot.inlet_valid        = inlet.valid;
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

// The RP2040 watchdog counter is 24 bits of half-microseconds, so anything over
// roughly 8.3 s is silently clamped — there is no such thing as a longer window
// to lean on during a slow start-up.
static constexpr uint32_t kRuntimeWatchdogMs = 8000;

static constexpr uint32_t kMemoryCheckInterval = 30000; // 30 s
static constexpr uint32_t kHeartbeatInterval = 10000;   // 10 s
static uint32_t last_memory_check = 0;
static uint32_t last_heartbeat = 0;
static uint32_t loop_count = 0;

// ========== SETUP HELPERS ==========

static void SetupI2C()
{
  pinMode(RTC_I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(RTC_I2C_SCL_PIN, INPUT_PULLUP);
  rtc_i2c.begin();
  rtc_i2c.setClock(10000);
  rtc_i2c.setTimeout(1000);
  Logger::Info("RTC I2C ready on GP%d/GP%d (10kHz)", RTC_I2C_SDA_PIN,
               RTC_I2C_SCL_PIN);
}

static void SetupOutputs()
{
  fan_output.Begin();
  damper_output.Begin();
  electric_output.Begin();

  // The LEDs come up with the command outputs, in the first gesture of setup():
  // both dark, and the panel stays dark until the first UpdateStatusLed().
  status_led.Begin();
}

static void SetupAnalogInputs()
{
  analogReadResolution(12);
  pinMode(DAMPER_EXTRACTION_FEEDBACK_PIN, INPUT);
  // Both pads are configured whatever the register count says: the count is a
  // runtime setting now, and an input left unsampled costs nothing, whereas a
  // pin that has never been through pinMode() the first time the setting changes
  // would read whatever the last function left behind.
  pinMode(DAMPER_RECYCLING_FEEDBACK_PIN, INPUT);
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
  Logger::Info("RTC ready: %s", time_manager.GetDateTimeString().c_str());
}

// --- Clock hooks for the menu ---
//
// The menu edits a staging copy and hands it back once the user validates, so
// the RTC never sees a half-typed date. Passing these as functions keeps
// RTClib out of MenuSystem, which the host tests compile as-is.

static bool ReadRtcClock(MenuClock &clock)
{
  if (!g_rtc_available)
  {
    return false;
  }

  DateTime now = time_manager.GetNow();
  clock.year   = now.year();
  clock.month  = now.month();
  clock.day    = now.day();
  clock.hour   = now.hour();
  clock.minute = now.minute();
  return true;
}

static void WriteRtcClock(const MenuClock &clock)
{
  if (!g_rtc_available)
  {
    return;
  }

  // Seconds restart at zero: the user set the time to the minute, and carrying
  // the old seconds over would only add an unpredictable offset.
  time_manager.SetTime(clock.year, clock.month, clock.day,
                       clock.hour, clock.minute, 0);
  Logger::Info("RTC set from menu: %s", time_manager.GetDateTimeString().c_str());
}

// --- Damper hooks for the menu ---
//
// Same split as the clock: the menu shows live feedback and can move the air
// path, but knows nothing about the ADC or the damper object.

static bool ReadDamperReadback(uint8_t index, MenuDamperReadback &readback)
{
  const AirDamper *damper = dryer.GetAirDamper();
  const DamperFeedback &feedback =
      index == 0 ? damper->Extraction() : damper->Recycling();

  readback.raw        = feedback.GetRawPosition();
  readback.percent    = feedback.GetPositionPercent();
  readback.has_signal = feedback.HasSignal();
  return true;
}

static void CommandDamper(bool extraction)
{
  AirDamper *damper = dryer.GetAirDamper();
  if (extraction)
  {
    damper->Open();
  }
  else
  {
    damper->Close();
  }
  // The relay follows on the next UpdateOutputs(), which runs whether or not a
  // session is going: nothing else has to be poked here.
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
    Logger::Debug("Inlet: %F C  %F%%RH", g_sensors.inlet_temperature, g_sensors.inlet_humidity);
    Logger::Debug("Water: %F C  Tank %F C", g_sensors.water_temperature, g_sensors.tank_temperature);
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

  // The buttons are read before the menu and act whatever is on screen: they
  // are the safety controls, not menu entries.
  //
  // STOP is read first and both are read every pass, so a press on each in the
  // same window ends with the dryer stopped: whatever else is being asked of
  // the machine, the request to stop it is the one that must land. Start() and
  // Stop() both return immediately when the dryer is already in the state being
  // asked for, so a press that changes nothing costs nothing.
  bool stop_pressed  = input_handler.IsStopPressed();
  bool start_pressed = input_handler.IsStartPressed();

  if (stop_pressed)
  {
    Logger::Info("STOP pressed");
    dryer.Stop();
  }
  else if (start_pressed)
  {
    Logger::Info("START pressed");
    dryer.Start();
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

// ========== STATUS LEDS ==========

// Runs every loop rather than on an interval of its own: Apply() only touches a
// pin whose level changes, and the blink needs a finer resolution than the
// display's 100 ms. What is logged is the state changing, once — never the
// blink.
static void UpdateStatusLed()
{
  uint32_t now = millis();

  // Same expression as DisplayModel::fan_cooling below: the fan outliving a
  // stopped session is the cooldown.
  bool fan_active = dryer.GetFanOutput() > 0.0f;

  DryerStatus status =
      ResolveStatus(dryer.IsRunning(), fan_active, dryer.HasFault(), now);

  static DryerStatus last_status = DryerStatus::kStopped;
  static bool        status_seen = false;
  if (!status_seen || status != last_status)
  {
    status_seen = true;
    last_status = status;
    Logger::Info("Status: %s", StatusName(status));
  }

  status_led.Apply(PatternFor(status, now));
}

// ========== DAMPER POSITION FEEDBACK ==========

static uint32_t last_damper_sample = 0;

// Average a few samples: the RP2040 ADC is noisy and this only feeds a
// display, so a slow, smooth value is what we want. The averaging spans a few
// microseconds, so it flattens converter noise and nothing else — mains hum is
// the 100nF's job, at the divider.
static uint16_t ReadDamperFeedback(uint8_t pin)
{
  constexpr uint8_t kSamples = 8;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < kSamples; i++)
  {
    sum += analogRead(pin);
  }
  return static_cast<uint16_t>(sum / kSamples);
}

// Reports an opening that moved further between two samples than the actuator
// can physically travel.
//
// The LM24A-SR takes about 150 s end to end, so 0.33 % per 500 ms sample. A jump
// of tens of percent is therefore not a register moving, it is the reading
// itself breaking down — a feedback wire picking up a switching load, a divider
// losing its ground, or an unwired neighbour channel dragging this one through
// the multiplexed ADC's sample-and-hold. On screen all three look identical: a
// vane flicking between shut and open every second or so, which reads as a
// mechanical fault and is not one.
//
// Named for what it measures rather than what it suspects: the log line gives
// the two raw values, which is what tells those causes apart.
static void CheckPositionPlausibility(const char *name, float previous, float current,
                                      uint16_t previous_raw, uint16_t current_raw)
{
  constexpr float kMaxStepPercent = 20.0f; // ~60x the actuator's real rate

  if (isnan(previous) || isnan(current))
  {
    return;
  }
  if (fabsf(current - previous) <= kMaxStepPercent)
  {
    return;
  }

  Logger::Warning("Damper %s: impossible jump %F%% -> %F%% in %ums "
                  "(raw %u -> %u) — this is the reading, not the vane",
                  name, previous, current, DAMPER_SAMPLE_INTERVAL,
                  previous_raw, current_raw);
}

static void UpdateDamperPosition()
{
  uint32_t now = millis();
  if (now - last_damper_sample < DAMPER_SAMPLE_INTERVAL)
    return;
  last_damper_sample = now;

  AirDamper *damper = dryer.GetAirDamper();

  float    previous_extraction     = damper->Extraction().GetPositionPercent();
  uint16_t previous_extraction_raw = damper->Extraction().GetRawPosition();
  float    previous_recycling      = damper->Recycling().GetPositionPercent();
  uint16_t previous_recycling_raw  = damper->Recycling().GetRawPosition();

  damper->Extraction().SetRawPosition(
      ReadDamperFeedback(DAMPER_EXTRACTION_FEEDBACK_PIN));
  if (damper->GetCount() >= 2)
  {
    damper->Recycling().SetRawPosition(
        ReadDamperFeedback(DAMPER_RECYCLING_FEEDBACK_PIN));
  }

  CheckPositionPlausibility("extraction", previous_extraction,
                            damper->Extraction().GetPositionPercent(),
                            previous_extraction_raw,
                            damper->Extraction().GetRawPosition());
  if (damper->GetCount() >= 2)
  {
    CheckPositionPlausibility("recycling", previous_recycling,
                              damper->Recycling().GetPositionPercent(),
                              previous_recycling_raw,
                              damper->Recycling().GetRawPosition());
  }
  // On a dryer declaring one register, Recycling() never receives a sample, so
  // it reports no position: dashes on screen, the sentinel over the air.
  // Sampling a pin with nothing on it instead would put believable noise there.

  // Times the interlock's confirmation window, so it has to run after the raw
  // values have landed and at the rate they land.
  damper->UpdateInterlock();
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

// DisplayModel::phase carries a DryerPhase, and TftDisplay indexes its table of
// phase names and colours with it. The renderer is deliberately unaware of
// SessionManager — this is the seam where the two meet, so this is where the
// correspondence is pinned down.
static_assert(static_cast<uint8_t>(DryerPhase::kStop) == 0, "phase table order");
static_assert(static_cast<uint8_t>(DryerPhase::kInit) == 1, "phase table order");
static_assert(static_cast<uint8_t>(DryerPhase::kBrassage) == 2, "phase table order");
static_assert(static_cast<uint8_t>(DryerPhase::kExtraction) == 3, "phase table order");

// How far the running phase has gone against its configured duration, 0..1, or
// NAN when nothing is running.
//
// An estimate rather than a countdown: brassage and extraction both end on a
// humidity threshold when the crop dries faster than the clock allows, so the
// bar can reach the end and the phase change before it, or the phase change
// while the bar is part way. It answers "is this phase well along", which is
// what the operator glancing at the screen actually wants to know.
static float PhaseProgress()
{
  SessionManager *session = dryer.GetSessionManager();
  const PhaseDurations &durations = session->GetDurations();

  uint32_t total = 0;
  switch (session->GetCurrentPhase())
  {
    case DryerPhase::kInit:       total = durations.init; break;
    case DryerPhase::kBrassage:   total = durations.brassage; break;
    case DryerPhase::kExtraction: total = durations.extraction; break;
    default:                      return NAN;
  }

  if (total == 0)
  {
    return NAN;
  }

  float ratio = static_cast<float>(session->GetPhaseElapsedTime()) /
                static_cast<float>(total);
  return ratio > 1.0f ? 1.0f : ratio;
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
  model.phase           = static_cast<uint8_t>(dryer.GetSessionManager()->GetCurrentPhase());
  model.phase_progress  = PhaseProgress();
  model.running         = dryer.IsRunning();

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
  model.damper_open          = damper->IsOpen();
  model.extraction_position  = damper->Extraction().GetPositionPercent();
  model.extraction_moving    = damper->Extraction().IsMoving();
  model.recycling_position   = damper->Recycling().GetPositionPercent();
  model.recycling_moving     = damper->Recycling().IsMoving();
  model.damper_count         = damper->GetCount();

  model.sensor_fault          = !temperature_manager->GetHeatingPermitted();
  model.airflow_fault         = dryer.GetAirflowBlocked();
  model.damper_feedback_fault = dryer.GetDamperFeedbackFault();

  display.RenderMain(model);
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
    Logger::Info("Heartbeat — loops=%u uptime=%us running=%s",
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

  // Outputs first, whatever else happens: this drives the three command lines
  // to their inactive level, and nothing below it is allowed to take priority.
  SetupOutputs();
  SetupAnalogInputs();

  // The panel comes up before anything that can block, so the splash covers the
  // whole start-up: it only needs SPI0, and every probe below it takes long
  // enough to be worth announcing. Each stage is written to the splash as well
  // as to the log, so a board that hangs says where on its own screen.
  display.Begin();

  // --- Hardware probing, deliberately outside the watchdog ---
  //
  // Probing hardware that is not fitted blocks for seconds: the I2C bus waits
  // out its own 1 s timeout for an absent RTC, and mounting the filesystem for
  // the first time formats it. The RP2040 watchdog cannot be set beyond ~8.3 s,
  // so there is no window generous enough to cover them — arming it here
  // rebooted the board mid-setup, and since the reboot came back to the same
  // absent hardware, it looped forever.
  //
  // A stage that hangs outright therefore leaves the board stopped rather than
  // cycling. That is the better failure: the log ends on the exact stage that
  // hung instead of scrolling past in a reboot loop.

  display.ShowBootStage("horloge...");
  SetupI2C();
  delay(100);

  SetupRTC();
  delay(50);

  display.ShowBootStage("regulation...");
  dryer.Begin();
  input_handler.Begin();

  // Settings must be applied before any session is restored, so the restored
  // cycle runs with the phase durations the user actually configured.
  display.ShowBootStage("reglages...");
  settings_store.Begin();
  settings_store.LoadSettings(settings);
  dryer.ApplySettings(settings, g_rtc_available);
  g_water_target = settings.water_target;

  // --- Probing done; everything below is bounded and fast ---
  watchdog_enable(kRuntimeWatchdogMs, 1);
  Logger::Info("Watchdog enabled (%u s)", kRuntimeWatchdogMs / 1000);

  MenuSetRtcAvailable(g_rtc_available);
  MenuSetClockHooks(ReadRtcClock, WriteRtcClock);
  MenuSetDamperHooks(ReadDamperReadback, CommandDamper);
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
  // established by SetupOutputs(), and light the panel for the state that was
  // just restored — the splash is still up, and the LEDs are what says whether
  // the board came back into a running session.
  UpdateOutputs();
  UpdateStatusLed();

  Logger::Info("Setup complete — running=%s", was_running ? "YES" : "NO");

  // The first loop() iteration renders the main screen immediately, so the
  // splash hands over to the interface directly. The wait here is bounded by
  // kSplashMinMs, well inside the watchdog window armed above.
  display.ShowBootStage("pret");
  display.EndSplash();

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
  UpdateStatusLed();
  UpdateDamperPosition();
  UpdateDisplay();
  UpdateSessionPersistence();
  UpdateDiagnostics();

  delay(10);
}
