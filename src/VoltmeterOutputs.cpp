#include "VoltmeterOutputs.h"
#include "config.h"
#include "Logger.h"

// PWM resolution: 12 bits (0-4095) for smooth analog output
static constexpr uint8_t  kPwmBits   = 12;
static constexpr uint16_t kPwmMax    = (1u << kPwmBits) - 1;  // 4095
static constexpr uint32_t kPwmFreqHz = 50000;                   // 50 kHz

VoltmeterOutputs::VoltmeterOutputs() {}

void VoltmeterOutputs::Begin()
{
  const uint8_t pins[] = {
    VOLTMETER_CH1_TEMPERATURE_PIN,
    VOLTMETER_CH2_HUMIDITY_PIN,
    VOLTMETER_CH3_TOTAL_DURATION_PIN,
    VOLTMETER_CH4_PHASE_DURATION_PIN,
  };

  for (uint8_t pin : pins)
  {
    pinMode(pin, OUTPUT);
    analogWriteFreq(kPwmFreqHz);
    analogWriteResolution(kPwmBits);
    analogWrite(pin, 0);
  }

  Logger::Info("VoltmeterOutputs: PWM ready at %lu Hz, %d-bit resolution", kPwmFreqHz, kPwmBits);
}

void VoltmeterOutputs::SetTemperature(float celsius)
{
  WriteDuty(VOLTMETER_CH1_TEMPERATURE_PIN, ValueToDuty(celsius, VOLTMETER_TEMP_MAX));
}

void VoltmeterOutputs::SetHumidity(float percent)
{
  WriteDuty(VOLTMETER_CH2_HUMIDITY_PIN, ValueToDuty(percent, VOLTMETER_HUM_MAX));
}

void VoltmeterOutputs::SetTotalDuration(float seconds)
{
  float hours = seconds / 3600.0f;
  WriteDuty(VOLTMETER_CH3_TOTAL_DURATION_PIN, ValueToDuty(hours, VOLTMETER_TOTAL_DUR_H));
}

void VoltmeterOutputs::SetPhaseDuration(float seconds)
{
  float minutes = seconds / 60.0f;
  WriteDuty(VOLTMETER_CH4_PHASE_DURATION_PIN, ValueToDuty(minutes, VOLTMETER_PHASE_DUR_MIN));
}

void VoltmeterOutputs::WriteDuty(uint8_t pin, float duty)
{
  duty = constrain(duty, 0.0f, kDutyMax);
  analogWrite(pin, static_cast<int>(duty * kPwmMax));
}

float VoltmeterOutputs::ValueToDuty(float value, float max_value)
{
  if (max_value <= 0.0f) return 0.0f;
  float ratio = constrain(value / max_value, 0.0f, 1.0f);
  return ratio * kDutyMax;
}
