// Display wiring test — built by `pio run -e pin_test -t upload`.
//
// TFT_eSPI never reads anything back from the panel, so a blank screen says
// nothing about which of the five signals is actually reaching it. This drives
// each one on its own, slowly enough to follow with a multimeter or an LED and
// a 1 kOhm resistor to ground, and announces what it is doing on the serial
// monitor.
//
// It deliberately does not use TFT_eSPI: the point is to prove the wire, not
// the library.

#include <Arduino.h>

#include "config.h"

namespace
{

struct TestPin
{
  const char *name;      // silkscreen name on the module
  const char *role;
  uint8_t     pin;
  uint8_t     header;    // physical pin on the Pico header
};

// Order matches the wiring table in HARDWARE.md.
const TestPin kPins[] = {
    {"CS ", "chip select", TFT_CS_PIN, 22},
    {"SCL", "SPI clock",   SPI0_SCK_PIN, 24},
    {"SDA", "SPI data",    SPI0_MOSI_PIN, 25},
    {"DC ", "data/command", TFT_DC_PIN, 26},
    {"RES", "reset",       TFT_RST_PIN, 27},
};

constexpr uint8_t kPinCount = sizeof(kPins) / sizeof(kPins[0]);
constexpr uint8_t kBlinks = 6;
constexpr uint32_t kHalfPeriodMs = 500;

void DriveAllLow()
{
  for (uint8_t i = 0; i < kPinCount; i++)
  {
    digitalWrite(kPins[i].pin, LOW);
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(2000);

  for (uint8_t i = 0; i < kPinCount; i++)
  {
    pinMode(kPins[i].pin, OUTPUT);
    digitalWrite(kPins[i].pin, LOW);
  }

  Serial.println();
  Serial.println("========================================");
  Serial.println("  Display wiring test");
  Serial.println("========================================");
  Serial.println("Each signal is driven on its own for 6 s at 1 Hz.");
  Serial.println("Probe the module side of the wire, not the Pico side:");
  Serial.println("that is what tells a broken wire from a wrong pin.");
  Serial.println();
}

void loop()
{
  for (uint8_t i = 0; i < kPinCount; i++)
  {
    const TestPin &target = kPins[i];

    Serial.printf("--> %s (%s)  GP%-2u  header pin %u : toggling now\n",
                  target.name, target.role, target.pin, target.header);

    // Everything else sits low, so a probe that moves on more than one wire
    // means two module pins are shorted together.
    DriveAllLow();

    for (uint8_t blink = 0; blink < kBlinks; blink++)
    {
      digitalWrite(target.pin, HIGH);
      delay(kHalfPeriodMs);
      digitalWrite(target.pin, LOW);
      delay(kHalfPeriodMs);
    }
  }

  Serial.println();
  Serial.println("All five signals high for 3 s (continuity check), then low.");
  for (uint8_t i = 0; i < kPinCount; i++)
  {
    digitalWrite(kPins[i].pin, HIGH);
  }
  delay(3000);
  DriveAllLow();
  Serial.println("Restarting the sweep.");
  Serial.println();
}
