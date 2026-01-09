#ifndef HEATERS_MANAGER_H
#define HEATERS_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "ElectricHeater.h"
#include "HydraulicHeater.h"

/**
 * Heating parameters
 */
struct HeatingParams {
  float temperature_target;           // 20-80°C
  float temperature_deadband;         // 0.5-10°C
  float heating_action_min_wait_s;    // 5-120s
  float heater_step_min;              // 0.01-0.5 ratio
  float heater_step_max;              // 0.05-1.0 ratio
  float heater_full_scale_delta;      // 5-30°C

  HeatingParams()
    : temperature_target(DEFAULT_TEMPERATURE_TARGET),
      temperature_deadband(DEFAULT_TEMPERATURE_DEADBAND),
      heating_action_min_wait_s(DEFAULT_HEATING_ACTION_MIN_WAIT),
      heater_step_min(DEFAULT_HEATER_STEP_MIN),
      heater_step_max(DEFAULT_HEATER_STEP_MAX),
      heater_full_scale_delta(DEFAULT_HEATER_FULL_SCALE_DELTA) {}
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

  bool IncreaseHeating();
  bool DecreaseHeating();

  // Adaptive step calculation
  float CalculateStep() const;
};

#endif  // HEATERS_MANAGER_H
