#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RotaryEncoder.h>
#include <Bounce2.h>
#include <hardware/watchdog.h>

#include "config.h"
#include <CHT8305.h>
#include "Display.h"
#include "Dryer.h"
#include "MenuSystem.h"
#include "MenuStructure.h"
#include "TimeManager.h"
#include "SessionMonitor.h"
#include "Logger.h"
#include "SystemStatus.h"

// ========== GLOBAL OBJECTS ==========

// I2C Buses
TwoWire i2c_bus_1(i2c0, I2C_BUS_1_SDA_PIN, I2C_BUS_1_SCL_PIN);
TwoWire i2c_bus_2(i2c1, I2C_BUS_2_SDA_PIN, I2C_BUS_2_SCL_PIN);

// Temperature/humidity sensors
CHT8305 inlet_air_sensor(CHT8305_INLET_ADDR, &i2c_bus_2);
CHT8305 outlet_air_sensor(CHT8305_OUTLET_ADDR, &i2c_bus_1);

// OneWire for water temperature sensor
OneWire one_wire(WATER_SENSOR_PIN);
DallasTemperature water_temperature_sensor(&one_wire);

// DAC
DFRobot_GP8403 dac(&i2c_bus_2, DAC_GP8403_ADDR);

// OLED
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &i2c_bus_1, SCREEN_OLED_RESET);

// RTC
TimeManager time_manager(&i2c_bus_2);

// Rotary Encoder
RotaryEncoder encoder(ROTARY_ENCODER_CLK_PIN, ROTARY_ENCODER_DT_PIN, RotaryEncoder::LatchMode::FOUR3);

// Display
Display display(&oled);

// Dryer controller
Dryer dryer(&dac);

// Menu System
MenuSystem menu(&dryer, &display);

// Session Monitor
SessionMonitor session_monitor(&dryer, &time_manager);

// Debounce objects
Bounce button_encoder = Bounce();

// ========== GLOBAL VARIABLES ==========

// Core 0 variables (sensors, control, outputs, display)
unsigned long last_sensor_update = 0;
unsigned long last_control_update = 0;
unsigned long last_display_update = 0;
unsigned long last_settings_save = 0;
bool settings_need_save = false;

// Data logging variables
bool was_running = false;

// Memory monitoring variables
unsigned long last_memory_check = 0;
static constexpr unsigned long MEMORY_CHECK_INTERVAL = 30000; // 30 seconds

// Loop monitoring variables
unsigned long last_loop_log = 0;
unsigned long loop_count = 0;
static constexpr unsigned long LOOP_LOG_INTERVAL = 10000; // 10 seconds

// Water temperature sensor async variables
unsigned long water_temp_request_time = 0;
bool water_temp_conversion_started = false;
static constexpr unsigned long WATER_TEMP_CONVERSION_TIME = 750; // 750ms for 12-bit resolution

// Core 1 variables (encoder inputs)
int encoder_position = 0;
int last_encoder_position = 0;

// ========== INTERRUPTS ==========

void EncoderISR()
{
  encoder.tick();
}

// ========== CALLBACKS ==========

void OnSettingsChanged()
{
  settings_need_save = true;
  last_settings_save = millis(); // Reset timer for immediate save
  Log.notice("Settings changed, will save immediately");
}

// ========== SETUP FUNCTIONS ==========

void SetupPins()
{
  // Encoder button with debounce
  button_encoder.attach(ROTARY_ENCODER_SW_PIN, INPUT_PULLUP);
  button_encoder.interval(50); // 50ms debounce interval

  // Rotary encoder pins
  pinMode(ROTARY_ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ROTARY_ENCODER_DT_PIN, INPUT_PULLUP);

  // Attach encoder interrupts
  attachInterrupt(digitalPinToInterrupt(ROTARY_ENCODER_CLK_PIN), EncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ROTARY_ENCODER_DT_PIN), EncoderISR, CHANGE);

  // Relays
  pinMode(ELECTRIC_HEATER_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);

  // PWM
  pinMode(WATER_CIRCULATOR_PWM_PIN, OUTPUT);

  // Initial output state
  digitalWrite(ELECTRIC_HEATER_RELAY_PIN, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
}

