#ifndef TEMPERATURE_MANAGER_H
#define TEMPERATURE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "ElectricHeater.h"
#include "HydraulicHeater.h"
#include "PIDController.h"

enum class OperatingMode : uint8_t
{
  ECO         = 0,  // Target reduced to 85% during night window (18h–9h)
  PERFORMANCE = 1,  // Full target at all times
};

// Temperature control parameters (all initialised from config.h defaults).
struct TemperatureParams
{
  float temperature_target;

  float hydraulic_kp;
  float hydraulic_ki;
  float hydraulic_kd;

  float pid_integral_max;
  float pid_derivative_filter;

  TemperatureParams()
      : temperature_target(TEMPERATURE_TARGET),
        hydraulic_kp(HYDRAULIC_KP),
        hydraulic_ki(HYDRAULIC_KI),
        hydraulic_kd(HYDRAULIC_KD),
        pid_integral_max(PID_INTEGRAL_MAX),
        pid_derivative_filter(PID_DERIVATIVE_FILTER) {}
};

// Manages temperature via a single split-range PID controller.
//
// The hydraulic source is manual (not software-controlled). hydraulic_available_
// selects the active heat source strategy — call SetHydraulicAvailable() at runtime:
//   - PRIMARY_HYDRO (hydraulic_available = true):  hydraulic is primary, electric supplements above demand threshold
//   - PRIMARY_ELEC  (hydraulic_available = false): electric is primary, activates as soon as there is demand
//
// Operating mode controls the effective setpoint:
//   ECO mode:         target × ECO_NIGHT_TARGET_PERCENTAGE during night window (18h–9h)
//   PERFORMANCE mode: full target at all times
//
// SetCurrentHour() must be called each loop (from RTC) for time-based ECO logic.
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

  // Heater access (for LEDs / monitoring — hydraulic is not driven by this manager)
  ElectricHeater  *GetElectricHeater()  { return electric_heater_; }
  HydraulicHeater *GetHydraulicHeater() { return hydraulic_heater_; }

  // PID access (for logging/monitoring)
  PIDController *GetPID() { return &pid_; }

  // Hydraulic availability — compile-time default, overridable at runtime (future UI switch)
  void SetHydraulicAvailable(bool available);
  bool GetHydraulicAvailable() const { return hydraulic_available_; }

  // Electric heater enable/disable (used by sensor-timeout safety in main.cpp)
  void SetElectricEnabled(bool enabled);
  bool GetElectricEnabled() const { return electric_enabled_; }

  // Fan active flag — electric heater is blocked when fan is not running
  void SetFanActive(bool active);
  bool GetFanActive() const { return fan_active_; }

  // Electric heater current state (for LEDs / monitoring)
  bool  GetElectricOn()      const { return electric_on_; }
  float GetElectricOnTimer() const { return electric_on_timer_s_; }

  // Operating mode (set from physical MODE_SELECTOR_PIN each cycle)
  void          SetOperatingMode(OperatingMode mode);
  OperatingMode GetOperatingMode() const { return operating_mode_; }
  bool          IsEcoActive() const { return operating_mode_ == OperatingMode::ECO; }

  // Current hour from RTC — must be updated each loop for time-based ECO logic
  void SetCurrentHour(uint8_t hour) { current_hour_ = hour; }

  // Returns true when ECO switch is ON and current time is inside the night window
  bool IsEcoWindowActive() const;

  // Reset PID and all electric heater timers — call on phase transitions
  void ResetControl();

  // Print current PID state and heater status to logger (for tuning / debug)
  void PrintDebug() const;

private:
  ElectricHeater   *electric_heater_;
  HydraulicHeater  *hydraulic_heater_;
  TemperatureParams params_;

  PIDController pid_;  // Single split-range PID, output 0–100%

  float    current_temperature_;
  uint32_t last_update_ms_;

  bool  hydraulic_available_;     // Is the hydraulic source providing heat?
  bool  electric_enabled_;        // Is electric heating authorised? (sensor-timeout guard)
  bool  fan_active_;              // Is the fan running? Electric heater blocked if false
  bool  electric_on_;             // Current state of the electric heater relay
  float electric_on_timer_s_;     // Accumulated time with active electric demand (normal mode)
  float electric_settle_timer_s_; // Counts down after heater ON — integral frozen during lag
  float debug_log_timer_s_;       // Accumulates dt to trigger a periodic debug log

  OperatingMode operating_mode_;
  uint8_t       current_hour_;

  void UpdateHeating(float dt);
};

#endif // TEMPERATURE_MANAGER_H
