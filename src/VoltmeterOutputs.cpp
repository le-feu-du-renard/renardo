#include "VoltmeterOutputs.h"
#include "config.h"
#include "Logger.h"

static constexpr uint8_t  kPwmBits   = 12;
static constexpr uint16_t kPwmMax    = (1u << kPwmBits) - 1; // 4095
static constexpr uint32_t kPwmFreqHz = 50000;


VoltmeterOutputs::VoltmeterOutputs() {}

void VoltmeterOutputs::Begin()
{
  const uint8_t pins[] = {
      VOLTMETER_INLET_TEMPERATURE_PIN,
      VOLTMETER_INLET_HUMIDITY_PIN,
      VOLTMETER_OUTLET_TEMPERATURE_PIN,
      VOLTMETER_OUTLET_HUMIDITY_PIN,
  };

  analogWriteFreq(kPwmFreqHz);
  analogWriteResolution(kPwmBits);

  for (uint8_t pin : pins)
  {
    pinMode(pin, OUTPUT);
    analogWrite(pin, 0);
  }

  Logger::Info("VoltmeterOutputs: PWM ready at %lu Hz, %d-bit resolution", kPwmFreqHz, kPwmBits);
}

void VoltmeterOutputs::SetInletTemperature(float celsius)
{
  float ratio = constrain(celsius / VOLTMETER_TEMPERATURE_MAX, 0.0f, 1.0f);
  WriteRatio(VOLTMETER_INLET_TEMPERATURE_PIN, ratio, VOLTMETER_V1_TEMP_IN_MIN,  VOLTMETER_V1_TEMP_IN_MAX);
}

void VoltmeterOutputs::SetInletHumidity(float percent)
{
  float ratio = constrain(percent / VOLTMETER_HUMIDITY_MAX, 0.0f, 1.0f);
  WriteRatio(VOLTMETER_INLET_HUMIDITY_PIN,    ratio, VOLTMETER_V2_HUM_IN_MIN,   VOLTMETER_V2_HUM_IN_MAX);
}

void VoltmeterOutputs::SetOutletTemperature(float celsius)
{
  float ratio = constrain(celsius / VOLTMETER_TEMPERATURE_MAX, 0.0f, 1.0f);
  WriteRatio(VOLTMETER_OUTLET_TEMPERATURE_PIN, ratio, VOLTMETER_V3_TEMP_OUT_MIN, VOLTMETER_V3_TEMP_OUT_MAX);
}

void VoltmeterOutputs::SetOutletHumidity(float percent)
{
  float ratio = constrain(percent / VOLTMETER_HUMIDITY_MAX, 0.0f, 1.0f);
  WriteRatio(VOLTMETER_OUTLET_HUMIDITY_PIN,   ratio, VOLTMETER_V4_HUM_OUT_MIN,  VOLTMETER_V4_HUM_OUT_MAX);
}

void VoltmeterOutputs::WriteRatio(uint8_t pin, float ratio, int16_t val_min, uint16_t val_max)
{
  int32_t raw = val_min + static_cast<int32_t>(ratio * (val_max - val_min));
  analogWrite(pin, static_cast<uint16_t>(constrain(raw, 0, kPwmMax)));
}
