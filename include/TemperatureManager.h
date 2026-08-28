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

// What is wired to the command output on OUT_ELECTRIC_PIN.
//
// One relay drives either, so this is a setting rather than a second pin — and
// it is not a preference. The two dry by opposite means: a resistance heats air
// that is then thrown away carrying the moisture with it, a dehumidifier
// condenses the moisture out and keeps the air. Everything downstream follows,
// including which way the register earns its movements.
enum class HeatSourceType : uint8_t
{
  kElectric      = HEAT_SOURCE_ELECTRIC,
  kDehumidifier  = HEAT_SOURCE_DEHUMIDIFIER,
};

// Which heat sources are usable this cycle. Reporting only — the commanded
// source is the one regulated here, and the hydraulic is not regulated at all,
// so there is no mode to switch between.
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

  float band_electric;      // °C, error above which the electric is requested
  float horizon_electric;   // s, predictive shutoff window
  float electric_t_on_min;  // s, anti-short-cycle
  float electric_t_off_min; // s
  float air_renewal_window; // s, tolerance window after the damper moves
  float safety_max;         // °C, hard cutoff

  uint8_t eco_start_hour;
  uint8_t eco_end_hour;
  float   eco_target_percentage;

  TemperatureParams()
      : temperature_target(TEMPERATURE_TARGET),
        band_electric(CTRL_BANDE_ELEC),
        horizon_electric(CTRL_HORIZON),
        electric_t_on_min(CTRL_T_ON_MIN),
        electric_t_off_min(CTRL_T_OFF_MIN),
        air_renewal_window(CTRL_AIR_RENEWAL_S),
        safety_max(TEMPERATURE_SAFETY_MAX),
        eco_start_hour(ECO_START_HOUR),
        eco_end_hour(ECO_END_HOUR),
        eco_target_percentage(ECO_NIGHT_TARGET_PERCENTAGE) {}
};

// Regulates the dryer air temperature. Two heat sources, only one of them
// regulated here — the split is one of authority, not of speed.
//
// Electric — the source the dryer commands. Narrow hysteresis band with
//   predictive shutoff, so thermal inertia does not carry the temperature past
//   the setpoint.
// Hydraulic — the source the dryer only asks for. The remote module owns its
//   start, its circulator and its water regulation; what leaves here is a run
//   permission, held for as long as the source is enabled and the interlocks
//   hold, and dropped when they do not. Cycling it on air temperature would put
//   two regulators on one three-way valve, and the slower one is not ours.
//
// Four independent conditions gate heating, all of which must hold:
//   heating_permitted_ — inlet probe is fresh (sensor timeout interlock)
//   fan_active_        — no heat without airflow
//   *_enabled_         — user toggles from the menu
//   hydraulic_online_  — the remote module is answering on RS485 (reporting and
//                        the fault only; an unreachable module cannot be written
//                        to, so the permission itself does not consult it)
//
// NotifyAirRenewal() must be called whenever the damper moves — see the method
// for what it relaxes and for how long.
//
// SetCurrentHour() must be called each loop (from the optional RTC) for the ECO
// window. Without an RTC the mode stays PERFORMANCE.
class TemperatureManager
{
public:
  explicit TemperatureManager(ElectricHeater *electric_heater);

  void Begin();

  // Both readings, because with a dehumidifier fitted the humidity is a control
  // input here and not only a transition test: the machine's whole reason to run
  // is the water in the air, and the heat is a by-product. With a resistance the
  // humidity is ignored, and the law is unchanged from the one that has always
  // been here.
  void Update(float current_temperature, float current_humidity);

  // Temperature target (set from the menu)
  void  SetTargetTemperature(float temperature);
  float GetTargetTemperature() const { return params_.temperature_target; }

  // Humidity target, %RH — the same figure HumidityManager holds, pushed here
  // so the dehumidifier's demand can be decided in one place with the rest of
  // the source law. Zero means no humidity target is set, and the dehumidifier
  // then runs on temperature alone.
  void  SetTargetHumidity(float humidity);
  float GetTargetHumidity() const { return target_humidity_; }

  // What is wired to the command output. Switching it cuts the source at once:
  // the two laws have nothing to hand over to each other, and leaving a
  // compressor energised across the change would be running it under a law that
  // is no longer the one that started it.
  void           SetHeatSource(HeatSourceType source);
  HeatSourceType GetHeatSource() const { return heat_source_; }
  bool IsDehumidifier() const { return heat_source_ == HeatSourceType::kDehumidifier; }

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
  bool GetElectricOn() const { return electric_on_; }

