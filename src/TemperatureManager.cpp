#include "TemperatureManager.h"
#include "Logger.h"

TemperatureManager::TemperatureManager(ElectricHeater *electric_heater)
    : electric_heater_(electric_heater),
      params_(),
      current_temperature_(0.0f),
      last_update_ms_(0),
      hydraulic_online_(false),
      hydraulic_enabled_(HYDRAULIC_ENABLED_DEFAULT),
      electric_enabled_(ELECTRIC_ENABLED_DEFAULT),
      heating_permitted_(false),  // no reading yet at construction
      fan_active_(false),
      electric_on_(false),
      hydraulic_demand_(false),
      control_state_(ControlState::OFF),
      dT_dt_(0.0f),
      prev_temp_(0.0f),
      first_tick_(true),
      elec_on_timer_(0.0f),
      elec_off_timer_(CTRL_T_OFF_MIN),  // allow immediate first activation
      air_renewal_timer_s_(0.0f),
      debug_log_timer_s_(0.0f),
      operating_mode_(OperatingMode::PERFORMANCE),
      current_hour_(0) {}

void TemperatureManager::Begin()
{
  electric_heater_->Begin();
  last_update_ms_ = millis();

  Logger::Info("TemperatureManager: initialized (electric regulated, hydraulic on permission)");
  Logger::Info("  electric: band=%FC on>=%Fs off>=%Fs horizon=%Fs",
               params_.band_electric, params_.electric_t_on_min,
               params_.electric_t_off_min, params_.horizon_electric);
  Logger::Info("  air renewal window: %Fs", params_.air_renewal_window);
}

void TemperatureManager::Update(float current_temperature)
{
  current_temperature_ = current_temperature;

  uint32_t now = millis();
  float dt = (now - last_update_ms_) / 1000.0f;
  last_update_ms_ = now;

  if (dt <= 0.0f || dt > 10.0f)
  {
    Logger::Warning("TemperatureManager: invalid dt (%F s), skipping update", dt);
    return;
  }

  UpdateHeating(dt);
}

void TemperatureManager::SetElectric(bool on)
{
  if (on == electric_on_) return;
  electric_on_ = on;
  if (on)
    elec_on_timer_ = 0.0f;   // start counting a new ON period
  else
    elec_off_timer_ = 0.0f;  // start counting a new OFF period
}

void TemperatureManager::ForceAllOff()
{
  SetElectric(false);
  hydraulic_demand_ = false;
  electric_heater_->SetPower(0.0f);
}

bool TemperatureManager::WillOvershoot(float temperature, float setpoint, float horizon) const
{
  // Only trust the derivative above the quantization noise floor: with 0.1°C
  // sensor resolution at 1 Hz, a single-step noise spike yields a filtered
  // derivative around 0.03°C/s, which would otherwise cut the source short.
  if (dT_dt_ <= CTRL_DT_PREDICT_MIN)
  {
    return false;
  }
  return (temperature + dT_dt_ * horizon) >= setpoint;
}

