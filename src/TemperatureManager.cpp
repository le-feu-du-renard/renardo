#include "TemperatureManager.h"
#include "Logger.h"

TemperatureManager::TemperatureManager(ElectricHeater *electric_heater,
                                       HydraulicHeater *hydraulic_heater)
    : electric_heater_(electric_heater),
      hydraulic_heater_(hydraulic_heater),
      params_(),
      pid_(HYDRAULIC_KP, HYDRAULIC_KI, HYDRAULIC_KD,
           0.0f, 100.0f, PID_INTEGRAL_MAX, PID_DERIVATIVE_FILTER),
      current_temperature_(0.0f),
      last_update_ms_(0),
      hydraulic_available_(HYDRAULIC_AVAILABLE),
      electric_enabled_(ELECTRIC_ENABLED),
      fan_active_(false),
      electric_on_(false),
      control_state_(HYDRAULIC_AVAILABLE ? ControlState::REGULATION : ControlState::ELECTRIC_ONLY),
      dT_dt_(0.0f),
      prev_temp_(0.0f),
      first_tick_(true),
      hydro_sat_timer_(0.0f),
      elec_on_timer_(0.0f),
      elec_off_timer_(CTRL_T_OFF_MIN),  // allow immediate first activation
      debug_log_timer_s_(0.0f),
      operating_mode_(OperatingMode::PERFORMANCE),
      current_hour_(0) {}

