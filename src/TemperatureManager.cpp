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
      electric_on_timer_s_(0.0f),
      electric_settle_timer_s_(0.0f),
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

  Logger::Info("TemperatureManager: initialized (split-range, %s)",
               hydraulic_available_ ? "PRIMARY_HYDRO" : "PRIMARY_ELEC");
  Logger::Info("PID: Kp=%F Ki=%F Kd=%F",
               params_.hydraulic_kp, params_.hydraulic_ki, params_.hydraulic_kd);
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

void TemperatureManager::UpdateHeating(float dt)
{
  float effective_target = GetEffectiveTargetTemperature();

  // === BLOCK A: Safety cutoff ===
  // Hard stop if temperature exceeds the safety limit, regardless of mode.
  if (current_temperature_ > TEMPERATURE_SAFETY_MAX)
  {
    electric_heater_->SetPower(0.0f);
    hydraulic_heater_->SetPower(0);
    electric_on_ = false;
    electric_on_timer_s_ = 0.0f;
    electric_settle_timer_s_ = 0.0f;
    pid_.Reset();
    Logger::Warning("TempMgr: SAFETY CUTOFF T=%F > %FC — all heaters OFF",
                    current_temperature_, TEMPERATURE_SAFETY_MAX);
    return;
  }

  // === BLOCK A+: Ventilation interlock ===
  // Block both heaters if the fan is not confirmed active.
  if (!fan_active_)
  {
    electric_heater_->SetPower(0.0f);
    hydraulic_heater_->SetPower(0);
    electric_on_ = false;
    electric_on_timer_s_ = 0.0f;
    electric_settle_timer_s_ = 0.0f;
    Logger::Warning("TempMgr: fan not active — heaters blocked");
    return;
  }

  // === BLOCK B: Single PID with conditional anti-windup ===
  // The integral is frozen in three situations:
  //   1. Output saturated (u at 0% or 100%) — classic anti-windup.
  //   2. Electric heater pending (timer > 0 but relay not yet ON) — avoids windup
  //      during the ~30s delay before the heater activates.
  //   3. Settle window after heater turns ON — avoids windup during the ~30s
  //      thermal lag before the heater's heat actually reaches the sensor.
  float last_u = pid_.GetLastOutput();
  bool freeze_int = (last_u >= 100.0f) ||
                    (last_u <= 0.0f) ||
                    (electric_on_timer_s_ > 0.0f && !electric_on_) ||
                    (electric_settle_timer_s_ > 0.0f);

  float u = electric_enabled_
                ? pid_.Compute(effective_target, current_temperature_, dt, freeze_int)
                : 0.0f;

  if (!electric_enabled_)
  {
    electric_heater_->SetPower(0.0f);
    hydraulic_heater_->SetPower(0);
    electric_on_ = false;
    electric_on_timer_s_ = 0.0f;
    electric_settle_timer_s_ = 0.0f;
    return;
  }

  // Track the settle window: count down from ELECTRIC_SETTLE_S after heater turns ON.
  if (electric_settle_timer_s_ > 0.0f)
    electric_settle_timer_s_ = (electric_settle_timer_s_ > dt) ? electric_settle_timer_s_ - dt : 0.0f;

  // === BLOCK C: Unified split-range logic ===
  // Parameters differ by heat source mode; the ON/OFF/anticipation logic is shared.
  //   PRIMARY_HYDRO: high thresholds + debounce timer — electric is a late supplement
  //   PRIMARY_ELEC:  low thresholds + no delay     — electric is the primary source
  const float on_threshold = hydraulic_available_ ? SPLIT_ELECTRIC_ON : SPLIT_ELECTRIC_ON_DEG;
  const float off_threshold = hydraulic_available_ ? SPLIT_ELECTRIC_OFF : SPLIT_ELECTRIC_OFF_DEG;
  const float on_delay = hydraulic_available_ ? ELECTRIC_ON_DELAY_S : 0.0f;
  const float dt_on_guard = hydraulic_available_ ? ELECTRIC_DT_ON : 0.0f;

  // ON timer: demand must stay above threshold (and T must be far enough from setpoint)
  // for on_delay seconds before the relay closes. In PRIMARY_ELEC, on_delay=0 → immediate.
  if (u > on_threshold && current_temperature_ < (effective_target - dt_on_guard))
    electric_on_timer_s_ += dt;
  else
    electric_on_timer_s_ = 0.0f;

  if (!electric_on_ && electric_on_timer_s_ >= on_delay)
  {
    electric_on_ = true;
    electric_settle_timer_s_ = ELECTRIC_SETTLE_S;
  }

  // OFF: demand below hysteresis threshold, setpoint reached, or predictive shutoff.
  // Predictive shutoff: if temperature is rising and will overshoot setpoint within
  // ELECTRIC_OFF_ANTICIPATION_S seconds (thermal lag after relay opens), cut off early.
  // A 0.02°C/s deadband on the slope filters noise-driven false shutoffs.
  const float rise_rate = -pid_.GetDerivative(); // positive when T is climbing
  const bool will_overshoot = (rise_rate > 0.02f) &&
                              (current_temperature_ + rise_rate * ELECTRIC_OFF_ANTICIPATION_S >= effective_target);

  if (u < off_threshold || current_temperature_ >= effective_target || will_overshoot)
  {
    electric_on_ = false;
    electric_on_timer_s_ = 0.0f;
    electric_settle_timer_s_ = 0.0f;
  }

  // === BLOCK D: Apply outputs + periodic debug log ===
  // Hydraulic: proportional to demand on [0, SPLIT_ELECTRIC_ON], saturated at 100% above.
  // In PRIMARY_ELEC mode (no hydraulic source), pump stays off.
  if (hydraulic_available_)
  {
    float hydro_pct = fminf(u / SPLIT_ELECTRIC_ON * 100.0f, 100.0f);
    hydraulic_heater_->SetPower((uint8_t)hydro_pct);
  }
  else
  {
    hydraulic_heater_->SetPower(0);
  }

  electric_heater_->SetPower(electric_on_ ? 1.0f : 0.0f);

  uint8_t hydro_power = hydraulic_heater_->GetPower();

  // Log target and current temperature every 2 seconds
  debug_log_timer_s_ += dt;
  if (debug_log_timer_s_ >= 2.0f)
  {
    debug_log_timer_s_ = 0.0f;
    Logger::Info("TempMgr: target=%FC  T=%FC  u=%F%%  hydro=%u%%  elec=%s%s",
                 effective_target, current_temperature_, u, hydro_power,
                 electric_on_ ? "ON" : "OFF",
                 electric_settle_timer_s_ > 0.0f ? " (settling)" : "");
  }

  Logger::Debug("TempMgr: sp=%F T=%F u=%F%% hydro=%u%% freeze=%d | %s | elec=%s timer=%Fs settle=%Fs",
                effective_target, current_temperature_, u, hydro_power, (int)freeze_int,
                hydraulic_available_ ? "PRIMARY_HYDRO" : "PRIMARY_ELEC",
                electric_on_ ? "ON" : "OFF",
                electric_on_timer_s_, electric_settle_timer_s_);
}

