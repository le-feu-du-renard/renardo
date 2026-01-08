#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RotaryEncoder.h>
#include <Bounce2.h>

#include "config.h"
#include <CHT8305.h>
#include "Display.h"
#include "Dryer.h"
#include "MenuSystem.h"
#include "MenuStructure.h"

// ========== GLOBAL OBJECTS ==========

// I2C Buses
TwoWire i2c_bus_1(i2c0, I2C_BUS_1_SDA_PIN, I2C_BUS_1_SCL_PIN);
TwoWire i2c_bus_2(i2c1, I2C_BUS_2_SDA_PIN, I2C_BUS_2_SCL_PIN);

// Temperature/humidity sensors
CHT8305 inlet_air_sensor(CHT8305_INLET_ADDR, &i2c_bus_1);
CHT8305 outlet_air_sensor(CHT8305_OUTLET_ADDR, &i2c_bus_2);

// OneWire for water temperature sensor
OneWire one_wire(WATER_SENSOR_PIN);
DallasTemperature water_temperature_sensor(&one_wire);

// DAC
DFRobot_GP8403 dac(&i2c_bus_2, DAC_GP8403_ADDR);

// OLED
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &i2c_bus_1, SCREEN_OLED_RESET);

// Rotary Encoder
RotaryEncoder encoder(ROTARY_ENCODER_CLK_PIN, ROTARY_ENCODER_DT_PIN, RotaryEncoder::LatchMode::TWO03);

// Display
Display display(&oled);

// Dryer controller
Dryer dryer(&dac);

// Menu System
MenuSystem menu(&dryer, &display);

// Debounce objects
Bounce button_start = Bounce();
Bounce button_encoder = Bounce();

// ========== GLOBAL VARIABLES ==========

// Core 0 variables (sensors, control, outputs, display)
unsigned long last_sensor_update = 0;
unsigned long last_control_update = 0;
unsigned long last_display_update = 0;
unsigned long last_settings_save = 0;
bool settings_need_save = false;

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
  // Buttons with debounce
  button_start.attach(BUTTON_START_PIN, INPUT_PULLUP);
  button_start.interval(50); // 50ms debounce interval

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
  i2c_bus_1.begin();
  i2c_bus_1.setClock(400000);
  Serial.println("I2C bus 1 initialized");

  i2c_bus_2.begin();
  i2c_bus_2.setClock(50000);
  Serial.println("I2C bus 2 initialized");

  // Scan all I2C buses
  ScanI2C(i2c_bus_1, "i2c_bus_1");
  ScanI2C(i2c_bus_2, "i2c_bus_2");
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

  oled.setRotation(2); // 0=0°, 1=90°, 2=180°, 3=270°
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
  // CHT8305 sensors
  if (inlet_air_sensor.begin() != 0)
  {
    Serial.println("ERROR: Inlet air sensor not found!");
  }
  else if (!inlet_air_sensor.isConnected())
  {
    Serial.println("ERROR: Inlet air sensor not connected!");
  }

  if (outlet_air_sensor.begin() != 0)
  {
    Serial.println("ERROR: Outlet air sensor not found!");
  }
  else if (!outlet_air_sensor.isConnected())
  {
    Serial.println("ERROR: Outlet air sensor not connected!");
  }

  // Water temperature sensor
  water_temperature_sensor.begin();
  water_temperature_sensor.setWaitForConversion(false); // Async mode
  Serial.print("Found ");
  Serial.print(water_temperature_sensor.getDeviceCount());
  Serial.println(" OneWire devices (async mode enabled)");
}

// ========== MAIN SETUP (Core 0) ==========

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("    Dryer Controller - MINIMAL TEST");
  Serial.println("========================================\n");

  SetupI2C();
  SetupOLED();

  SetupPins();
  SetupDAC();
  SetupSensors();

  dryer.Begin();
  dryer.SetSettingsChangedCallback(OnSettingsChanged);
  menu.Begin(MenuStructure::BuildMenu());

  Serial.println("Setup complete!");
}

// ========== LOOP FUNCTIONS ==========

void UpdateSensors()
{
  unsigned long now = millis();

  if (now - last_sensor_update >= SENSOR_UPDATE_INTERVAL)
  {
    last_sensor_update = now;

    // Update CHT8305 sensors
    if (inlet_air_sensor.read() == CHT8305_OK)
    {
      dryer.SetInletTemperature(inlet_air_sensor.getTemperature());
      dryer.SetInletHumidity(inlet_air_sensor.getHumidity());
    }

    if (outlet_air_sensor.read() == CHT8305_OK)
    {
      dryer.SetOutletTemperature(outlet_air_sensor.getTemperature());
      dryer.SetOutletHumidity(outlet_air_sensor.getHumidity());
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
  // Update start/stop button debounce state
  button_start.update();

  // Start/Stop button
  if (button_start.fell())
  {
    // Button pressed (transition from HIGH to LOW)
    Serial.println(">>> START BUTTON PRESSED <<<");
    if (dryer.IsRunning())
    {
      Serial.println("Button stop press");
      dryer.Stop();
    }
    else
    {
      Serial.println("Button start press");
      dryer.Start();
    }
  }
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
        // Adjust value in edit mode
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
  // Update relays
  bool heater_state = dryer.GetHeaterOutput() > 0.5 ? HIGH : LOW;
  bool fan_state = dryer.GetFanOutput() > 0.0 ? HIGH : LOW;
  digitalWrite(ELECTRIC_HEATER_RELAY_PIN, heater_state);
  digitalWrite(FAN_RELAY_PIN, fan_state);

  // Update circulator PWM (0-255)
  uint8_t circulator_pwm = dryer.GetCirculatorOutput() * 255;
  analogWrite(WATER_CIRCULATOR_PWM_PIN, circulator_pwm);

  // // Log output values
  // Serial.print("Outputs - Heater: ");
  // Serial.print(heater_state ? "ON" : "OFF");
  // Serial.print(" (");
  // Serial.print(dryer.GetHeaterOutput() * 100.0f);
  // Serial.print("%), Fan: ");
  // Serial.print(fan_state ? "ON" : "OFF");
  // Serial.print(" (");
  // Serial.print(dryer.GetFanOutput() * 100.0f);
  // Serial.print("%), Circulator PWM: ");
  // Serial.print(circulator_pwm);
  // Serial.print("/255 (");
  // Serial.print(dryer.GetCirculatorOutput() * 100.0f);
  // Serial.println("%)");
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
      display.SetPhaseDutyTime(dryer.GetPhasesManager()->GetPhaseElapsedTime());

      // Cycle durations
      display.SetCycleDuration(dryer.GetDryingSessionDuration());
      display.SetPhaseDuration(dryer.GetPhasesManager()->GetCurrentPhaseDuration());

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
      display.SetHydraulicHeaterPower(dryer.GetCirculatorOutput() * 100.0);

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

  // Check if we need to save settings (either changed or periodic)
  if (settings_need_save || (now - last_settings_save >= SETTINGS_SAVE_INTERVAL))
  {
    last_settings_save = now;

    if (settings_need_save)
    {
      Serial.println("Saving settings immediately (user changed)...");
      settings_need_save = false;
    }
    else
    {
      Serial.println("Saving settings periodically...");
    }

    dryer.SaveSettings();
  }
}

void loop()
{
  UpdateSensors();
  UpdateInputs();
  UpdateEncoderInputs();
  UpdateControl();
  UpdateSettings();
  UpdateOutputs();
  UpdateDisplay();

  delay(10); // Short delay to prevent CPU hogging
}