void ScanI2C(TwoWire &bus, const char *bus_name)
{
  Log.notice("Scanning %s...", bus_name);

  int count = 0;
  for (byte addr = 1; addr < 127; addr++)
  {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0)
    {
      Log.notice("  Found device at 0x%X", addr);
      count++;
    }
  }

  if (count == 0)
  {
    Log.notice("  No devices found");
  }
}

void SetupI2C()
{
  // Bus 1
  // Enable pull-ups for stability
  pinMode(I2C_BUS_1_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_BUS_1_SCL_PIN, INPUT_PULLUP);
  i2c_bus_1.begin();
  i2c_bus_1.setClock(50000);
  i2c_bus_1.setTimeout(1000); // 1 second timeout to prevent I2C hangs
  Log.notice("I2C bus 1 initialized at 50kHz with 1s timeout");

  // Bus 2
  // Enable pull-ups for stability
  pinMode(I2C_BUS_2_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_BUS_2_SCL_PIN, INPUT_PULLUP);
  i2c_bus_2.begin();
  i2c_bus_2.setClock(100000); // 100kHz for stability with 3 devices
  i2c_bus_2.setTimeout(1000); // 1 second timeout to prevent I2C hangs
  Log.notice("I2C bus 2 initialized at 100kHz with internal pull-ups and 1s timeout");

  // Scan all I2C buses
  ScanI2C(i2c_bus_1, "i2c_bus_1");
  ScanI2C(i2c_bus_2, "i2c_bus_2");
}

void SetupOLED()
{
  Log.notice("Initialize OLED...");

  // Initialize display
  if (!oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_SSD1306_ADDR))
  {
    Log.error("OLED initialization failed!");
  }
  else
  {
    Log.notice("OLED initialized successfully!");
    oled.clearDisplay();
    oled.display();
    Log.trace("OLED cleared");
  }

  oled.setRotation(0); // 0=0°, 1=90°, 2=180°, 3=270°
  Log.trace("OLED rotation set to 180°");
}

void SetupDAC()
{
  Log.notice("Initialize DAC...");

  if (dac.begin() != 0)
  {
    Log.error("Failed to initialize GP8403 DAC");
  }
  else
  {
    dac.setDACOutRange(DFRobot_GP8403::eOutputRange10V);
    Log.notice("GP8403 DAC initialized with 0-10V range");
  }
}

void SetupSensors()
{
  // Add delay to let I2C bus stabilize after OLED/RTC init
  delay(100);

  // CHT8305 sensors
  Log.notice("Testing Inlet sensor connection...");

  // Try to clear any I2C bus issues
  i2c_bus_2.beginTransmission(CHT8305_INLET_ADDR);
  i2c_bus_2.endTransmission();
  delay(50);

  int result = inlet_air_sensor.begin();
  if (result != 0)
  {
    Log.error("Inlet air sensor not found! Error code: %d", result);
  }
  else if (!inlet_air_sensor.isConnected())
  {
    Log.error("Inlet air sensor not connected!");
  }
  else
  {
    Log.notice("Inlet air sensor initialized successfully!");
  }

  // Clear I2C bus 1 before outlet sensor init
  Log.notice("Testing Outlet sensor connection...");
  i2c_bus_1.beginTransmission(CHT8305_OUTLET_ADDR);
  i2c_bus_1.endTransmission();
  delay(50);

  int outlet_result = outlet_air_sensor.begin();
  if (outlet_result != 0)
  {
    Log.error("Outlet air sensor not found! Error code: %d", outlet_result);
  }
  else if (!outlet_air_sensor.isConnected())
  {
    Log.error("Outlet air sensor not connected!");
  }
  else
  {
    Log.notice("Outlet air sensor initialized successfully!");
  }

  // Water temperature sensor
  water_temperature_sensor.begin();
  water_temperature_sensor.setWaitForConversion(false); // Async mode
  Log.notice("Found %d OneWire devices (async mode enabled)", water_temperature_sensor.getDeviceCount());
}

