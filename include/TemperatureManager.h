#ifndef TEMPERATURE_MANAGER_H
#define TEMPERATURE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "ElectricHeater.h"
#include "HydraulicHeater.h"
#include "PIDController.h"

enum class OperatingMode : uint8_t
{
  ECO         = 0,  // Hydraulic only, reduced target (physical selector)
  PERFORMANCE = 1,  // All heaters, full target
};

// Temperature control parameters (all initialised from config.h defaults).
struct TemperatureParams
{
  float temperature_target;

  float hydraulic_kp;
  float hydraulic_ki;
  float hydraulic_kd;

  float electric_kp;
  float electric_ki;
  float electric_kd;

  float pid_integral_max;
  float pid_derivative_filter;

  TemperatureParams()
      : temperature_target(DEFAULT_TEMPERATURE_TARGET),
        hydraulic_kp(DEFAULT_HYDRAULIC_KP),
        hydraulic_ki(DEFAULT_HYDRAULIC_KI),
        hydraulic_kd(DEFAULT_HYDRAULIC_KD),
        electric_kp(DEFAULT_ELECTRIC_KP),
        electric_ki(DEFAULT_ELECTRIC_KI),
        electric_kd(DEFAULT_ELECTRIC_KD),
        pid_integral_max(DEFAULT_PID_INTEGRAL_MAX),
        pid_derivative_filter(DEFAULT_PID_DERIVATIVE_FILTER) {}
};

// Manages temperature via independent PID controllers for hydraulic and electric heaters.
// Operating mode is set from the physical mode selector (no time-based scheduling).
//   ECO mode:         hydraulic only, target reduced by DEFAULT_ECO_NIGHT_TARGET_PERCENTAGE
//   PERFORMANCE mode: both heaters, full target
class TemperatureManager
{
public:
  TemperatureManager(ElectricHeater *electric_heater, HydraulicHeater *hydraulic_heater);

  void Begin();
  void Update(float current_temperature);

  // Temperature target (overridden at runtime by potentiometer)
  void  SetTargetTemperature(float temperature);
  float GetTargetTemperature() const { return params_.temperature_target; }

  // Returns the currently active setpoint (reduced in ECO mode)
  float GetEffectiveTargetTemperature() const;

  bool IsTemperatureInRange() const;

  // Parameters
  TemperatureParams       &GetParams()       { return params_; }
  const TemperatureParams &GetParams() const { return params_; }

  // Heater access
  ElectricHeater  *GetElectricHeater()  { return electric_heater_; }
  HydraulicHeater *GetHydraulicHeater() { return hydraulic_heater_; }

  // PID access (for logging/monitoring)
  PIDController *GetHydraulicPID() { return &hydraulic_pid_; }
  PIDController *GetElectricPID()  { return &electric_pid_; }

  // Heater enable/disable
  void SetHydraulicEnabled(bool enabled);
  bool GetHydraulicEnabled() const { return hydraulic_enabled_; }
  void SetElectricEnabled(bool enabled);
  bool GetElectricEnabled() const { return electric_enabled_; }

  // Operating mode (set from physical MODE_SELECTOR_PIN each cycle)
  void          SetOperatingMode(OperatingMode mode);
  OperatingMode GetOperatingMode() const { return operating_mode_; }
  bool          IsEcoActive() const { return operating_mode_ == OperatingMode::ECO; }

private:
  ElectricHeater   *electric_heater_;
  HydraulicHeater  *hydraulic_heater_;
  TemperatureParams params_;

  PIDController hydraulic_pid_;
  PIDController electric_pid_;

  float    current_temperature_;
  uint32_t last_update_ms_;

  bool hydraulic_enabled_;
  bool electric_enabled_;

  OperatingMode operating_mode_;

  void UpdateHeating(float dt);
};

#endif // TEMPERATURE_MANAGER_H
