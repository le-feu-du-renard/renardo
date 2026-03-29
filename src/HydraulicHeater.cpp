#include "HydraulicHeater.h"
#include "Logger.h"

HydraulicHeater::HydraulicHeater()
  : power_(100) {
}

void HydraulicHeater::Begin() {
  power_ = 100;
  Log.notice("Hydraulic heater initialized");
}

void HydraulicHeater::Update() {
  // Nothing to update for now
}

void HydraulicHeater::SetPower(uint8_t power) {
  // Clamp to 0-100%
  if (power > 100) power = 100;

  power_ = power;
}

float HydraulicHeater::GetOutput() const {
  return MapPowerToPwm(power_);
}

float HydraulicHeater::MapPowerToPwm(float power) const {
  if (power_ == 0) return 0.0f;  // Pump completely off

  // Reverse: 100% power = fast circulation = low PWM duty
  float reversed_power = 100.0f - (float)power_;

  // Map to hardware range (10% = max speed, 85% = min speed)
  float mapped = kBandMin + (reversed_power / 100.0f) * (kBandMax - kBandMin);

  // Convert to 0-1 range for PWM output
  return mapped / 100.0f;
}