  // The run permission published to the remote module over RS485. True while
  // the source is enabled and a session is running with every interlock holding
  // — never a statement about whether the circulator is actually turning, which
  // is the module's business and only the module's.
  bool GetHydraulicDemand() const { return hydraulic_demand_; }

  float GetElectricOnTimer() const { return elec_on_timer_; }
  float GetTemperatureDerivative() const { return dT_dt_; }

  // Seconds left on the air-renewal tolerance window, 0 when it is closed.
  float GetAirRenewalRemaining() const { return air_renewal_timer_s_; }

  ControlState GetControlState() const { return control_state_; }
  static const char *GetControlStateName(ControlState state);

  // The same state, named for what is actually fitted: "DESHU_ONLY" rather than
  // "ELEC_ONLY" when the output drives a dehumidifier. The enum names the slot
  // and does not change; this names its occupant.
  const char *GetControlStateName() const;

  // Operating mode (ECO requires an RTC; forced to PERFORMANCE without one)
  void          SetOperatingMode(OperatingMode mode);
  OperatingMode GetOperatingMode() const { return operating_mode_; }
  bool          IsEcoActive() const { return operating_mode_ == OperatingMode::ECO; }

  // Current hour from the RTC — must be updated each loop for the ECO window
  void SetCurrentHour(uint8_t hour) { current_hour_ = hour; }

  // True when ECO is selected and the current time is inside the night window
  bool IsEcoWindowActive() const;

  // Cut the electric and withdraw the hydraulic permission immediately,
  // preserving the anti-short-cycle timers.
  void AllOff() { ForceAllOff(); }

  // The damper has just moved, and the air behind the probe is about to be
  // replaced. Opens a tolerance window of params_.air_renewal_window seconds in
  // which the predictive shutoff is suspended and the electric minimum off-time
  // is waived, so the loop works through the transient instead of sawtoothing
  // against it. The band, the error ≤ 0 cutoff, the minimum on-time and the
  // safety maximum are untouched.
  //
  // Deliberately does not cut the electric or disturb the timers: the heater is
  // most wanted at the moment cold air arrives, and the phase machine used to
  // open the circuit at exactly that instant.
  void NotifyAirRenewal();

  // Full reset of the control state — session start only. A phase transition
  // gets NotifyAirRenewal() instead.
  void ResetControl();

  void PrintDebug() const;

private:
  ElectricHeater   *electric_heater_;
  TemperatureParams params_;

  float    current_temperature_;
  float    current_humidity_;
  float    target_humidity_;
  uint32_t last_update_ms_;

  HeatSourceType heat_source_;

  bool hydraulic_online_;   // remote module reachable
  bool hydraulic_enabled_;  // menu toggle
  bool electric_enabled_;   // menu toggle
  bool heating_permitted_;  // sensor freshness interlock
  bool fan_active_;         // fan interlock

  bool electric_on_;
  bool hydraulic_demand_;

  ControlState control_state_;

  float dT_dt_;       // Filtered temperature derivative (°C/s), positive when rising
  float prev_temp_;
  bool  first_tick_;

  float elec_on_timer_;
  float elec_off_timer_;

  float air_renewal_timer_s_;  // counts down; > 0 while the window is open

  float debug_log_timer_s_;

  OperatingMode operating_mode_;
  uint8_t       current_hour_;

  // Switch the electric; resets the matching anti-short-cycle timer.
  void SetElectric(bool on);

  // Cut the electric and drop the hydraulic permission without disturbing the
  // anti-short-cycle timers.
  void ForceAllOff();

  // True when the temperature is rising fast enough that it would overshoot
  // the setpoint within `horizon` seconds if the source kept running.
  bool WillOvershoot(float temperature, float setpoint, float horizon) const;

  // The two halves of the source law, split so the anti-short-cycle timers and
  // the interlocks around them are written once and mean the same thing under
  // either source.
  //
  //   SourceWanted    should an idle source start?
  //   SourceSatisfied should a running source stop?
  //
  // Not each other's negation, and deliberately so: between them lies the
  // hysteresis band, which is the whole point.
  bool SourceWanted(float error) const;
  bool SourceSatisfied(float error, float temperature, float setpoint,
                       bool air_renewal) const;

  // Humidity above target, %RH, positive when there is water to remove. NAN
  // when no target is set or the reading is unusable — which a dehumidifier
  // reads as "no humidity demand", degrading it to a heater rather than leaving
  // it running against a number that is not there.
  float HumidityError() const;

  void UpdateHeating(float dt);
};

#endif // TEMPERATURE_MANAGER_H
