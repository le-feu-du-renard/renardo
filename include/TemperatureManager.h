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

// Control state for the heating state machine.
enum class ControlState : uint8_t
{
  REGULATION    = 0,  // PID drives hydraulic; electric OFF
  BOOST         = 1,  // Hydraulic forced to 100%; electric ON
  ELECTRIC_ONLY = 2,  // Hydraulic disabled by user; electric follows ON/OFF hysteresis
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

// Manages temperature via a single PID (hydraulic) + electric state machine.
//
// Architecture:
//   REGULATION  — PID drives hydraulic (0–100%); electric stays OFF.
//   BOOST       — Triggered when hydraulic alone cannot close the gap fast enough.
//                 Hydraulic forced to 100%, electric ON.
//                 Exits once setpoint is nearly reached (with anti-short-cycle guard).
//   ELECTRIC_ONLY — hydraulic_available_ == false (user-disabled).
//                 Hydraulic hard-guarded to 0; electric regulated by simple hysteresis.
//
// Invariant: when electric is ON and hydraulic is available, hydraulic is at 100%.
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

  // Returns the currently active setpoint (reduced in ECO mode during night window)
  float GetEffectiveTargetTemperature() const;

  bool IsTemperatureInRange() const;

  // Parameters
  TemperatureParams       &GetParams()       { return params_; }
  const TemperatureParams &GetParams() const { return params_; }

  // Heater access (for LEDs / monitoring)
  ElectricHeater  *GetElectricHeater()  { return electric_heater_; }
  HydraulicHeater *GetHydraulicHeater() { return hydraulic_heater_; }

  // PID access (for logging/monitoring)
  PIDController *GetPID() { return &pid_; }

  // Hydraulic availability — compile-time default, overridable at runtime.
  // False → ELECTRIC_ONLY mode; electric continues without resetting its timers.
  void SetHydraulicAvailable(bool available);
  bool GetHydraulicAvailable() const { return hydraulic_available_; }

  // Electric heater enable/disable (sensor-timeout safety guard in main.cpp).
  // False → all heating off; PID frozen. Timers preserved for anti-short-cycle.
  void SetElectricEnabled(bool enabled);
  bool GetElectricEnabled() const { return electric_enabled_; }

  // Fan active flag — both heaters blocked when fan is not running.
  void SetFanActive(bool active);
  bool GetFanActive() const { return fan_active_; }

  // Electric heater current state (for LEDs / monitoring)
  bool  GetElectricOn()      const { return electric_on_; }
  float GetElectricOnTimer() const { return elec_on_timer_; }

  // Current control state (for logging / monitoring)
  ControlState GetControlState() const { return control_state_; }

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

  // Print current control state and heater status to logger (for tuning / debug)
  void PrintDebug() const;

private:
  ElectricHeater   *electric_heater_;
  HydraulicHeater  *hydraulic_heater_;
  TemperatureParams params_;

  PIDController pid_;  // Drives hydraulic (0–100%), output frozen during BOOST

  float    current_temperature_;
  uint32_t last_update_ms_;

  bool hydraulic_available_;  // User toggle: false → ELECTRIC_ONLY
  bool electric_enabled_;     // Sensor-timeout guard: false → all heating off
  bool fan_active_;           // Fan interlock: false → all heating blocked
  bool electric_on_;          // Current state of the electric relay

  ControlState control_state_;  // Current state machine state

  float dT_dt_;           // Filtered temperature derivative (°C/s), positive when rising
  float prev_temp_;       // Previous temperature for derivative computation
  bool  first_tick_;      // Skip derivative on the very first tick

  float hydro_sat_timer_; // Seconds hydraulic has been continuously at ≥99%
  float elec_on_timer_;   // Seconds electric has been continuously ON (this cycle)
  float elec_off_timer_;  // Seconds electric has been continuously OFF (this cycle)

  float debug_log_timer_s_;

  OperatingMode operating_mode_;
  uint8_t       current_hour_;

  // Switch electric relay state; resets the appropriate timer for anti-short-cycle.
  void SetElectric(bool on);

  // Force both actuators off without changing state machine or timers.
  void ForceAllOff();

  void UpdateHeating(float dt);
};

#endif // TEMPERATURE_MANAGER_H
