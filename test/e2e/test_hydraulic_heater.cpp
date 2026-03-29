#include <Arduino.h>
#include "HydraulicHeater.h"
#include "config.h"

// Hydraulic heater instance
HydraulicHeater hydraulic_heater;

// Test state
float current_power = 0.0f;
unsigned long last_update = 0;
const unsigned long UPDATE_INTERVAL = 1000; // 1 second

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n=== Hydraulic Heater Test ===");
  Serial.println("Testing power range from 0% to 100% with 10% steps");
  Serial.println("Power will increment every 1 second, then reset to 0%\n");

  // Setup PWM pin
  pinMode(WATER_CIRCULATOR_PWM_PIN, OUTPUT);

  // Initialize hydraulic heater
  hydraulic_heater.Begin();

  // Set initial power to 0%
  hydraulic_heater.SetPower(0.0f);
  current_power = 0.0f;
  last_update = millis();

  Serial.println("Power% | PWM Output | Inverted PWM | Pin Value");
  Serial.println("-------|------------|--------------|----------");
}

void loop()
{
  unsigned long now = millis();

  if (now - last_update >= UPDATE_INTERVAL)
  {
    last_update = now;

    // Set new power level
    hydraulic_heater.SetPower(current_power);

    // Get output value (0.0-1.0)
    float output = hydraulic_heater.GetOutput();

    // Calculate inverted PWM (what actually goes to the pin with PNP transistor)
    float inverted_output = 1.0f - output;

    // Calculate pin value (0-255)
    uint8_t pin_value = inverted_output * 255;

    // Write to pin
    analogWrite(WATER_CIRCULATOR_PWM_PIN, pin_value);

    // Print results
    Serial.print(current_power, 1);
    Serial.print("%   | ");
    Serial.print(output * 100.0f, 1);
    Serial.print("%     | ");
    Serial.print(inverted_output * 100.0f, 1);
    Serial.print("%       | ");
    Serial.println(pin_value);

    // Increment power by 10%
    current_power += 10.0f;

    // Reset to 0% when reaching 110% (after 100% is displayed)
    if (current_power > 100.0f)
    {
      current_power = 0.0f;
      Serial.println("-------|------------|--------------|----------");
      Serial.println("Cycle complete - resetting to 0%\n");
      delay(500); // Small pause between cycles
    }
  }
}
