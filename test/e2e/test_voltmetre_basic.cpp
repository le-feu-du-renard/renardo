#include <Arduino.h>
#include <hardware/gpio.h>
#include "config.h"

// =========================================================
// Voltmeter basic PWM test — no RC filter needed
//
// Wiring:
//   GPIO (TEST_PIN) ── Oscilloscope or multimeter probe
//
// Outputs a random duty cycle at 50 kHz / 12-bit,
// changing every 5 seconds.
// =========================================================

static constexpr uint8_t TEST_PIN = 21;
static constexpr uint8_t kPwmBits = 12;
static constexpr uint32_t kPwmFreqHz = 50000;
static constexpr uint16_t kPwmMax = (1u << kPwmBits) - 1; // 4095
static constexpr float kVcc = 3.3f;
static constexpr float kDutyMax = 3.0f / kVcc; // ~0.909
static constexpr uint32_t kIntervalMs = 5000;

void applyRandom()
{
  uint8_t percent = random(0, 101); // 0–100 %
  float duty = (percent / 100.0f) * kDutyMax;
  uint16_t raw = static_cast<uint16_t>(duty * kPwmMax);
  float expected = duty * kVcc;

  analogWrite(TEST_PIN, raw);

  Serial.print("Duty: ");
  Serial.print(percent);
  Serial.print(" %  →  expected ");
  Serial.print(expected, 3);
  Serial.println(" V");
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  analogWriteFreq(kPwmFreqHz);
  analogWriteResolution(kPwmBits);
  pinMode(TEST_PIN, OUTPUT);
  gpio_pull_up(TEST_PIN);

  Serial.println("\n=== Voltmeter random PWM test ===");
  Serial.print("Pin       : GPIO ");
  Serial.println(TEST_PIN);
  Serial.print("Frequency : ");
  Serial.print(kPwmFreqHz / 1000);
  Serial.println(" kHz");
  Serial.print("Resolution: ");
  Serial.print(kPwmBits);
  Serial.println(" bits");
  Serial.println("Changing every 5 s. Probe GPIO 15 with a multimeter (DC mode).");
  Serial.println();

  randomSeed(micros());
  applyRandom();
}

void loop()
{
  delay(kIntervalMs);
  applyRandom();
}
