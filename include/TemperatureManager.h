#ifndef TEMPERATURE_MANAGER_H
#define TEMPERATURE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "ElectricHeater.h"
#include "HydraulicHeater.h"
#include "PIDController.h"

/**
 * Temperature control parameters
 */
struct TemperatureParams {
  float temperature_target;           // 20-80°C

  // PID parameters for hydraulic heater
  float hydraulic_kp;                 // Proportional gain (0.1-20)
  float hydraulic_ki;                 // Integral gain (0.0-2)
  float hydraulic_kd;                 // Derivative gain (0.0-5)

  // PID parameters for electric heater
  float electric_kp;                  // Proportional gain (0.1-20)
  float electric_ki;                  // Integral gain (0.0-2)
  float electric_kd;                  // Derivative gain (0.0-5)

  // PID advanced settings
  float pid_integral_max;             // Anti-windup limit (10-100)
  float pid_derivative_filter;        // Filter coefficient (0.01-0.5)

  // Water temperature constraint
  float water_temp_margin;            // °C - Minimum margin between water temp and air setpoint

  TemperatureParams()
    : temperature_target(DEFAULT_TEMPERATURE_TARGET),
      hydraulic_kp(DEFAULT_HYDRAULIC_KP),
      hydraulic_ki(DEFAULT_HYDRAULIC_KI),
      hydraulic_kd(DEFAULT_HYDRAULIC_KD),
      electric_kp(DEFAULT_ELECTRIC_KP),
      electric_ki(DEFAULT_ELECTRIC_KI),
      electric_kd(DEFAULT_ELECTRIC_KD),
      pid_integral_max(DEFAULT_PID_INTEGRAL_MAX),
      pid_derivative_filter(DEFAULT_PID_DERIVATIVE_FILTER),
      water_temp_margin(DEFAULT_WATER_TEMP_MARGIN) {}
};

// Backward compatibility alias
typedef TemperatureParams HeatingParams;

/**
 * Manages temperature control via electric and hydraulic heaters
 * Implements PID control with independent controllers for each heater
 */
class TemperatureManager {
 public:
  TemperatureManager(ElectricHeater* electric_heater, HydraulicHeater* hydraulic_heater);

  void Begin();
  void Update(float current_temperature);

  // Temperature control
  void SetTargetTemperature(float temperature);
  float GetTargetTemperature() const { return params_.temperature_target; }

  // Water temperature (for hydraulic heater constraint)
  void SetWaterTemperature(float temperature) { water_temperature_ = temperature; }
  float GetWaterTemperature() const { return water_temperature_; }

  // Check if temperature is in target range (legacy method)
  bool IsTemperatureInRange() const;

  // Parameters
  TemperatureParams& GetParams() { return params_; }
  const TemperatureParams& GetParams() const { return params_; }

  // Heater access
  ElectricHeater* GetElectricHeater() { return electric_heater_; }
  HydraulicHeater* GetHydraulicHeater() { return hydraulic_heater_; }

  // PID access (for debugging/monitoring)
  PIDController* GetHydraulicPID() { return &hydraulic_pid_; }
  PIDController* GetElectricPID() { return &electric_pid_; }

  // Energy source enable/disable
  void SetHydraulicEnabled(bool enabled);
  bool GetHydraulicEnabled() const { return hydraulic_enabled_; }
  void SetElectricEnabled(bool enabled);
  bool GetElectricEnabled() const { return electric_enabled_; }

 private:
  ElectricHeater* electric_heater_;
  HydraulicHeater* hydraulic_heater_;
  TemperatureParams params_;

  // PID controllers
  PIDController hydraulic_pid_;
  PIDController electric_pid_;

  // State
  float current_temperature_;
  float water_temperature_;
  unsigned long last_update_ms_;

  bool hydraulic_enabled_;
  bool electric_enabled_;

  // Control logic
  void UpdateHeating();
  bool IsWaterTemperatureValid() const;
};

// Backward compatibility alias
typedef TemperatureManager HeatersManager;

#endif  // TEMPERATURE_MANAGER_H