void TemperatureManager::UpdateHeating(float dt)
{
  float setpoint = GetEffectiveTargetTemperature();
  float T = current_temperature_;

  // Ahead of the early returns below, so a window armed while heating is blocked
  // still expires on time rather than waiting for the interlock to clear.
  bool air_renewal = air_renewal_timer_s_ > 0.0f;
  if (air_renewal)
  {
    air_renewal_timer_s_ -= dt;
    if (air_renewal_timer_s_ < 0.0f) air_renewal_timer_s_ = 0.0f;
  }

  // --- Sensor fault ---
  if (isnan(T) || T < -20.0f || T > 200.0f)
  {
    ForceAllOff();
    control_state_ = ControlState::OFF;
    Logger::Error("TempMgr: sensor fault (T=%F) — all off", T);
    return;
  }

  // --- Safety temperature cutoff ---
  if (T > params_.safety_max)
  {
    ForceAllOff();
    control_state_ = ControlState::OFF;
    Logger::Warning("TempMgr: SAFETY CUTOFF T=%FC > %FC — all off", T, params_.safety_max);
    return;
  }

  // --- Global interlocks: stale reading, or no airflow ---
  if (!heating_permitted_ || !fan_active_)
  {
    ForceAllOff();
    control_state_ = ControlState::OFF;
    return;
  }

  // --- Filtered temperature derivative (°C/s, positive when rising) ---
  if (!first_tick_)
  {
    float raw = (T - prev_temp_) / dt;
    dT_dt_ = DERIVATIVE_FILTER * raw + (1.0f - DERIVATIVE_FILTER) * dT_dt_;
  }
  else
  {
    dT_dt_ = 0.0f;
    first_tick_ = false;
  }
  prev_temp_ = T;

  float error = setpoint - T;

  bool hydro_usable = hydraulic_online_ && hydraulic_enabled_;
  bool elec_usable  = electric_enabled_;

  // --- Hydraulic: a run permission, not a command ---
  //
  // Everything that would withdraw it — a fault, the safety cutoff, a stale
  // probe, the fan stopping, the session ending — has already returned above
  // through ForceAllOff(). Reaching here with the source enabled is the whole
  // condition. Deliberately not gated on hydraulic_online_: an unreachable
  // module cannot be written to either way, and folding it in would make the
  // flag restate what the availability check already says.
  hydraulic_demand_ = hydraulic_enabled_;

  // --- Electric: fine trim, narrow band, fast cycling ---
  if (!elec_usable)
  {
    SetElectric(false);
  }
  else if (!electric_on_)
  {
    // The minimum off-time is waived while the air is being renewed: the cold
    // front arriving is exactly when the heater is wanted, and making it sit out
    // a minute first is how the transient turns into a sag.
    bool off_time_met = air_renewal || elec_off_timer_ >= params_.electric_t_off_min;
    if (off_time_met && error > params_.band_electric)
    {
      SetElectric(true);
      Logger::Info("TempMgr: electric -> ON (err=%F%s)", error,
                   air_renewal ? " air-renewal" : "");
    }
  }
  else
  {
    // Suspended for the same window. The recovery ramp once the register shuts
    // is far steeper than any approach to setpoint the horizon was sized for,
    // and reading it as an impending overshoot cuts the heater degrees short.
    bool overshoot = !air_renewal && WillOvershoot(T, setpoint, params_.horizon_electric);
    if (elec_on_timer_ >= params_.electric_t_on_min && (error <= 0.0f || overshoot))
    {
      SetElectric(false);
      Logger::Info("TempMgr: electric -> OFF (err=%F dT=%F overshoot=%d)",
                   error, dT_dt_, (int)overshoot);
    }
  }

  // --- Apply electric output ---
  electric_heater_->SetPower(electric_on_ ? 1.0f : 0.0f);

  // --- Report which sources are in play ---
  if (hydro_usable && elec_usable)
    control_state_ = ControlState::HYDRAULIC_ELECTRIC;
  else if (hydro_usable)
    control_state_ = ControlState::HYDRAULIC_ONLY;
  else if (elec_usable)
    control_state_ = ControlState::ELECTRIC_ONLY;
  else
    control_state_ = ControlState::OFF;

  // --- Advance anti-short-cycle timers ---
  if (electric_on_)
    elec_on_timer_ += dt;
  else
    elec_off_timer_ += dt;

  // --- Periodic debug log ---
  debug_log_timer_s_ += dt;
  if (debug_log_timer_s_ >= 2.0f)
  {
    debug_log_timer_s_ = 0.0f;
    Logger::Info("TempMgr: [%s] sp=%FC T=%FC err=%F dT=%F/s hydro=%s elec=%s%s",
                 GetControlStateName(control_state_), setpoint, T, error, dT_dt_,
                 hydraulic_demand_ ? "RUN" : "OFF", electric_on_ ? "ON" : "OFF",
                 air_renewal ? " [air renewal]" : "");
    Logger::Debug("TempMgr: elec_on=%Fs elec_off=%Fs renewal=%Fs",
                  elec_on_timer_, elec_off_timer_, air_renewal_timer_s_);
  }
}

const char *TemperatureManager::GetControlStateName(ControlState state)
{
  switch (state)
  {
  case ControlState::HYDRAULIC_ELECTRIC: return "HYDRO+ELEC";
  case ControlState::HYDRAULIC_ONLY:     return "HYDRO_ONLY";
  case ControlState::ELECTRIC_ONLY:      return "ELEC_ONLY";
  default:                               return "OFF";
  }
}

void TemperatureManager::NotifyAirRenewal()
{
  air_renewal_timer_s_ = params_.air_renewal_window;

  // The derivative is restarted, not carried across. Either side of a damper
  // movement the probe is reading a different body of air, so the filtered slope
  // built up before it describes nothing that is still true.
  dT_dt_      = 0.0f;
  first_tick_ = true;

  Logger::Info("TemperatureManager: air renewal — tolerance window %Fs",
               params_.air_renewal_window);
}

void TemperatureManager::ResetControl()
{
  electric_on_      = false;
  hydraulic_demand_ = false;
  elec_on_timer_    = 0.0f;
  elec_off_timer_   = params_.electric_t_off_min;  // allow immediate activation
  air_renewal_timer_s_ = 0.0f;
  dT_dt_      = 0.0f;
  first_tick_ = true;
  control_state_ = ControlState::OFF;
  electric_heater_->SetPower(0.0f);
  Logger::Info("TemperatureManager: control reset");
}

