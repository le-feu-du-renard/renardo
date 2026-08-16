#ifndef TEMPERATURE_MANAGER_H
#define TEMPERATURE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "ElectricHeater.h"

enum class OperatingMode : uint8_t
{
  ECO         = 0,  // Target reduced during the night window
  PERFORMANCE = 1,  // Full target at all times
};

// Which heat sources are actually usable this cycle. Reporting only — the two
// sources are regulated independently, there is no mode to switch between.
enum class ControlState : uint8_t
{
  OFF                = 0,  // no source usable (fault, safety, fan off, all disabled)
  ELECTRIC_ONLY      = 1,
  HYDRAULIC_ONLY     = 2,
  HYDRAULIC_ELECTRIC = 3,
};

// Temperature control parameters (all initialised from config.h defaults).
// Overridable at runtime from the menu and persisted by SettingsStore.
struct TemperatureParams
{
  float temperature_target;

  float band_hydraulic;      // °C, error above which the hydraulic is requested
  float band_electric;       // °C, error above which the electric is requested
  float horizon_hydraulic;   // s, predictive shutoff window, hydraulic
  float horizon_electric;    // s, predictive shutoff window, electric
  float hydraulic_t_on_min;  // s, anti-short-cycle
  float hydraulic_t_off_min; // s
  float electric_t_on_min;   // s
  float electric_t_off_min;  // s
  float safety_max;          // °C, hard cutoff

  uint8_t eco_start_hour;
  uint8_t eco_end_hour;
  float   eco_target_percentage;

  TemperatureParams()
      : temperature_target(TEMPERATURE_TARGET),
        band_hydraulic(CTRL_BANDE_HYDRO),
        band_electric(CTRL_BANDE_ELEC),
        horizon_hydraulic(CTRL_HYDRO_HORIZON),
        horizon_electric(CTRL_HORIZON),
        hydraulic_t_on_min(CTRL_HYDRO_T_ON_MIN),
        hydraulic_t_off_min(CTRL_HYDRO_T_OFF_MIN),
        electric_t_on_min(CTRL_T_ON_MIN),
        electric_t_off_min(CTRL_T_OFF_MIN),
        safety_max(TEMPERATURE_SAFETY_MAX),
        eco_start_hour(ECO_START_HOUR),
        eco_end_hour(ECO_END_HOUR),
        eco_target_percentage(ECO_NIGHT_TARGET_PERCENTAGE) {}
};

// Regulates the dryer air temperature with two independent on/off heat sources.
//
// Hydraulic — base heat, commanded on/off on a wide hysteresis band with long
//   minimum on/off times. The remote module holds a fixed water setpoint; its
//   three-way valve is far too slow to be modulated, which is why v3's PID on
//   the circulator was dropped.
// Electric — fine trim on a narrow band, with predictive shutoff so thermal
//   inertia does not carry the temperature past the setpoint.
//
// Because the bands differ (hydraulic 1.5°C, electric 0.5°C), a large error
// engages both sources while the last fraction of a degree is closed by the
// electric alone.
//
// Four independent conditions gate heating, all of which must hold:
//   heating_permitted_ — inlet probe is fresh (sensor timeout interlock)
//   fan_active_        — no heat without airflow
//   *_enabled_         — user toggles from the menu
//   hydraulic_online_  — the remote module is answering on RS485
//
// SetCurrentHour() must be called each loop (from the optional RTC) for the ECO
// window. Without an RTC the mode stays PERFORMANCE.
class TemperatureManager
{
public:
  explicit TemperatureManager(ElectricHeater *electric_heater);

  void Begin();
  void Update(float current_temperature);

  // Temperature target (set from the menu)
  void  SetTargetTemperature(float temperature);
  float GetTargetTemperature() const { return params_.temperature_target; }

  // Returns the currently active setpoint (reduced during the ECO night window)
  float GetEffectiveTargetTemperature() const;

  bool IsTemperatureInRange() const;

  // Parameters
  TemperatureParams       &GetParams()       { return params_; }
  const TemperatureParams &GetParams() const { return params_; }

  ElectricHeater *GetElectricHeater() { return electric_heater_; }

  // --- Source availability and user intent ---

  // Remote hydraulic module answering on RS485.
  void SetHydraulicOnline(bool online);
  bool GetHydraulicOnline() const { return hydraulic_online_; }

  // Menu toggles.
  void SetHydraulicEnabled(bool enabled);
  bool GetHydraulicEnabled() const { return hydraulic_enabled_; }
  void SetElectricEnabled(bool enabled);
  bool GetElectricEnabled() const { return electric_enabled_; }

  // Sensor freshness interlock: false → all heating off, timers preserved.
  void SetHeatingPermitted(bool permitted);
  bool GetHeatingPermitted() const { return heating_permitted_; }

  // Fan interlock — both sources blocked when the fan is not running.
  void SetFanActive(bool active);
  bool GetFanActive() const { return fan_active_; }

  // --- Current outputs (for the display and telemetry) ---
  bool GetElectricOn()  const { return electric_on_; }
  bool GetHydraulicOn() const { return hydraulic_on_; }

  float GetElectricOnTimer()  const { return elec_on_timer_; }
  float GetHydraulicOnTimer() const { return hydro_on_timer_; }
  float GetTemperatureDerivative() const { return dT_dt_; }

  ControlState GetControlState() const { return control_state_; }
  static const char *GetControlStateName(ControlState state);

  // Operating mode (ECO requires an RTC; forced to PERFORMANCE without one)
  void          SetOperatingMode(OperatingMode mode);
  OperatingMode GetOperatingMode() const { return operating_mode_; }
  bool          IsEcoActive() const { return operating_mode_ == OperatingMode::ECO; }

  // Current hour from the RTC — must be updated each loop for the ECO window
  void SetCurrentHour(uint8_t hour) { current_hour_ = hour; }

  // True when ECO is selected and the current time is inside the night window
  bool IsEcoWindowActive() const;

  // Turn both sources off immediately, preserving the anti-short-cycle timers.
  void AllOff() { ForceAllOff(); }

  // Reset both sources and their timers — called on phase transitions
  void ResetControl();

  void PrintDebug() const;

private:
  ElectricHeater   *electric_heater_;
  TemperatureParams params_;

  float    current_temperature_;
  uint32_t last_update_ms_;

  bool hydraulic_online_;   // remote module reachable
  bool hydraulic_enabled_;  // menu toggle
  bool electric_enabled_;   // menu toggle
  bool heating_permitted_;  // sensor freshness interlock
  bool fan_active_;         // fan interlock

  bool electric_on_;
  bool hydraulic_on_;

  ControlState control_state_;

  float dT_dt_;       // Filtered temperature derivative (°C/s), positive when rising
  float prev_temp_;
  bool  first_tick_;

  float elec_on_timer_;
  float elec_off_timer_;
  float hydro_on_timer_;
  float hydro_off_timer_;

  float debug_log_timer_s_;

  OperatingMode operating_mode_;
  uint8_t       current_hour_;

  // Switch a source; resets the matching anti-short-cycle timer.
  void SetElectric(bool on);
  void SetHydraulic(bool on);

  // Force both sources off without disturbing the anti-short-cycle timers.
  void ForceAllOff();

  // True when the temperature is rising fast enough that it would overshoot
  // the setpoint within `horizon` seconds if the source kept running.
  bool WillOvershoot(float temperature, float setpoint, float horizon) const;

  void UpdateHeating(float dt);
};

#endif // TEMPERATURE_MANAGER_H