void TemperatureManager::Begin()
{
  electric_heater_->Begin();
  hydraulic_heater_->Begin();

  pid_.SetParameters(params_.hydraulic_kp, params_.hydraulic_ki, params_.hydraulic_kd);
  pid_.SetOutputLimits(0.0f, 100.0f);
  pid_.SetIntegralLimit(params_.pid_integral_max);
  pid_.SetDerivativeFilter(params_.pid_derivative_filter);
  pid_.Reset();

  last_update_ms_ = millis();

  Logger::Info("TemperatureManager: initialized (PID+state-machine, %s)",
               hydraulic_available_ ? "REGULATION" : "ELECTRIC_ONLY");
  Logger::Info("PID: Kp=%F Ki=%F Kd=%F | E_HAUT=%F E_BAS=%F T_ON=%F T_OFF=%F",
               params_.hydraulic_kp, params_.hydraulic_ki, params_.hydraulic_kd,
               CTRL_E_HAUT, CTRL_E_BAS, CTRL_T_ON_MIN, CTRL_T_OFF_MIN);
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

// Helper: switch electric relay and reset the appropriate anti-short-cycle timer.
void TemperatureManager::SetElectric(bool on)
{
  if (on == electric_on_) return;
  electric_on_ = on;
  if (on)
    elec_on_timer_ = 0.0f;   // start counting new ON period
  else
    elec_off_timer_ = 0.0f;  // start counting new OFF period
}

// Helper: force both actuators off without touching the state machine or timers.
void TemperatureManager::ForceAllOff()
{
  if (electric_on_)
    SetElectric(false);
  electric_heater_->SetPower(0.0f);
  hydraulic_heater_->SetPower(0);
}

void TemperatureManager::UpdateHeating(float dt)
{
  float setpoint = GetEffectiveTargetTemperature();
  float T = current_temperature_;

  // --- Sensor fault ---
  if (isnan(T) || T < -20.0f || T > 200.0f)
  {
    ForceAllOff();
    pid_.Reset();
    Logger::Error("TempMgr: sensor fault (T=%F) — all off", T);
    return;
  }

  // --- Safety temperature cutoff ---
  if (T > TEMPERATURE_SAFETY_MAX)
  {
    ForceAllOff();
    pid_.Reset();
    control_state_ = hydraulic_available_ ? ControlState::REGULATION : ControlState::ELECTRIC_ONLY;
    Logger::Warning("TempMgr: SAFETY CUTOFF T=%FC > %FC — all off",
                    T, TEMPERATURE_SAFETY_MAX);
    return;
  }

  // --- Fan interlock ---
  if (!fan_active_)
  {
    ForceAllOff();
    Logger::Warning("TempMgr: fan not active — heaters blocked");
    return;
  }

  // --- Heating globally disabled (sensor-timeout guard) ---
  if (!electric_enabled_)
  {
    ForceAllOff();
    return;
  }

  // --- Filtered temperature derivative ---
  // dT_dt_ > 0 when temperature is rising; used for ETA and prediction.
  if (!first_tick_)
  {
    float raw = (T - prev_temp_) / dt;
    dT_dt_ = PID_DERIVATIVE_FILTER * raw + (1.0f - PID_DERIVATIVE_FILTER) * dT_dt_;
  }
  else
  {
    dT_dt_ = 0.0f;
    first_tick_ = false;
  }
  prev_temp_ = T;

  float error = setpoint - T;

  // --- Hydraulic availability transitions ---
  bool should_use_hydro = hydraulic_available_;

  if (!should_use_hydro && control_state_ != ControlState::ELECTRIC_ONLY)
  {
    // Hydraulic just lost: freeze PID, enter ELECTRIC_ONLY.
    // Electric timers are intentionally NOT reset (anti-short-cycle preserved).
    control_state_ = ControlState::ELECTRIC_ONLY;
    Logger::Info("TempMgr: hydraulic disabled -> ELECTRIC_ONLY (elec=%s)",
                 electric_on_ ? "ON" : "OFF");
  }
  else if (should_use_hydro && control_state_ == ControlState::ELECTRIC_ONLY)
  {
    // Hydraulic restored: bumpless resume.
    // Set integral so PID output starts at 0 (hydro was off), minimising the step.
    float resume_int = 0.0f;
    if (params_.hydraulic_ki > 0.0f)
      resume_int = constrain(-params_.hydraulic_kp * error / params_.hydraulic_ki,
                              -params_.pid_integral_max, params_.pid_integral_max);
    pid_.SetIntegral(resume_int);
    control_state_ = ControlState::REGULATION;
    hydro_sat_timer_ = 0.0f;
    Logger::Info("TempMgr: hydraulic re-enabled -> REGULATION (bumpless I=%F)", resume_int);
  }

  // --- State machine ---

  if (control_state_ == ControlState::ELECTRIC_ONLY)
  {
    // Hard guard applied here AND again at output stage.
    hydraulic_heater_->SetPower(0);

    if (!electric_on_)
    {
      if (elec_off_timer_ >= CTRL_T_OFF_MIN && error > CTRL_BANDE_ELEC)
      {
        SetElectric(true);
        Logger::Info("TempMgr: ELEC_ONLY -> ON (err=%F)", error);
      }
    }
    else
    {
      // Predictive shutoff: only when dT_dt exceeds the quantization noise floor.
      // With 0.1°C sensor resolution at 1 Hz, a single-step noise spike gives a
      // filtered derivative of ~0.03°C/s. CTRL_DT_PREDICT_MIN filters out these
      // transients and only fires when the temperature is genuinely rising fast
      // enough to risk overshoot.
      float predicted = T + dT_dt_ * CTRL_HORIZON;
      bool will_overshoot = (dT_dt_ > CTRL_DT_PREDICT_MIN) && (predicted >= setpoint);
      if (elec_on_timer_ >= CTRL_T_ON_MIN && (error <= 0.0f || will_overshoot))
      {
        SetElectric(false);
        Logger::Info("TempMgr: ELEC_ONLY -> OFF (err=%F predict=%F dT=%F)",
                     error, predicted, dT_dt_);
      }
    }
  }
  else if (control_state_ == ControlState::REGULATION)
  {
    // PID drives hydraulic; integral frozen when output is saturated.
    float last_u = pid_.GetLastOutput();
    bool freeze_int = (last_u >= 100.0f || last_u <= 0.0f);
    float u = pid_.Compute(setpoint, T, dt, freeze_int);

    hydraulic_heater_->SetPower((uint8_t)constrain(u, 0.0f, 100.0f));

    hydro_sat_timer_ = (u >= 99.0f) ? hydro_sat_timer_ + dt : 0.0f;

    // Estimated time to setpoint:
    //   rising:          ETA = error / dT_dt
    //   falling notably: treat as infinite (temp will never reach setpoint on its own)
    //   flat/slow:       0 (don't trigger ETA-based BOOST)
    float eta;
    if (dT_dt_ > 0.001f)
      eta = error / dT_dt_;
    else if (dT_dt_ < -CTRL_DT_FALLING)
      eta = CTRL_ETA_MAX + 1.0f;  // treat as infinite → triggers cond3
    else
      eta = 0.0f;

    bool can_boost = (elec_off_timer_ >= CTRL_T_OFF_MIN);
    bool cond1 = (error > CTRL_E_HAUT);
    bool cond2 = (hydro_sat_timer_ >= CTRL_T_SAT && error > CTRL_E_BAS);
    bool cond3 = (eta > CTRL_ETA_MAX && error > CTRL_E_BAS);

    if (can_boost && (cond1 || cond2 || cond3))
    {
      control_state_ = ControlState::BOOST;
      hydro_sat_timer_ = 0.0f;
      hydraulic_heater_->SetPower(100);
      SetElectric(true);
      Logger::Info("TempMgr: -> BOOST (err=%F cond1=%d cond2=%d cond3=%d eta=%F)",
                   error, (int)cond1, (int)cond2, (int)cond3,
                   (dT_dt_ > 0.001f ? eta : -1.0f));
    }
  }
  else  // BOOST
  {
    // Hydraulic forced to 100%. PID runs with integral frozen to track error/derivative
    // state, so that the bumpless exit integral is computed against the current error.
    hydraulic_heater_->SetPower(100);
    pid_.Compute(setpoint, T, dt, true);

    bool can_exit    = (elec_on_timer_ >= CTRL_T_ON_MIN);
    bool should_exit = (error < CTRL_E_BAS);

    if (can_exit && should_exit)
    {
      // Bumpless: set integral so PID output starts at 100% (hydro was at 100%).
      float resume_int = 0.0f;
      if (params_.hydraulic_ki > 0.0f)
        resume_int = constrain((100.0f - params_.hydraulic_kp * error) / params_.hydraulic_ki,
                                -params_.pid_integral_max, params_.pid_integral_max);
      pid_.SetIntegral(resume_int);
      control_state_ = ControlState::REGULATION;
      hydro_sat_timer_ = 0.0f;
      SetElectric(false);
      Logger::Info("TempMgr: BOOST -> REGULATION (err=%F bumpless I=%F)", error, resume_int);
    }
  }

  // --- Hard hydraulic guard: when hydro is unavailable, force to 0 at output ---
  if (!hydraulic_available_)
    hydraulic_heater_->SetPower(0);

  // --- Apply electric output ---
  electric_heater_->SetPower(electric_on_ ? 1.0f : 0.0f);

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
    const char *st = (control_state_ == ControlState::REGULATION)   ? "REGULATION"
                   : (control_state_ == ControlState::BOOST)        ? "BOOST"
                                                                     : "ELEC_ONLY";
    Logger::Info("TempMgr: [%s] sp=%FC T=%FC err=%F dT=%F/s hydro=%u%% elec=%s",
                 st, setpoint, T, error, dT_dt_,
                 hydraulic_heater_->GetPower(), electric_on_ ? "ON" : "OFF");
    Logger::Debug("TempMgr: elec_on=%Fs elec_off=%Fs sat=%Fs | P=%F I=%F D=%F u=%F",
                  elec_on_timer_, elec_off_timer_, hydro_sat_timer_,
                  pid_.GetProportionalTerm(), pid_.GetIntegralTerm(),
                  pid_.GetDerivativeTerm(), pid_.GetLastOutput());
  }
}

