#ifndef TEMPERATURE_MANAGER_H
#define TEMPERATURE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "ElectricHeater.h"
#include "HydraulicHeater.h"

/**
 * Temperature control parameters
 */
struct TemperatureParams {
  float temperature_target;           // 20-80°C
  float temperature_deadband;         // 0.5-10°C
  float heating_action_min_wait_s;    // 5-120s
  float heater_step_min;              // 0.01-0.5 ratio
  float heater_step_max;              // 0.05-1.0 ratio
  float heater_full_scale_delta;      // 5-30°C

  TemperatureParams()
    : temperature_target(DEFAULT_TEMPERATURE_TARGET),
      temperature_deadband(DEFAULT_TEMPERATURE_DEADBAND),
      heating_action_min_wait_s(DEFAULT_HEATING_ACTION_MIN_WAIT),
      heater_step_min(DEFAULT_HEATER_STEP_MIN),
      heater_step_max(DEFAULT_HEATER_STEP_MAX),
      heater_full_scale_delta(DEFAULT_HEATER_FULL_SCALE_DELTA) {}
};

// Backward compatibility alias
typedef TemperatureParams HeatingParams;

/**
 * Manages temperature control via electric and hydraulic heaters
 * Implements adaptive heating control with deadband
 */
class TemperatureManager {
 public:
  TemperatureManager(ElectricHeater* electric_heater, HydraulicHeater* hydraulic_heater);

  void Begin();
  void Update(float current_temperature);

  // Temperature control
  void SetTargetTemperature(float temperature);
  float GetTargetTemperature() const { return params_.temperature_target; }

  // Check if temperature is in target range
  bool IsTemperatureInRange() const;

  // Parameters
  TemperatureParams& GetParams() { return params_; }
  const TemperatureParams& GetParams() const { return params_; }

  // Heater access
  ElectricHeater* GetElectricHeater() { return electric_heater_; }
  HydraulicHeater* GetHydraulicHeater() { return hydraulic_heater_; }

  // Action cooldown control
  void ResetCooldown();

  // Energy source enable/disable
  void SetHydraulicEnabled(bool enabled) { hydraulic_enabled_ = enabled; }
  bool GetHydraulicEnabled() const { return hydraulic_enabled_; }
  void SetElectricEnabled(bool enabled) { electric_enabled_ = enabled; }
  bool GetElectricEnabled() const { return electric_enabled_; }

 private:
  ElectricHeater* electric_heater_;
  HydraulicHeater* hydraulic_heater_;
  TemperatureParams params_;

  unsigned long heating_action_next_allowed_ms_;
  float current_temperature_;

  bool hydraulic_enabled_;
  bool electric_enabled_;

  // Temperature validation
  bool IsTemperatureTooHigh() const;
  bool IsTemperatureTooLow() const;

  // Heating actions
  bool IsHeatingActionAllowed() const;
  void ArmHeatingActionCooldown();

  bool IncreaseHeating();
  bool DecreaseHeating();

  // Adaptive step calculation
  float CalculateStep() const;
};

// Backward compatibility alias
typedef TemperatureManager HeatersManager;

#endif  // TEMPERATURE_MANAGER_H
