#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RotaryEncoder.h>

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

// Display
Display display;

// Dryer controller
Dryer dryer;

// Rotary Encoder
RotaryEncoder encoder(ROTARY_ENCODER_CLK_PIN, ROTARY_ENCODER_DT_PIN, RotaryEncoder::LatchMode::TWO03);

// Menu System
MenuSystem menu(&dryer, &display);

// ========== GLOBAL VARIABLES ==========

unsigned long last_sensor_update = 0;
unsigned long last_display_update = 0;
unsigned long last_control_update = 0;

bool button_start_pressed = false;
bool last_button_state = HIGH;

int encoder_position = 0;
int last_encoder_position = 0;

// ========== INTERRUPTS ==========

void EncoderISR() {
  encoder.tick();
}

// ========== SETUP FUNCTIONS ==========

void SetupPins() {
  // Buttons
  pinMode(BUTTON_START_PIN, INPUT_PULLUP);
  pinMode(ROTARY_ENCODER_SW_PIN, INPUT_PULLUP);

  // LEDs
  pinMode(STOP_LED_PIN, OUTPUT);
  pinMode(START_LED_PIN, OUTPUT);
  pinMode(ELECTRIC_HEATER_LED_PIN, OUTPUT);

  // Relays
  pinMode(ELECTRIC_HEATER_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);

  // PWM
  pinMode(WATER_CIRCULATOR_PWM_PIN, OUTPUT);

  // Initial output state
  digitalWrite(ELECTRIC_HEATER_RELAY_PIN, LOW);
  digitalWrite(FAN_RELAY_PIN, LOW);
  digitalWrite(STOP_LED_PIN, HIGH);
  digitalWrite(START_LED_PIN, LOW);
  digitalWrite(ELECTRIC_HEATER_LED_PIN, LOW);
}

void SetupI2C() {
  i2c_bus_1.begin();
  i2c_bus_1.setClock(400000);

  i2c_bus_2.begin();
  i2c_bus_2.setClock(50000);

  Serial.println("I2C buses initialized");
}

void SetupSensors() {
  // CHT8305 sensors
  if (inlet_air_sensor.begin() != 0) {
    Serial.println("ERROR: Inlet air sensor not found!");
  } else if (!inlet_air_sensor.isConnected()) {
    Serial.println("ERROR: Inlet air sensor not connected!");
  }

  if (outlet_air_sensor.begin() != 0) {
    Serial.println("ERROR: Outlet air sensor not found!");
  } else if (!outlet_air_sensor.isConnected()) {
    Serial.println("ERROR: Outlet air sensor not connected!");
  }

  // Water temperature sensor
  water_temperature_sensor.begin();
  Serial.print("Found ");
  Serial.print(water_temperature_sensor.getDeviceCount());
  Serial.println(" OneWire devices");
}

// ========== MAIN SETUP ==========

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("    Dryer Controller - PlatformIO");
  Serial.println("========================================\n");

  SetupPins();
  SetupI2C();

  // Initialize display
  if (!display.Begin()) {
    Serial.println("ERROR: Display initialization failed!");
  }

  SetupSensors();

  // Initialize controller
  dryer.Begin();

  // Initialize menu system
  menu.Begin(MenuStructure::BuildMenu());

  // Configure encoder interrupts
  attachInterrupt(digitalPinToInterrupt(ROTARY_ENCODER_CLK_PIN), EncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ROTARY_ENCODER_DT_PIN), EncoderISR, CHANGE);

  Serial.println("System ready!\n");
}

// ========== LOOP FUNCTIONS ==========