void TemperatureManager::ResetControl()
{
  pid_.Reset();
  electric_on_ = false;
  elec_on_timer_ = 0.0f;
  elec_off_timer_ = CTRL_T_OFF_MIN;  // allow immediate first activation in new phase
  hydro_sat_timer_ = 0.0f;
  dT_dt_ = 0.0f;
  first_tick_ = true;
  control_state_ = hydraulic_available_ ? ControlState::REGULATION : ControlState::ELECTRIC_ONLY;
  electric_heater_->SetPower(0.0f);
  hydraulic_heater_->SetPower(0);
  Logger::Info("TemperatureManager: control reset (phase transition)");
}

void TemperatureManager::PrintDebug() const
{
  const char *st = (control_state_ == ControlState::REGULATION)   ? "REGULATION"
                 : (control_state_ == ControlState::BOOST)        ? "BOOST"
                                                                   : "ELEC_ONLY";
  Logger::Info("[TempMgr] state=%s err=%F P=%F I=%F D=%F u=%F%% | elec=%s on=%Fs off=%Fs",
               st, pid_.GetLastError(),
               pid_.GetProportionalTerm(), pid_.GetIntegralTerm(), pid_.GetDerivativeTerm(),
               pid_.GetLastOutput(),
               electric_on_ ? "ON" : "OFF",
               elec_on_timer_, elec_off_timer_);
}

