#include <Arduino.h>
#include <Wire.h>
#include "Display.h"
#include "config.h"

// I2C Bus 1 only
TwoWire i2c_bus_1(i2c0, I2C_BUS_1_SDA_PIN, I2C_BUS_1_SCL_PIN);

// Display
Display display(&i2c_bus_1);

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n========================================");
  Serial.println("    Dryer - Display Only Test");
  Serial.println("========================================\n");

  // Initialize I2C bus 1
  i2c_bus_1.begin();
  i2c_bus_1.setClock(400000);
  Serial.println("I2C bus 1 initialized");

  // Scan I2C
  Serial.println("Scanning I2C bus 1...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    i2c_bus_1.beginTransmission(addr);
    if (i2c_bus_1.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      Serial.println(addr, HEX);
    }
  }

  // Initialize display
  if (!display.Begin()) {
    Serial.println("ERROR: Display initialization failed!");
    while (1) delay(10);
  }

  Serial.println("Core 0 setup complete!");
}

void loop() {
  static unsigned long lastUpdate = 0;
  static uint32_t counter = 0;

  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    counter++;

    Serial.print("Loop iteration: ");
    Serial.println(counter);

    // Update display with test data
    display.SetTotalDutyTime(counter);
    display.SetPhaseDutyTime(counter / 2);
    display.SetCycleDuration(3600);
    display.SetPhaseDuration(1800);
    display.SetPhase("TEST");

    display.SetInletAirData(25.5, 45.0);
    display.SetOutletAirData(50.0, 20.0);

    display.SetRecyclingRate(75.0);
    display.SetVentilationState(counter % 2 == 0);
    display.SetHydraulicHeaterPower(50.0);
    display.SetWaterTemperature(45.0);
    display.SetElectricHeaterPower(counter % 3 == 0);

    Serial.println("Updating display...");
    display.Update();
    Serial.println("Display updated");
  }

  delay(10);
}