void UpdateSensors() {
  unsigned long now = millis();

  if (now - last_sensor_update >= SENSOR_UPDATE_INTERVAL) {
    last_sensor_update = now;

    // Update CHT8305 sensors
    if (inlet_air_sensor.read() == CHT8305_OK) {
      dryer.SetInletTemperature(inlet_air_sensor.getTemperature());
      dryer.SetInletHumidity(inlet_air_sensor.getHumidity());
    }

    if (outlet_air_sensor.read() == CHT8305_OK) {
      dryer.SetOutletTemperature(outlet_air_sensor.getTemperature());
      dryer.SetOutletHumidity(outlet_air_sensor.getHumidity());
    }

    // Update water temperature sensor
    water_temperature_sensor.requestTemperatures();
    float water_temperature = water_temperature_sensor.getTempCByIndex(0);
    if (water_temperature != DEVICE_DISCONNECTED_C) {
      dryer.SetWaterTemperature(water_temperature);
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

void UpdateInputs() {
  // Start/Stop button
  bool current_button_state = digitalRead(BUTTON_START_PIN);

  if (current_button_state == LOW && last_button_state == HIGH) {
    // Button pressed
    delay(50);  // Debounce
    if (digitalRead(BUTTON_START_PIN) == LOW) {
      if (dryer.IsRunning()) {
        dryer.Stop();
      } else {
        dryer.Start();
      }
    }
  }

  last_button_state = current_button_state;

  // Rotary Encoder
  encoder.tick();
  encoder_position = encoder.getPosition();

  if (encoder_position != last_encoder_position) {
    int delta = encoder_position - last_encoder_position;
    last_encoder_position = encoder_position;

    if (menu.IsActive()) {
      // Menu navigation or value adjustment
      if (menu.IsEditing()) {
        // Adjust value in edit mode
        if (delta > 0) {
          menu.GetCurrentItem()->OnIncrement(&menu);
        } else {
          menu.GetCurrentItem()->OnDecrement(&menu);
        }
      } else {
        // Navigate menu
        if (delta > 0) {
          menu.Down();
        } else {
          menu.Up();
        }
      }
    } else {
      // Adjust target temperature when menu not active
      float new_target = dryer.GetTargetTemperature() + (delta * 0.5);
      dryer.SetTargetTemperature(new_target);
    }
  }

  // Encoder button
  static bool last_encoder_button_state = HIGH;
  bool encoder_button_state = digitalRead(ROTARY_ENCODER_SW_PIN);

  if (encoder_button_state == LOW && last_encoder_button_state == HIGH) {
    delay(50);  // Debounce
    if (digitalRead(ROTARY_ENCODER_SW_PIN) == LOW) {
      if (menu.IsActive()) {
        menu.Enter();
      } else {
        menu.Show();
      }
    }
  }

  last_encoder_button_state = encoder_button_state;
}

void UpdateOutputs() {
  // Update relays
  digitalWrite(ELECTRIC_HEATER_RELAY_PIN, dryer.GetHeaterOutput() > 0.5 ? HIGH : LOW);
  digitalWrite(FAN_RELAY_PIN, dryer.GetFanOutput() > 0.0 ? HIGH : LOW);

  // Update circulator PWM (0-255)
  analogWrite(WATER_CIRCULATOR_PWM_PIN, dryer.GetCirculatorOutput() * 255);

  // Update LEDs
  digitalWrite(STOP_LED_PIN, !dryer.IsRunning() ? HIGH : LOW);
  digitalWrite(START_LED_PIN, dryer.IsRunning() ? HIGH : LOW);
  digitalWrite(ELECTRIC_HEATER_LED_PIN, dryer.GetHeaterOutput() > 0.5 ? HIGH : LOW);
}

void UpdateDisplay() {
  unsigned long now = millis();

  if (now - last_display_update >= DISPLAY_UPDATE_INTERVAL) {
    last_display_update = now;

    if (menu.IsActive()) {
      // Render menu
      menu.Render();
    } else {
      // Render home page with ESPHome layout
      // Time tracking
      display.SetTotalDutyTime(dryer.GetTotalDutyTime());
      display.SetPhaseDutyTime(dryer.GetPhasesManager()->GetPhaseElapsedTime() / 1000);

      // Cycle durations
      display.SetCycleDuration(dryer.GetInitPhaseDuration() +
                               dryer.GetExtractionPhaseDuration() +
                               dryer.GetCirculationPhaseDuration());
      display.SetPhaseDuration(dryer.GetPhasesManager()->GetParams().init_phase_duration_s);

      // Phase name
      display.SetPhase(dryer.GetPhaseName());

      // Air data
      display.SetInletAirData(
        dryer.GetInletTemperature(),
        inlet_air_sensor.getHumidity()
      );
      display.SetOutletAirData(
        dryer.GetOutletTemperature(),
        outlet_air_sensor.getHumidity()
      );

      // Right side icons and values
      display.SetRecyclingRate(dryer.GetRecyclingRate());
      display.SetVentilationState(dryer.GetFanOutput() > 0.0);
      display.SetHydraulicHeaterPower(dryer.GetCirculatorOutput() * 100.0);

      // Get water temperature from sensor
      water_temperature_sensor.requestTemperatures();
      float water_temp = water_temperature_sensor.getTempCByIndex(0);
      if (water_temp != DEVICE_DISCONNECTED_C) {
        display.SetWaterTemperature(water_temp);
      }

      display.SetElectricHeaterPower(dryer.GetHeaterOutput() > 0.5);

      display.Update();
    }
  }
}

void UpdateControl() {
  unsigned long now = millis();

  if (now - last_control_update >= CONTROL_LOOP_INTERVAL) {
    last_control_update = now;
    dryer.Update();
  }
}

// ========== MAIN LOOP ==========

void loop() {
  UpdateSensors();
  UpdateInputs();
  UpdateControl();
  UpdateOutputs();
  UpdateDisplay();

  // Small delay to avoid CPU overload
  delay(10);
}