void TemperatureManager::SetTargetTemperature(float temperature)
{
  temperature = constrain(temperature, 20.0f, 45.0f);
  if (fabsf(temperature - params_.temperature_target) < 1.0f)
    return;

  Logger::Info("TemperatureManager: target %FC -> %FC, resetting control",
               params_.temperature_target, temperature);
  params_.temperature_target = temperature;
  pid_.Reset();
  electric_on_ = false;
  electric_heater_->SetPower(0.0f);
  hydraulic_heater_->SetPower(0);
  elec_on_timer_ = 0.0f;
  elec_off_timer_ = CTRL_T_OFF_MIN;
  hydro_sat_timer_ = 0.0f;
  first_tick_ = true;
  control_state_ = hydraulic_available_ ? ControlState::REGULATION : ControlState::ELECTRIC_ONLY;
}

float TemperatureManager::GetEffectiveTargetTemperature() const
{
  if (IsEcoWindowActive())
    return params_.temperature_target * (ECO_NIGHT_TARGET_PERCENTAGE / 100.0f);
  return params_.temperature_target;
}

bool TemperatureManager::IsEcoWindowActive() const
{
  if (operating_mode_ != OperatingMode::ECO)
    return false;
  return (current_hour_ >= ECO_START_HOUR) || (current_hour_ < ECO_END_HOUR);
}

bool TemperatureManager::IsTemperatureInRange() const
{
  return fabs(current_temperature_ - params_.temperature_target) <= 2.0f;
}

void TemperatureManager::SetHydraulicAvailable(bool available)
{
  if (hydraulic_available_ == available)
    return;
  hydraulic_available_ = available;
  // Do NOT reset electric timers here: UpdateHeating() handles the transition
  // on the next tick and the anti-short-cycle timers must be preserved.
  Logger::Info("TemperatureManager: hydraulic %s (transition on next tick)",
               available ? "available" : "unavailable");
}

void TemperatureManager::SetElectricEnabled(bool enabled)
{
  if (electric_enabled_ == enabled)
    return;
  electric_enabled_ = enabled;
  if (!enabled)
  {
    // ForceAllOff() preserves the anti-short-cycle timers via SetElectric().
    ForceAllOff();
    hydraulic_heater_->SetPower(0);
  }
  Logger::Info("TemperatureManager: electric heating %s", enabled ? "enabled" : "disabled");
}

void TemperatureManager::SetFanActive(bool active)
{
  if (fan_active_ == active)
    return;
  fan_active_ = active;
  if (!active)
  {
    ForceAllOff();
    hydraulic_heater_->SetPower(0);
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
