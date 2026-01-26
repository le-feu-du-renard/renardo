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
DFRobot_GP8403 dac(&i2c_bus_1, DAC_GP8403_ADDR);

// OLED
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &i2c_bus_2, SCREEN_OLED_RESET);

// RTC (on bus 2)
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
  Serial.println("Settings changed, will save immediately");
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
  Serial.print("Scanning ");
  Serial.print(bus_name);
  Serial.println("...");

  int count = 0;
  for (byte addr = 1; addr < 127; addr++)
  {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0)
    {
      Serial.print("  Found device at 0x");
      if (addr < 16)
        Serial.print("0");
      Serial.println(addr, HEX);
      count++;
    }
  }

  if (count == 0)
  {
    Serial.println("  No devices found");
  }
  Serial.println();
}

void SetupI2C()
{
  // Bus 1: DAC + Outlet Air Sensor
  i2c_bus_1.begin();
  i2c_bus_1.setClock(50000);
  i2c_bus_1.setTimeout(1000); // 1 second timeout to prevent I2C hangs
  Serial.println("I2C bus 1 (DAC + Outlet) initialized at 50kHz with 1s timeout");

  // Bus 2: OLED + Inlet Air Sensor + RTC - Enable pull-ups for stability
  pinMode(I2C_BUS_2_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_BUS_2_SCL_PIN, INPUT_PULLUP);
  i2c_bus_2.begin();
  i2c_bus_2.setClock(100000); // 100kHz for stability with 3 devices
  i2c_bus_2.setTimeout(1000); // 1 second timeout to prevent I2C hangs
  Serial.println("I2C bus 2 (OLED + Inlet + RTC) initialized at 100kHz with internal pull-ups and 1s timeout");

  // Scan all I2C buses
  ScanI2C(i2c_bus_1, "i2c_bus_1 (DAC + Outlet)");
  ScanI2C(i2c_bus_2, "i2c_bus_2 (OLED + Inlet + RTC)");
}

void SetupOLED()
{
  Serial.println("Initialize OLED...");

  // Initialize display
  if (!oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_SSD1306_ADDR))
  {
    Serial.println("ERROR: OLED initialization failed!");
  }
  else
  {
    Serial.println("SUCCESS: OLED initialized!");
    oled.clearDisplay();
    oled.display();
    Serial.println("OLED cleared");
  }

  oled.setRotation(0); // 0=0°, 1=90°, 2=180°, 3=270°
  Serial.println("OLED rotation set to 180°");
}

void SetupDAC()
{
  Serial.println("Initialize DAC...");

  if (dac.begin() != 0)
  {
    Serial.println("Failed to initialize GP8403 DAC");
  }
  else
  {
    dac.setDACOutRange(DFRobot_GP8403::eOutputRange10V);
    Serial.println("GP8403 DAC initialized with 0-10V range");
  }
}

void SetupSensors()
{
  // Add delay to let I2C bus stabilize after OLED/RTC init
  delay(100);

  // CHT8305 sensors
  Serial.println("Testing Inlet sensor connection...");

  // Try to clear any I2C bus issues
  i2c_bus_2.beginTransmission(CHT8305_INLET_ADDR);
  i2c_bus_2.endTransmission();
  delay(50);

  int result = inlet_air_sensor.begin();
  if (result != 0)
  {
    Serial.print("ERROR: Inlet air sensor not found! Error code: ");
    Serial.println(result);
  }
  else if (!inlet_air_sensor.isConnected())
  {
    Serial.println("ERROR: Inlet air sensor not connected!");
  }
  else
  {
    Serial.println("SUCCESS: Inlet air sensor initialized!");
  }

  // Clear I2C bus 1 before outlet sensor init
  Serial.println("Testing Outlet sensor connection...");
  i2c_bus_1.beginTransmission(CHT8305_OUTLET_ADDR);
  i2c_bus_1.endTransmission();
  delay(50);

  int outlet_result = outlet_air_sensor.begin();
  if (outlet_result != 0)
  {
    Serial.print("ERROR: Outlet air sensor not found! Error code: ");
    Serial.println(outlet_result);
  }
  else if (!outlet_air_sensor.isConnected())
  {
    Serial.println("ERROR: Outlet air sensor not connected!");
  }
  else
  {
    Serial.println("SUCCESS: Outlet air sensor initialized!");
  }

  // Water temperature sensor
  water_temperature_sensor.begin();
  water_temperature_sensor.setWaitForConversion(false); // Async mode
  Serial.print("Found ");
  Serial.print(water_temperature_sensor.getDeviceCount());
  Serial.println(" OneWire devices (async mode enabled)");
}

