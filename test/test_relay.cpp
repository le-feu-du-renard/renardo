#include <Arduino.h>
#include "config.h"

// Test state
bool relay_state = false;
unsigned long last_toggle = 0;
const unsigned long TOGGLE_INTERVAL = 2000; // 2 seconds

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n=== Electric Heater Relay Test ===");
  Serial.println("Testing relay on pin " + String(ELECTRIC_HEATER_RELAY_PIN));
  Serial.println("Toggle interval: 2 seconds\n");

  // Setup relay pin as output
  pinMode(ELECTRIC_HEATER_RELAY_PIN, OUTPUT);

  // Initialize relay to OFF
  digitalWrite(ELECTRIC_HEATER_RELAY_PIN, LOW);
  relay_state = false;
  last_toggle = millis();

  Serial.println("Relay initialized to OFF");
}

void loop() {
  unsigned long now = millis();

  if (now - last_toggle >= TOGGLE_INTERVAL) {
    last_toggle = now;

    // Toggle relay state
    relay_state = !relay_state;
    digitalWrite(ELECTRIC_HEATER_RELAY_PIN, relay_state ? HIGH : LOW);

    // Log state
    Serial.print("[");
    Serial.print(now);
    Serial.print(" ms] Relay: ");
    Serial.println(relay_state ? "ON" : "OFF");
  }
}