void SetupRTC()
{
  Log.notice("Initialize RTC...");

  if (!time_manager.Begin())
  {
    Log.error("RTC initialization failed!");
  }
  else
  {
    Log.notice("RTC initialized successfully!");

    // Check if time needs to be set
    if (time_manager.HasLostPower())
    {
      Log.warning("Please set the RTC time");
      Log.warning("RTC has been set to compile time as default");
    }

    // Display current time
    Log.notice("Current time: %s", time_manager.GetDateTimeString());
  }
}

void SetupSessionMonitor()
{
  Log.notice("Initialize Session Monitor...");
  Log.notice("Note: This may take a few seconds if SD card is not present");

  if (!session_monitor.Begin())
  {
    Log.error("Session Monitor initialization failed!");
    Log.warning("System will continue without session monitoring");
  }
  else
  {
    Log.notice("Session Monitor initialized successfully!");
  }

  Log.notice("Session Monitor setup complete");
}

void SetupDryer()
{
  Log.notice("Initialize Dryer...");

  dryer.Begin();
  dryer.SetSettingsChangedCallback(OnSettingsChanged);
  menu.Begin(MenuStructure::BuildMenu());

  Log.notice("Dryer initialized");
}

void SetupSystemStatus()
{
  Log.notice("Initialize SystemStatus for menu display...");

  SystemStatus::SetTimeManager(&time_manager);
  SystemStatus::SetDisplay(&display);
  SystemStatus::SetInletSensor(&inlet_air_sensor);
  SystemStatus::SetOutletSensor(&outlet_air_sensor);
  SystemStatus::SetWaterSensor(&water_temperature_sensor);
  SystemStatus::SetDAC(&dac);
  SystemStatus::SetSDAvailable(session_monitor.IsReady());
  SystemStatus::SetSessionMonitorReady(session_monitor.IsReady());

  Log.notice("SystemStatus initialized");
}

// ========== MAIN SETUP (Core 0) ==========

void setup()
{
  Serial.begin(115200);
  delay(2000);

  // Initialize logger first
  Logger::Init(LOG_LEVEL_VERBOSE);

  Log.notice("========================================");
  Log.notice("    Dryer Controller");
  Log.notice("========================================");

  // Check if we rebooted due to watchdog
  if (watchdog_caused_reboot())
  {
    Log.warning("!!! System recovered from watchdog reset !!!");
    Log.warning("!!! Previous execution was frozen/hung !!!");
  }

  // Enable watchdog with 8 second timeout
  // This will reset the system if watchdog_update() is not called within 8 seconds
  watchdog_enable(8000, 1);
  Log.notice("Watchdog timer enabled (8 second timeout)");

  SetupI2C();
  delay(100);

  SetupOLED();
  delay(100);

  SetupRTC();
  delay(100);

  SetupSessionMonitor();

  SetupPins();
  delay(100);

  SetupDAC();
  delay(100);

  SetupSensors();
  delay(100);

  SetupDryer();
  delay(100);

  SetupSystemStatus();

  // Initialize was_running to current state
  was_running = dryer.IsRunning();
  Log.notice("Initial dryer state: %s", was_running ? "RUNNING" : "STOPPED");

  // If dryer is already running at boot, start logging immediately
  if (was_running && session_monitor.IsReady())
  {
    Log.notice("Dryer was running at boot - starting session monitoring");
    session_monitor.StartSession();
  }

  Log.notice("Setup complete!");
}

// ========== LOOP FUNCTIONS ==========

