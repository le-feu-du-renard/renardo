#include "HydraulicHeater.h"

HydraulicHeater::HydraulicHeater()
  : power_(100.0f) {
}

void HydraulicHeater::Begin() {
  power_ = 100.0f;
  Serial.println("Hydraulic heater initialized");
}

void HydraulicHeater::Update() {
  // Nothing to update for now
}

void HydraulicHeater::SetPower(float power) {
  // Clamp to 0-100%
  if (power < 0.0f) power = 0.0f;
  if (power > 100.0f) power = 100.0f;

  power_ = power;
}

float HydraulicHeater::GetOutput() const {
  return MapPowerToPwm(power_);
}

float HydraulicHeater::MapPowerToPwm(float power) const {
  // Reverse: 100% power = fast circulation = low PWM
  float reversed_power = 100.0f - power;

  // Map to hardware range (10% = max speed, 85% = min speed)
  float mapped = kBandMin + (reversed_power / 100.0f) * (kBandMax - kBandMin);

  // Convert to 0-1 range for PWM output
  return mapped / 100.0f;
}
