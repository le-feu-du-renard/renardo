#ifndef HEATERS_MANAGER_H
#define HEATERS_MANAGER_H

#include <Arduino.h>
#include "ElectricHeater.h"
#include "HydraulicHeater.h"

/**
 * Heating parameters
 */
struct HeatingParams {
  float temperature_target;           // 20-45°C
  float temperature_deadband;         // 0-2°C
  float heating_action_min_wait_s;    // 1-120s
  float heater_step_min;              // 1-10%
  float heater_step_max;              // 2-10%
  float heater_full_scale_delta;      // 10-30°C

  HeatingParams()
    : temperature_target(40.0f),
      temperature_deadband(0.5f),
      heating_action_min_wait_s(10.0f),
      heater_step_min(1.0f),
      heater_step_max(10.0f),
      heater_full_scale_delta(30.0f) {}
};

/**
 * Manages both electric and hydraulic heaters
 * Implements adaptive heating control with deadband
 */
class HeatersManager {
 public:
  HeatersManager(ElectricHeater* electric_heater, HydraulicHeater* hydraulic_heater);

  void Begin();
  void Update(float current_temperature);

  // Temperature control
  void SetTargetTemperature(float temperature);
  float GetTargetTemperature() const { return params_.temperature_target; }

  // Parameters
  HeatingParams& GetParams() { return params_; }
  const HeatingParams& GetParams() const { return params_; }

  // Heater access
  ElectricHeater* GetElectricHeater() { return electric_heater_; }
  HydraulicHeater* GetHydraulicHeater() { return hydraulic_heater_; }

  // Action cooldown control
  void ResetCooldown();

 private:
  ElectricHeater* electric_heater_;
  HydraulicHeater* hydraulic_heater_;
  HeatingParams params_;

  unsigned long heating_action_next_allowed_ms_;
  float current_temperature_;

  // Temperature validation
  bool IsTemperatureTooHigh() const;
  bool IsTemperatureTooLow() const;
  bool IsTemperatureInRange() const;

  // Heating actions
  bool IsHeatingActionAllowed() const;
  void ArmHeatingActionCooldown();

  void IncreaseHeating();
  void DecreaseHeating();

  // Adaptive step calculation
  float CalculateStep() const;
};

#endif  // HEATERS_MANAGER_H