void UpdateSensors()
{
  unsigned long now = millis();

  if (now - last_sensor_update >= SENSOR_UPDATE_INTERVAL)
  {
    last_sensor_update = now;

    // Update CHT8305 sensors with error tracking
    static uint8_t inlet_errors = 0;
    static uint8_t outlet_errors = 0;

    int inlet_result = inlet_air_sensor.read();
    if (inlet_result == CHT8305_OK)
    {
      dryer.SetInletTemperature(inlet_air_sensor.getTemperature());
      dryer.SetInletHumidity(inlet_air_sensor.getHumidity());
      inlet_errors = 0; // Reset error counter on success
    }
    else
    {
      inlet_errors++;
      if (inlet_errors <= 3) // Only log first few errors
      {
        Log.warning("Inlet sensor read failed (error %d, count: %d)", inlet_result, inlet_errors);
      }
    }

    int outlet_result = outlet_air_sensor.read();
    if (outlet_result == CHT8305_OK)
    {
      dryer.SetOutletTemperature(outlet_air_sensor.getTemperature());
      dryer.SetOutletHumidity(outlet_air_sensor.getHumidity());
      outlet_errors = 0; // Reset error counter on success
    }
    else
    {
      outlet_errors++;
      if (outlet_errors <= 3) // Only log first few errors
      {
        Log.warning("Outlet sensor read failed (error %d, count: %d)", outlet_result, outlet_errors);
      }
    }

    // Update water temperature sensor (async mode)
    if (!water_temp_conversion_started)
    {
      // Start temperature conversion (non-blocking)
      water_temperature_sensor.requestTemperatures();
      water_temp_request_time = millis();
      water_temp_conversion_started = true;
    }
    else if (millis() - water_temp_request_time >= WATER_TEMP_CONVERSION_TIME)
    {
      // Read temperature after conversion time has elapsed
      float water_temperature = water_temperature_sensor.getTempCByIndex(0);
      if (water_temperature != DEVICE_DISCONNECTED_C)
      {
        dryer.SetWaterTemperature(water_temperature);
      }
      water_temp_conversion_started = false; // Ready for next conversion
    }

    // Log values
    Log.verbose("Inlet: %D°C, %D%% | Outlet: %D°C, %D%%",
                inlet_air_sensor.getTemperature(), inlet_air_sensor.getHumidity(),
                outlet_air_sensor.getTemperature(), outlet_air_sensor.getHumidity());
  }
}

void UpdateInputs()
{
  // This function is kept for future input handling if needed
}

void UpdateEncoderInputs()
{
  // Update encoder button debounce state
  button_encoder.update();

  // Rotary Encoder
  encoder.tick();
  encoder_position = encoder.getPosition();

  if (encoder_position != last_encoder_position)
  {
    int delta = encoder_position - last_encoder_position;
    last_encoder_position = encoder_position;

    if (menu.IsActive())
    {
      // Menu navigation or value adjustment
      if (menu.IsEditing())
      {
        // Adjust value in edit mode - process each step individually
        for (int i = 0; i < abs(delta); i++)
        {
          if (delta > 0)
          {
            Log.trace("Encoder increment");
            menu.GetCurrentItem()->OnIncrement(&menu);
          }
          else
          {
            Log.trace("Encoder decrement");
            menu.GetCurrentItem()->OnDecrement(&menu);
          }
        }
      }
      else
      {
        // Navigate menu
        if (delta > 0)
        {
          Log.trace("Encoder down");
          menu.Down();
        }
        else
        {
          Log.trace("Encoder up");
          menu.Up();
        }
      }
    }
  }

  // Encoder button
  if (button_encoder.fell())
  {
    Log.trace(">>> ENCODER BUTTON PRESSED <<<");

    // Button pressed (transition from HIGH to LOW)
    if (menu.IsActive())
    {
      Log.trace("Menu enter");
      menu.Enter();
    }
    else
    {
      Log.trace("Menu show");
      menu.Show();
    }
  }
}