void SetupRTC()
{
  Serial.println("Initialize RTC...");

  if (!time_manager.Begin())
  {
    Serial.println("ERROR: RTC initialization failed!");
  }
  else
  {
    Serial.println("SUCCESS: RTC initialized!");

    // Check if time needs to be set
    if (time_manager.HasLostPower())
    {
      Serial.println("WARNING: Please set the RTC time");
      Serial.println("RTC has been set to compile time as default");
    }

    // Display current time
    Serial.print("Current time: ");
    Serial.println(time_manager.GetDateTimeString());
  }
}

void SetupSessionMonitor()
{
  Serial.println("Initialize Session Monitor...");
  Serial.println("Note: This may take a few seconds if SD card is not present");

  if (!session_monitor.Begin())
  {
    Serial.println("ERROR: Session Monitor initialization failed!");
    Serial.println("WARNING: System will continue without session monitoring");
  }
  else
  {
    Serial.println("SUCCESS: Session Monitor initialized!");
  }

  Serial.println("Session Monitor setup complete");
}

// ========== MAIN SETUP (Core 0) ==========

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n========================================");
  Serial.println("    Dryer Controller - Mono Core");
  Serial.println("========================================\n");

  // Check if we rebooted due to watchdog
  if (watchdog_caused_reboot())
  {
    Serial.println("!!! WARNING: System recovered from watchdog reset !!!");
    Serial.println("!!! Previous execution was frozen/hung !!!");
  }

  // Enable watchdog with 8 second timeout
  // This will reset the system if watchdog_update() is not called within 8 seconds
  watchdog_enable(8000, 1);
  Serial.println("Watchdog timer enabled (8 second timeout)");

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

  dryer.Begin();
  dryer.SetSettingsChangedCallback(OnSettingsChanged);
  menu.Begin(MenuStructure::BuildMenu());

  // Initialize was_running to current state
  was_running = dryer.IsRunning();
  Serial.print("Initial dryer state: ");
  Serial.println(was_running ? "RUNNING" : "STOPPED");

  // If dryer is already running at boot, start logging immediately
  if (was_running && session_monitor.IsReady())
  {
    Serial.println("Dryer was running at boot - starting session monitoring");
    session_monitor.StartSession();
  }

  Serial.println("Setup complete!");
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
        Serial.print("WARNING: Inlet sensor read failed (error ");
        Serial.print(inlet_result);
        Serial.print(", count: ");
        Serial.print(inlet_errors);
        Serial.println(")");
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
        Serial.print("WARNING: Outlet sensor read failed (error ");
        Serial.print(outlet_result);
        Serial.print(", count: ");
        Serial.print(outlet_errors);
        Serial.println(")");
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
    Serial.print("Inlet: ");
    Serial.print(inlet_air_sensor.getTemperature(), 1);
    Serial.print("°C, ");
    Serial.print(inlet_air_sensor.getHumidity(), 0);
    Serial.print("% | Outlet: ");
    Serial.print(outlet_air_sensor.getTemperature(), 1);
    Serial.print("°C, ");
    Serial.print(outlet_air_sensor.getHumidity(), 0);
    Serial.println("%");
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
            Serial.println("Encoder increment");
            menu.GetCurrentItem()->OnIncrement(&menu);
          }
          else
          {
            Serial.println("Encoder decrement");
            menu.GetCurrentItem()->OnDecrement(&menu);
          }
        }
      }
      else
      {
        // Navigate menu
        if (delta > 0)
        {
          Serial.println("Encoder down");
          menu.Down();
        }
        else
        {
          Serial.println("Encoder up");
          menu.Up();
        }
      }
    }
  }

  // Encoder button
  if (button_encoder.fell())
  {
    Serial.println(">>> ENCODER BUTTON PRESSED <<<");

    // Button pressed (transition from HIGH to LOW)
    if (menu.IsActive())
    {
      Serial.println("Menu enter");
      menu.Enter();
    }
    else
    {
      Serial.println("Menu show");
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

    Serial.print("Output changed - Heater: ");
    Serial.print(heater_state ? "ON" : "OFF");
    Serial.print(" (");
    Serial.print(dryer.GetHeaterOutput() * 100.0f);
    Serial.println("%)");
  }

  // Update fan if changed
  if (first_run || fan_state != last_fan_state)
  {
    digitalWrite(FAN_RELAY_PIN, fan_state);
    last_fan_state = fan_state;

    Serial.print("Output changed - Fan: ");
    Serial.print(fan_state ? "ON" : "OFF");
    Serial.print(" (");
    Serial.print(dryer.GetFanOutput() * 100.0f);
    Serial.println("%)");
  }

  // Update circulator if changed (with tolerance of ±1 to avoid jitter)
  if (first_run || abs(circulator_pwm - last_circulator_pwm) > 1)
  {
    analogWrite(WATER_CIRCULATOR_PWM_PIN, circulator_pwm);
    last_circulator_pwm = circulator_pwm;

    Serial.print("Output changed - Circulator PWM: ");
    Serial.print(circulator_pwm);
    Serial.print("/255 (");
    Serial.print(dryer.GetCirculatorOutput() * 100.0f);
    Serial.println("%)");
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
    Serial.println("Saving settings (user changed)...");
    settings_need_save = false;
    last_settings_save = now;

    Serial.flush(); // Ensure log is sent before potentially slow operation
    unsigned long save_start = millis();

    dryer.SaveSettings();

    unsigned long save_time = millis() - save_start;
    Serial.print("Settings save completed in ");
    Serial.print(save_time);
    Serial.println("ms");

    if (save_time > 1000)
    {
      Serial.print("WARNING: Settings save took too long: ");
      Serial.print(save_time);
      Serial.println("ms (but watchdog prevented freeze)");
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
    Serial.println("Dryer started - beginning session monitoring");
    if (session_monitor.StartSession())
    {
      Serial.println("Session monitoring started");
    }
    else
    {
      Serial.println("Failed to start session monitoring");
    }
  }
  else if (!is_running && was_running)
  {
    // Dryer just stopped - stop monitoring and save settings
    Serial.println("Dryer stopped - ending session monitoring");
    session_monitor.StopSession();

    // Save settings when dryer stops to preserve state
    Serial.println("Saving settings on dryer stop...");
    Serial.flush();
    unsigned long save_start = millis();
    dryer.SaveSettings();
    unsigned long save_time = millis() - save_start;
    Serial.print("Settings saved in ");
    Serial.print(save_time);
    Serial.println("ms");
  }

  was_running = is_running;

  // SessionMonitor.Update() writes directly to SD (mono-core)
  session_monitor.Update();

  // Retry SD initialization if needed
  if (!session_monitor.IsReady())
  {
    if (session_monitor.RetryInitialization())
    {
      Serial.println("SD card reinitialized successfully!");
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

    Serial.println("========== MEMORY STATUS ==========");
    Serial.print("Free heap: ");
    Serial.print(free_heap);
    Serial.print(" bytes (");
    Serial.print((free_heap * 100) / total_heap);
    Serial.println("%)");

    Serial.print("Used heap: ");
    Serial.print(used_heap);
    Serial.print(" bytes (");
    Serial.print((used_heap * 100) / total_heap);
    Serial.println("%)");

    Serial.print("Total heap: ");
    Serial.print(total_heap);
    Serial.println(" bytes");

    // Warning if less than 10% free
    if (free_heap < (total_heap / 10))
    {
      Serial.println("WARNING: Low memory! Less than 10% free!");
    }

    Serial.println("===================================");
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
    Serial.print("Heartbeat - loop count: ");
    Serial.print(loop_count);
    Serial.print(", uptime: ");
    Serial.print(loop_start / 1000);
    Serial.print("s, SD: ");
    Serial.print(session_monitor.IsReady() ? "OK" : "NOT READY");
    Serial.print(", logging: ");
    Serial.println(session_monitor.IsLogging() ? "YES" : "NO");
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
    Serial.print("[Core 0] WARNING: Long loop iteration: ");
    Serial.print(loop_time);
    Serial.println("ms");
  }

  delay(10); // Short delay to prevent CPU hogging
}
