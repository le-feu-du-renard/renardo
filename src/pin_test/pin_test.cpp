// Display wiring test — built by `pio run -e pin_test -t upload -t monitor`.
//
// Continuity checker that needs no instrument. The five display signals are set
// to inputs with pull-ups, so each one idles high. Touching the corresponding
// pad on the *module* with a wire from GND pulls it low, and the Pico says
// which signal it saw move.
//
// That answers, one wire at a time:
//   nothing reported  -> the wire is broken, unplugged, or the module pad is
//                        not actually soldered
//   wrong name reported -> two wires are swapped
//   several at once   -> a short between module pads
//
// It also flags any signal already low at start-up, which means it is wired to
// ground somewhere it should not be — a fault that keeps the panel silent no
// matter what the firmware does.
//
// Deliberately free of TFT_eSPI: the point is to prove the wire, not the driver.

#include <Arduino.h>

#include "config.h"

namespace
{

struct TestPin
{
  const char *name;    // silkscreen name on the module
  const char *role;
  uint8_t     pin;
  uint8_t     header;  // physical pin on the Pico header
  bool        was_low;
};

// Listed in the module's own pin order, so the serial output reads like the
// connector looks.
TestPin g_pins[] = {
    {"SCL", "SPI clock",    SPI0_SCK_PIN,  24, false},
    {"SDA", "SPI data",     SPI0_MOSI_PIN, 25, false},
    {"RES", "reset",        TFT_RST_PIN,   26, false},
    {"DC ", "data/command", TFT_DC_PIN,    22, false},
    {"CS ", "chip select",  TFT_CS_PIN,    21, false},
};

constexpr uint8_t kPinCount = sizeof(g_pins) / sizeof(g_pins[0]);
constexpr uint32_t kDebounceMs = 40;

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(2000);

  for (uint8_t i = 0; i < kPinCount; i++)
  {
    pinMode(g_pins[i].pin, INPUT_PULLUP);
  }
  delay(10);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  Display wiring — continuity check");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Expected wiring:");
  for (uint8_t i = 0; i < kPinCount; i++)
  {
    Serial.printf("  %s  ->  GP%-2u  (header pin %u)\n",
                  g_pins[i].name, g_pins[i].pin, g_pins[i].header);
  }
  Serial.println("  GND  ->  header pin 23");
  Serial.println("  VCC  ->  header pin 36  (3V3, NOT pin 40)");
  Serial.println();

  // A signal already low means it is tied to ground somewhere. The panel can
  // never work like that, so it is worth saying before anything else.
  bool fault = false;
  for (uint8_t i = 0; i < kPinCount; i++)
  {
    if (digitalRead(g_pins[i].pin) == LOW)
    {
      Serial.printf("!! %s is ALREADY LOW — shorted to ground somewhere\n",
                    g_pins[i].name);
      fault = true;
    }
  }
  if (!fault)
  {
    Serial.println("All five signals idle high, as they should.");
  }

  Serial.println();
  Serial.println("Now take a wire from any GND pin and touch each pad on the");
  Serial.println("MODULE side, one at a time. Probing the module end is what");
  Serial.println("tells a broken wire from a wrong pin.");
  Serial.println();
  Serial.println("Waiting...");
  Serial.println();
}

void loop()
{
  for (uint8_t i = 0; i < kPinCount; i++)
  {
    bool low = (digitalRead(g_pins[i].pin) == LOW);

    if (low != g_pins[i].was_low)
    {
      delay(kDebounceMs);
      low = (digitalRead(g_pins[i].pin) == LOW);
      if (low == g_pins[i].was_low)
      {
        continue;
      }

      g_pins[i].was_low = low;
      if (low)
      {
        Serial.printf("  touched: %s  (%s, GP%u, header pin %u)\n",
                      g_pins[i].name, g_pins[i].role, g_pins[i].pin,
                      g_pins[i].header);
      }
    }
  }
}