void UpdateOutputs()
{
  // Static state to track previous values
  static bool last_heater_state = LOW;
  static bool last_fan_state = LOW;
  static uint8_t last_circulator_pwm = 0;
  static bool first_run = true;

  // Calculate new states
  bool heater_state = dryer.GetHeaterOutput() > 0.5 ? HIGH : LOW;
  bool fan_state = dryer.GetFanOutput() > 0.0 ? HIGH : LOW;

  // Update circulator PWM (0-255)
  // Signal is inverted because we use a PNP transistor (2N2222)
  uint8_t circulator_pwm = (1.0f - dryer.GetCirculatorOutput()) * 255;

  // Update heater if changed
  if (first_run || heater_state != last_heater_state)
  {
    digitalWrite(ELECTRIC_HEATER_RELAY_PIN, heater_state);
    last_heater_state = heater_state;

    Log.notice("Output changed - Heater: %s (%.0f%%)",
               heater_state ? "ON" : "OFF",
               dryer.GetHeaterOutput() * 100.0f);
  }

  // Update fan if changed
  if (first_run || fan_state != last_fan_state)
  {
    digitalWrite(FAN_RELAY_PIN, fan_state);
    last_fan_state = fan_state;

    Log.notice("Output changed - Fan: %s (%.0f%%)",
               fan_state ? "ON" : "OFF",
               dryer.GetFanOutput() * 100.0f);
  }

  // Update circulator if changed (with tolerance of ±1 to avoid jitter)
  if (first_run || abs(circulator_pwm - last_circulator_pwm) > 1)
  {
    analogWrite(WATER_CIRCULATOR_PWM_PIN, circulator_pwm);
    last_circulator_pwm = circulator_pwm;

    Log.notice("Output changed - Circulator PWM: %d/255 (%.0f%%)",
               circulator_pwm,
               dryer.GetCirculatorOutput() * 100.0f);
  }

  first_run = false;
}

void UpdateDisplay()
{
  unsigned long now = millis();

  if (now - last_display_update >= DISPLAY_UPDATE_INTERVAL)
  {
    last_display_update = now;

    // Serial.println("Updating display...");

    if (menu.IsActive())
    {
      // Render menu
      // Serial.println("Rendering menu");
      menu.Render();
    }
    else
    {
      // Serial.println("Rendering home page");
      // Time tracking
      display.SetTotalDutyTime(dryer.GetTotalDutyTime());
      display.SetPhaseDutyTime(dryer.GetPhaseElapsedTime());

      // Cycle durations
      display.SetCycleDuration(86400); // Default 24h for now
      display.SetPhaseDuration(dryer.GetSessionManager()->GetCurrentPhaseDuration());

      // Phase name
      display.SetPhase(dryer.GetPhaseName());

      // Air data
      display.SetInletAirData(
          dryer.GetInletTemperature(),
          inlet_air_sensor.getHumidity());
      display.SetOutletAirData(
          dryer.GetOutletTemperature(),
          outlet_air_sensor.getHumidity());

      // Right side icons and values
      display.SetRecyclingRate(dryer.GetRecyclingRate());
      display.SetVentilationState(dryer.GetFanOutput() > 0.0);
      display.SetHydraulicHeaterPower(dryer.GetHeatersManager()->GetHydraulicHeater()->GetPower());

      // Get water temperature from dryer (already updated in UpdateSensors)
      display.SetWaterTemperature(dryer.GetWaterTemperature());

      display.SetElectricHeaterPower(dryer.GetHeaterOutput() > 0.5);

      display.SetHydraulicHeaterEnabled(dryer.GetHydraulicEnabled());
      display.SetElectricHeaterEnabled(dryer.GetElectricEnabled());

      display.Update();
      // Serial.println("Home page rendered");
    }
  }
}

void UpdateControl()
{
  unsigned long now = millis();

  if (now - last_control_update >= CONTROL_LOOP_INTERVAL)
  {
    last_control_update = now;
    dryer.Update();
  }
}

