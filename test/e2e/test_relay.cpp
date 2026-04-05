#include <Arduino.h>
#include "config.h"

// Test state
bool relay_state = false;
unsigned long last_toggle = 0;
const unsigned long TOGGLE_INTERVAL = 190000; // 190 seconds
static constexpr uint8_t kPin = 9;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n=== Electric Heater Relay Test ===");
  Serial.println("Testing relay on pin " + String(kPin));
  Serial.println("Toggle interval: 3 seconds\n");

  // Setup relay pin as output
  pinMode(kPin, OUTPUT);

  // Initialize relay to OFF
  // digitalWrite(kPin, LOW);
  digitalWrite(kPin, LOW);
  relay_state = false;
  last_toggle = millis();

  Serial.println("Relay initialized to OFF");
}

void loop()
{
  unsigned long now = millis();

  if (now - last_toggle >= TOGGLE_INTERVAL)
  {
    last_toggle = now;

    // Toggle relay state
    relay_state = !relay_state;
    digitalWrite(kPin, relay_state ? LOW : HIGH);

    // Log state
    Serial.print("[");
    Serial.print(now);
    Serial.print(" ms] Relay: ");
    Serial.println(relay_state ? "ON" : "OFF");
  }
}
