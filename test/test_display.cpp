#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SSD1306_ADDR 0x3C

#define I2C_BUS_1_SDA_PIN 12
#define I2C_BUS_1_SCL_PIN 13

TwoWire i2c_bus_1(i2c0, I2C_BUS_1_SDA_PIN, I2C_BUS_1_SCL_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &i2c_bus_1, OLED_RESET);

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n========================================");
  Serial.println("    OLED Display Test");
  Serial.println("========================================\n");

  // Initialize I2C bus
  i2c_bus_1.begin();
  i2c_bus_1.setClock(400000);
  Serial.println("I2C bus initialized");

  // Scan I2C
  Serial.println("Scanning I2C bus...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    i2c_bus_1.beginTransmission(addr);
    if (i2c_bus_1.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      Serial.println(addr, HEX);
    }
  }

  // Initialize display
  Serial.print("\nInitializing display at 0x");
  Serial.println(SSD1306_ADDR, HEX);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDR)) {
    Serial.println("ERROR: SSD1306 allocation failed!");
    while (1) delay(10);
  }

  Serial.println("Display allocation successful");

  // Set rotation (as per ESPHome config)
  display.setRotation(2);  // 180 degrees
  Serial.println("Rotation set to 180°");

  // Clear display
  display.clearDisplay();

  // Draw test pattern
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED");
  display.println("TEST");
  display.setTextSize(1);
  display.println("");
  display.println("Line 1");
  display.println("Line 2");
  display.println("Line 3");

  Serial.println("About to refresh display...");
  display.display();
  Serial.println("Display refreshed!");

  Serial.println("\n========================================");
  Serial.println("Test complete. Display should show text.");
  Serial.println("========================================\n");
}

void loop() {
  // Blink test pattern every second
  static unsigned long lastUpdate = 0;
  static bool inverted = false;

  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    inverted = !inverted;

    display.invertDisplay(inverted);
    Serial.print("Display inverted: ");
    Serial.println(inverted ? "YES" : "NO");
  }

  delay(10);
}