void TemperatureManager::ResetControl()
{
  pid_.Reset();
  electric_on_ = false;
  electric_on_timer_s_ = 0.0f;
  electric_settle_timer_s_ = 0.0f;
  electric_heater_->SetPower(0.0f);
  hydraulic_heater_->SetPower(0);
  Logger::Info("TemperatureManager: control reset (phase transition)");
}

void TemperatureManager::PrintDebug() const
{
  Logger::Info("[TempMgr] err=%F P=%F I=%F D=%F u=%F%% | mode=%s | elec=%s | timer=%Fs",
               pid_.GetLastError(),
               pid_.GetProportionalTerm(),
               pid_.GetIntegralTerm(),
               pid_.GetDerivativeTerm(),
               pid_.GetLastOutput(),
               hydraulic_available_ ? "PRIMARY_HYDRO" : "PRIMARY_ELEC",
               electric_on_ ? "ON" : "OFF",
               electric_on_timer_s_);
}

void TemperatureManager::SetTargetTemperature(float temperature)
{
  temperature = constrain(temperature, 20.0f, 45.0f);
  if (fabsf(temperature - params_.temperature_target) < 1.0f)
    return;

  Logger::Info("TemperatureManager: target %FC -> %FC, resetting PID",
               params_.temperature_target, temperature);
  pid_.Reset();
  electric_on_ = false;
  electric_on_timer_s_ = 0.0f;
  electric_settle_timer_s_ = 0.0f;
  params_.temperature_target = temperature;
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
  // Reset timers and electric state when switching mode to avoid stale state
  electric_on_ = false;
  electric_on_timer_s_ = 0.0f;
  electric_settle_timer_s_ = 0.0f;
  Logger::Info("TemperatureManager: hydraulic %s — switching to %s mode",
               available ? "available" : "unavailable",
               available ? "PRIMARY_HYDRO" : "PRIMARY_ELEC");
}

void TemperatureManager::SetElectricEnabled(bool enabled)
{
  if (electric_enabled_ == enabled)
    return;
  electric_enabled_ = enabled;
  if (!enabled)
  {
    electric_heater_->SetPower(0.0f);
    electric_on_ = false;
    electric_on_timer_s_ = 0.0f;
    electric_settle_timer_s_ = 0.0f;
    pid_.Reset();
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
    electric_heater_->SetPower(0.0f);
    electric_on_ = false;
    electric_on_timer_s_ = 0.0f;
    electric_settle_timer_s_ = 0.0f;
  }
  Logger::Info("TemperatureManager: fan %s", active ? "active" : "inactive — electric blocked");
}

void TemperatureManager::SetOperatingMode(OperatingMode mode)
{
  if (operating_mode_ == mode)
    return;
  operating_mode_ = mode;
  Logger::Info("TemperatureManager: mode -> %s",
               mode == OperatingMode::ECO ? "ECO" : "PERFORMANCE");
}
