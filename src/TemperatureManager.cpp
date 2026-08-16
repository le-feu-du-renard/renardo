#include "TemperatureManager.h"
#include "Logger.h"

TemperatureManager::TemperatureManager(ElectricHeater *electric_heater)
    : electric_heater_(electric_heater),
      params_(),
      current_temperature_(0.0f),
      last_update_ms_(0),
      hydraulic_online_(false),
      hydraulic_enabled_(HYDRAULIC_AVAILABLE),
      electric_enabled_(ELECTRIC_ENABLED),
      heating_permitted_(false),  // no reading yet at construction
      fan_active_(false),
      electric_on_(false),
      hydraulic_on_(false),
      control_state_(ControlState::OFF),
      dT_dt_(0.0f),
      prev_temp_(0.0f),
      first_tick_(true),
      elec_on_timer_(0.0f),
      elec_off_timer_(CTRL_T_OFF_MIN),        // allow immediate first activation
      hydro_on_timer_(0.0f),
      hydro_off_timer_(CTRL_HYDRO_T_OFF_MIN), // idem
      debug_log_timer_s_(0.0f),
      operating_mode_(OperatingMode::PERFORMANCE),
      current_hour_(0) {}

void TemperatureManager::Begin()
{
  electric_heater_->Begin();
  last_update_ms_ = millis();

  Logger::Info("TemperatureManager: initialized (dual on/off sources)");
  Logger::Info("  hydraulic: band=%FC on>=%Fs off>=%Fs horizon=%Fs",
               params_.band_hydraulic, params_.hydraulic_t_on_min,
               params_.hydraulic_t_off_min, params_.horizon_hydraulic);
  Logger::Info("  electric:  band=%FC on>=%Fs off>=%Fs horizon=%Fs",
               params_.band_electric, params_.electric_t_on_min,
               params_.electric_t_off_min, params_.horizon_electric);
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

void TemperatureManager::SetHydraulic(bool on)
{
  if (on == hydraulic_on_) return;
  hydraulic_on_ = on;
  if (on)
    hydro_on_timer_ = 0.0f;
  else
    hydro_off_timer_ = 0.0f;
}

void TemperatureManager::ForceAllOff()
{
  SetElectric(false);
  SetHydraulic(false);
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

  // --- Hydraulic: base heat, wide band, slow cycling ---
  if (!hydro_usable)
  {
    SetHydraulic(false);
  }
  else if (!hydraulic_on_)
  {
    if (hydro_off_timer_ >= params_.hydraulic_t_off_min && error > params_.band_hydraulic)
    {
      SetHydraulic(true);
      Logger::Info("TempMgr: hydraulic -> ON (err=%F)", error);
    }
  }
  else
  {
    bool overshoot = WillOvershoot(T, setpoint, params_.horizon_hydraulic);
    if (hydro_on_timer_ >= params_.hydraulic_t_on_min && (error <= 0.0f || overshoot))
    {
      SetHydraulic(false);
      Logger::Info("TempMgr: hydraulic -> OFF (err=%F dT=%F overshoot=%d)",
                   error, dT_dt_, (int)overshoot);
    }
  }

  // --- Electric: fine trim, narrow band, fast cycling ---
  if (!elec_usable)
  {
    SetElectric(false);
  }
  else if (!electric_on_)
  {
    if (elec_off_timer_ >= params_.electric_t_off_min && error > params_.band_electric)
    {
      SetElectric(true);
      Logger::Info("TempMgr: electric -> ON (err=%F)", error);
    }
  }
  else
  {
    bool overshoot = WillOvershoot(T, setpoint, params_.horizon_electric);
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

  if (hydraulic_on_)
    hydro_on_timer_ += dt;
  else
    hydro_off_timer_ += dt;

  // --- Periodic debug log ---
  debug_log_timer_s_ += dt;
  if (debug_log_timer_s_ >= 2.0f)
  {
    debug_log_timer_s_ = 0.0f;
    Logger::Info("TempMgr: [%s] sp=%FC T=%FC err=%F dT=%F/s hydro=%s elec=%s",
                 GetControlStateName(control_state_), setpoint, T, error, dT_dt_,
                 hydraulic_on_ ? "ON" : "OFF", electric_on_ ? "ON" : "OFF");
    Logger::Debug("TempMgr: elec_on=%Fs elec_off=%Fs hydro_on=%Fs hydro_off=%Fs",
                  elec_on_timer_, elec_off_timer_, hydro_on_timer_, hydro_off_timer_);
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

void TemperatureManager::ResetControl()
{
  electric_on_  = false;
  hydraulic_on_ = false;
  elec_on_timer_   = 0.0f;
  elec_off_timer_  = params_.electric_t_off_min;   // allow immediate activation in the new phase
  hydro_on_timer_  = 0.0f;
  hydro_off_timer_ = params_.hydraulic_t_off_min;
  dT_dt_ = 0.0f;
  first_tick_ = true;
  control_state_ = ControlState::OFF;
  electric_heater_->SetPower(0.0f);
  Logger::Info("TemperatureManager: control reset (phase transition)");
}

void TemperatureManager::PrintDebug() const
{
  Logger::Info("[TempMgr] state=%s T=%FC sp=%FC dT=%F/s | hydro=%s on=%Fs off=%Fs | elec=%s on=%Fs off=%Fs",
               GetControlStateName(control_state_), current_temperature_,
               GetEffectiveTargetTemperature(), dT_dt_,
               hydraulic_on_ ? "ON" : "OFF", hydro_on_timer_, hydro_off_timer_,
               electric_on_ ? "ON" : "OFF", elec_on_timer_, elec_off_timer_);
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
  // Timers are preserved: the transition is applied on the next tick and the
  // anti-short-cycle protection must survive a bus dropout.
  Logger::Info("TemperatureManager: hydraulic module %s",
               online ? "online" : "offline");
}

void TemperatureManager::SetHydraulicEnabled(bool enabled)
{
  if (hydraulic_enabled_ == enabled)
    return;
  hydraulic_enabled_ = enabled;
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