void TemperatureManager::PrintDebug() const
{
  Logger::Info("[TempMgr] state=%s T=%FC sp=%FC dT=%F/s | hydro=%s | elec=%s on=%Fs off=%Fs | renewal=%Fs",
               GetControlStateName(control_state_), current_temperature_,
               GetEffectiveTargetTemperature(), dT_dt_,
               hydraulic_demand_ ? "RUN" : "OFF",
               electric_on_ ? "ON" : "OFF", elec_on_timer_, elec_off_timer_,
               air_renewal_timer_s_);
}

void TemperatureManager::SetTargetTemperature(float temperature)
{
  temperature = constrain(temperature, TARGET_TEMP_MIN, TARGET_TEMP_MAX);
  if (fabsf(temperature - params_.temperature_target) < 0.05f)
    return;

  // The anti-short-cycle timers are deliberately preserved: a setpoint change
  // from the menu must never be a way to re-energise a source early. v3 reset
  // them here because the PID needed a clean restart; the hysteresis does not.
  Logger::Info("TemperatureManager: target %FC -> %FC",
               params_.temperature_target, temperature);
  params_.temperature_target = temperature;
}

float TemperatureManager::GetEffectiveTargetTemperature() const
{
  if (IsEcoWindowActive())
    return params_.temperature_target * (params_.eco_target_percentage / 100.0f);
  return params_.temperature_target;
}

bool TemperatureManager::IsEcoWindowActive() const
{
  if (operating_mode_ != OperatingMode::ECO)
    return false;

  // The window wraps around midnight when start > end (the usual 18h -> 9h).
  if (params_.eco_start_hour > params_.eco_end_hour)
    return (current_hour_ >= params_.eco_start_hour) || (current_hour_ < params_.eco_end_hour);
  return (current_hour_ >= params_.eco_start_hour) && (current_hour_ < params_.eco_end_hour);
}

bool TemperatureManager::IsTemperatureInRange() const
{
  return fabs(current_temperature_ - params_.temperature_target) <= 2.0f;
}

void TemperatureManager::SetHydraulicOnline(bool online)
{
  if (hydraulic_online_ == online)
    return;
  hydraulic_online_ = online;
  // The permission is untouched. It says what the dryer wants, and losing the
  // bus does not change that — it only stops the answer getting through. What
  // happens to a module left running behind a dead bus is the module's own
  // watchdog to handle; nothing here can reach it to say otherwise.
  Logger::Info("TemperatureManager: hydraulic module %s",
               online ? "online" : "offline");
}

void TemperatureManager::SetHydraulicEnabled(bool enabled)
{
  if (hydraulic_enabled_ == enabled)
    return;
  hydraulic_enabled_ = enabled;
  // Withdrawn at once rather than on the next tick: switching the source off at
  // the menu has to reach the module on the next RS485 cycle, not after the
  // control loop next happens to run — which, outside a session, it never does.
  if (!enabled)
  {
    hydraulic_demand_ = false;
  }
  Logger::Info("TemperatureManager: hydraulic source %s", enabled ? "enabled" : "disabled");
}

void TemperatureManager::SetElectricEnabled(bool enabled)
{
  if (electric_enabled_ == enabled)
    return;
  electric_enabled_ = enabled;
  if (!enabled)
  {
    SetElectric(false);
    electric_heater_->SetPower(0.0f);
  }
  Logger::Info("TemperatureManager: electric source %s", enabled ? "enabled" : "disabled");
}

void TemperatureManager::SetHeatingPermitted(bool permitted)
{
  if (heating_permitted_ == permitted)
    return;
  heating_permitted_ = permitted;
  if (!permitted)
  {
    ForceAllOff();
  }
  Logger::Info("TemperatureManager: heating %s",
               permitted ? "permitted" : "blocked — stale sensor reading");
}

void TemperatureManager::SetFanActive(bool active)
{
  if (fan_active_ == active)
    return;
  fan_active_ = active;
  if (!active)
  {
    ForceAllOff();
  }
  Logger::Info("TemperatureManager: fan %s", active ? "active" : "inactive — heaters blocked");
}

void TemperatureManager::SetOperatingMode(OperatingMode mode)
{
  if (operating_mode_ == mode)
    return;
  operating_mode_ = mode;
  Logger::Info("TemperatureManager: mode -> %s",
               mode == OperatingMode::ECO ? "ECO" : "PERFORMANCE");
}
