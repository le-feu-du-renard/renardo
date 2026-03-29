#include <Arduino.h>
#include "config.h"

// =========================================================
// Voltmeter RC filter test
//
// Wiring for this test:
//   GPIO (TEST_PIN) ── R ── ┬── Voltmeter (+)
//                           C (to GND)
//                           └── GND
//
// Adjust R_OHMS to match the resistor you are testing.
// C_UF should match the capacitor (default 100 µF 16V).
//
// The test cycles through 0 / 25 / 50 / 75 / 91 % duty,
// holding each step for 5 × tau so the filter fully settles.
// Use a multimeter or the voltmeter needle to verify the
// expected voltage at each step.
// =========================================================

// --- Change these to match your test circuit ---
static constexpr uint8_t TEST_PIN = 21;
static constexpr uint32_t R_OHMS = 10000; // resistor under test (ohms)
static constexpr float C_UF = 100.0f;     // capacitor (µF)
// -----------------------------------------------

// PWM settings (must match VoltmeterOutputs)
static constexpr uint8_t kPwmBits = 12;
static constexpr uint32_t kPwmFreqHz = 50000;
static constexpr uint16_t kPwmMax = (1u << kPwmBits) - 1; // 4095
static constexpr float kVcc = 3.3f;
static constexpr float kDutyMax = 3.0f / kVcc; // 0.909

// Derived timing
static constexpr float kTauMs = R_OHMS * (C_UF / 1e6f) * 1000.0f;
static constexpr uint32_t kSettleMs = static_cast<uint32_t>(kTauMs * 5.0f);

// Sweep steps: duty 0-100% of kDutyMax
static const uint8_t kSteps[] = {0, 25, 50, 75, 100};
static const uint8_t kNumSteps = sizeof(kSteps) / sizeof(kSteps[0]);

static uint8_t step_index = 0;
static uint32_t step_start = 0;
static bool auto_mode = true;
static uint8_t manual_duty = 0; // 0-100

// ---- helpers ----

static void SetDuty(uint8_t percent)
{
  float duty = (percent / 100.0f) * kDutyMax;
  uint16_t raw = static_cast<uint16_t>(duty * kPwmMax);
  analogWrite(TEST_PIN, raw);
}

static float DutyToVoltage(uint8_t percent)
{
  return (percent / 100.0f) * kDutyMax * kVcc;
}

static void PrintStatus(uint8_t percent)
{
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms]  duty=");
  Serial.print(percent);
  Serial.print("%  expected=");
  Serial.print(DutyToVoltage(percent), 3);
  Serial.print(" V  (settle in ");
  Serial.print(kSettleMs);
  Serial.println(" ms)");
}

static void PrintHelp()
{
  Serial.println("Commands:");
  Serial.println("  a        : auto sweep mode (default)");
  Serial.println("  0..100   : manual duty cycle (% of full scale)");
  Serial.println("  ?        : print this help");
  Serial.println();
}

// ---- setup / loop ----

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n=== Voltmeter RC filter test ===");
  Serial.print("Pin       : GPIO ");
  Serial.println(TEST_PIN);
  Serial.print("R         : ");
  Serial.print(R_OHMS);
  Serial.println(" ohms");
  Serial.print("C         : ");
  Serial.print(C_UF, 0);
  Serial.println(" uF");
  Serial.print("tau       : ");
  Serial.print(kTauMs, 0);
  Serial.println(" ms");
  Serial.print("Settle (5tau): ");
  Serial.print(kSettleMs);
  Serial.println(" ms");
  Serial.print("fc        : ");
  Serial.print(1000.0f / (6.2832f * kTauMs), 3);
  Serial.println(" Hz");
  Serial.println();
  PrintHelp();

  analogWriteFreq(kPwmFreqHz);
  analogWriteResolution(kPwmBits);
  pinMode(TEST_PIN, OUTPUT);
  analogWrite(TEST_PIN, 0);

  step_index = 0;
  step_start = millis();
  SetDuty(kSteps[0]);
  PrintStatus(kSteps[0]);
}

void loop()
{
  // --- Serial input ---
  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "a")
    {
      auto_mode = true;
      step_index = 0;
      step_start = millis();
      SetDuty(kSteps[0]);
      Serial.println(">> Auto sweep mode");
      PrintStatus(kSteps[0]);
    }
    else if (input == "?")
    {
      PrintHelp();
    }
    else
    {
      int val = input.toInt();
      if (val >= 0 && val <= 100)
      {
        auto_mode = false;
        manual_duty = static_cast<uint8_t>(val);
        SetDuty(manual_duty);
        Serial.print(">> Manual: ");
        PrintStatus(manual_duty);
      }
      else
      {
        Serial.println(">> Unknown command. Type ? for help.");
      }
    }
  }

  // --- Auto sweep ---
  if (auto_mode)
  {
    uint32_t now = millis();
    if (now - step_start >= kSettleMs)
    {
      step_index = (step_index + 1) % kNumSteps;
      step_start = now;
      SetDuty(kSteps[step_index]);
      PrintStatus(kSteps[step_index]);
    }
  }
}