void UpdateSettings()
{
  unsigned long now = millis();

  // Only save if settings changed by user
  // Note: EEPROM.commit() can take ~1 second, but this is acceptable
  // The watchdog (8s timeout) will prevent total system freeze
  if (settings_need_save)
  {
    Log.notice("Saving settings (user changed)...");
    settings_need_save = false;
    last_settings_save = now;

    Logger::Flush(); // Ensure log is sent before potentially slow operation
    unsigned long save_start = millis();

    dryer.SaveSettings();

    unsigned long save_time = millis() - save_start;
    Log.notice("Settings save completed in %d ms", save_time);

    if (save_time > 1000)
    {
      Log.warning("Settings save took too long: %d ms (but watchdog prevented freeze)", save_time);
    }
  }

  // Periodic save disabled to reduce flash wear
  // Settings are saved when user changes them and when dryer stops
}

void UpdateSessionMonitor()
{
  // Check if dryer state changed (started or stopped)
  bool is_running = dryer.IsRunning();

  if (is_running && !was_running)
  {
    // Dryer just started - start monitoring
    Log.notice("Dryer started - beginning session monitoring");
    if (session_monitor.StartSession())
    {
      Log.notice("Session monitoring started");
    }
    else
    {
      Log.error("Failed to start session monitoring");
    }
  }
  else if (!is_running && was_running)
  {
    // Dryer just stopped - stop monitoring and save settings
    Log.notice("Dryer stopped - ending session monitoring");
    session_monitor.StopSession();

    // Save settings when dryer stops to preserve state
    Log.notice("Saving settings on dryer stop...");
    Logger::Flush();
    unsigned long save_start = millis();
    dryer.SaveSettings();
    unsigned long save_time = millis() - save_start;
    Log.notice("Settings saved in %d ms", save_time);
  }

  was_running = is_running;

  // SessionMonitor.Update() writes directly to SD (mono-core)
  session_monitor.Update();

  // Retry SD initialization if needed
  if (!session_monitor.IsReady())
  {
    if (session_monitor.RetryInitialization())
    {
      Log.notice("SD card reinitialized successfully!");
    }
  }
}

void UpdateMemoryMonitor()
{
  unsigned long now = millis();

  if (now - last_memory_check >= MEMORY_CHECK_INTERVAL)
  {
    last_memory_check = now;

    // Get free heap memory
    uint32_t free_heap = rp2040.getFreeHeap();
    uint32_t total_heap = rp2040.getTotalHeap();
    uint32_t used_heap = total_heap - free_heap;

    Log.notice("========== MEMORY STATUS ==========");
    Log.notice("Free heap: %d bytes (%d%%)", free_heap, (free_heap * 100) / total_heap);
    Log.notice("Used heap: %d bytes (%d%%)", used_heap, (used_heap * 100) / total_heap);
    Log.notice("Total heap: %d bytes", total_heap);

    // Warning if less than 10% free
    if (free_heap < (total_heap / 10))
    {
      Log.warning("Low memory! Less than 10% free!");
    }

    Log.notice("===================================");
  }
}

void loop()
{
  // Feed the watchdog at the start of each loop iteration
  // This tells the watchdog that the system is still running
  watchdog_update();

  unsigned long loop_start = millis();
  loop_count++;

  // Log loop activity periodically
  if (loop_start - last_loop_log >= LOOP_LOG_INTERVAL)
  {
    last_loop_log = loop_start;
    Log.notice("Heartbeat - loop count: %d, uptime: %ds, SD: %s, logging: %s",
               loop_count,
               loop_start / 1000,
               session_monitor.IsReady() ? "OK" : "NOT READY",
               session_monitor.IsLogging() ? "YES" : "NO");
  }

  UpdateSensors();
  UpdateInputs();
  UpdateEncoderInputs();
  UpdateControl();
  UpdateSettings();
  UpdateSessionMonitor();
  UpdateMemoryMonitor();

  UpdateOutputs();
  UpdateDisplay();

  unsigned long loop_time = millis() - loop_start;

  // Warn if loop took too long
  if (loop_time > 100)
  {
    Log.warning("[Core 0] Long loop iteration: %d ms", loop_time);
  }

  delay(10); // Short delay to prevent CPU hogging
}
