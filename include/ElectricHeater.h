#ifndef ELECTRIC_HEATER_H
#define ELECTRIC_HEATER_H

#include <Arduino.h>

/**
 * Electric heater controller
 * Binary control (on/off only)
 */
class ElectricHeater {
 public:
  ElectricHeater();

  void Begin();
  void Update();

  // Power control (0 or 1)
  void SetPower(float power);
  float GetPower() const { return power_; }

  // Output control
  float GetOutput() const { return power_ > 0.5f ? 1.0f : 0.0f; }

 private:
  float power_;  // 0.0 or 1.0
};

#endif  // ELECTRIC_HEATER_H
