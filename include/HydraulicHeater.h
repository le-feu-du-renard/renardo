#ifndef HYDRAULIC_HEATER_H
#define HYDRAULIC_HEATER_H

#include <Arduino.h>

/**
 * Hydraulic heater controller
 * Controls water circulator with PWM mapping
 */
class HydraulicHeater {
 public:
  HydraulicHeater();

  void Begin();
  void Update();

  // Power control (0-100%)
  void SetPower(uint8_t power);
  uint8_t GetPower() const { return power_; }

  // PWM output (0.0-1.0) with hardware mapping
  float GetOutput() const;

 private:
  uint8_t power_;  // 0-100%

  // PWM hardware mapping constants
  static constexpr float kBandMin = 10.0f;  // Maximum speed PWM%
  static constexpr float kBandMax = 85.0f;  // Minimum speed PWM%

  float MapPowerToPwm(float power) const;
};

#endif  // HYDRAULIC_HEATER_H
