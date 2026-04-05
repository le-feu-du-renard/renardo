#include <Arduino.h>
#include "config.h"

// =========================================================
// Test registre PWM — pin GPIO 15 (WATER_CIRCULATOR_PWM_PIN)
// Alterne entre 2V (~7%) et 10V (~93%) toutes les 3 s
// =========================================================

static constexpr uint8_t kPwmPin = WATER_CIRCULATOR_PWM_PIN; // GPIO 15
static constexpr uint8_t kPwmLow = 50;                       // à calibrer
static constexpr uint8_t kPwmHigh = 95;                      // ~10V
static constexpr uint32_t kDelayMs = 3000;

static void SetPercent(uint8_t pct)
{
  uint8_t raw = static_cast<uint8_t>((pct / 100.0f) * 255);
  analogWrite(kPwmPin, raw);
  Serial.print("PWM : ");
  Serial.print(pct);
  Serial.println("%");
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  pinMode(kPwmPin, OUTPUT);
  Serial.println("=== Test registre PWM ===");

  SetPercent(kPwmLow);
  // SetPercent(kPwmHigh);
}

void loop()
{
  // SetPercent(kPwmLow);
  // delay(kDelayMs);
  // SetPercent(kPwmHigh);
  // delay(kDelayMs);
}
