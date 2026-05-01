#include "TemperatureManager.h"
#include "Logger.h"

TemperatureManager::TemperatureManager(ElectricHeater  *electric_heater,
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
               hydraulic_available_ ? "hydraulic+electric" : "electric only");
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
    electric_on_            = false;
    electric_on_timer_s_    = 0.0f;
    electric_settle_timer_s_ = 0.0f;
    pid_.Reset();
    Logger::Warning("TempMgr: SAFETY CUTOFF T=%.1f > %.1f°C — electric OFF",
                    current_temperature_, TEMPERATURE_SAFETY_MAX);
    return;
  }

  // === BLOCK A+: Ventilation interlock ===
  // Block the electric heater if the fan is not confirmed active.
  if (!fan_active_)
  {
    electric_heater_->SetPower(0.0f);
    electric_on_             = false;
    electric_on_timer_s_     = 0.0f;
    electric_settle_timer_s_ = 0.0f;
    Logger::Warning("TempMgr: fan not active — electric heater blocked");
    return;
  }

  // === BLOCK B: Single PID with conditional anti-windup ===
  // The integral is frozen in three situations:
  //   1. Output saturated (u at 0% or 100%) — classic anti-windup.
  //   2. Electric heater pending (timer > 0 but relay not yet ON) — avoids windup
  //      during the ~30s delay before the heater activates.
  //   3. Settle window after heater turns ON — avoids windup during the ~30s
  //      thermal lag before the heater's heat actually reaches the sensor.
  float last_u      = pid_.GetLastOutput();
  bool  freeze_int  = (last_u >= 100.0f) ||
                      (last_u <= 0.0f)   ||
                      (electric_on_timer_s_ > 0.0f && !electric_on_) ||
                      (electric_settle_timer_s_ > 0.0f);

  float u = electric_enabled_
              ? pid_.Compute(effective_target, current_temperature_, dt, freeze_int)
              : 0.0f;

  if (!electric_enabled_)
  {
    electric_heater_->SetPower(0.0f);
    electric_on_             = false;
    electric_on_timer_s_     = 0.0f;
    electric_settle_timer_s_ = 0.0f;
    pid_.Reset();
    return;
  }

  // Track the settle window: count down from ELECTRIC_SETTLE_S after heater turns ON.
  if (electric_settle_timer_s_ > 0.0f)
    electric_settle_timer_s_ = (electric_settle_timer_s_ > dt) ? electric_settle_timer_s_ - dt : 0.0f;

  // === BLOCK C: Split-Range distribution ===
  if (hydraulic_available_)
  {
    // Normal mode: hydraulic provides the base load (manual).
    // Electric activates only after sustained high demand AND temperature is still below setpoint.
    if (u > SPLIT_ELECTRIC_ON && current_temperature_ < (effective_target - ELECTRIC_DT_ON))
    {
      electric_on_timer_s_ += dt;
    }
    else
    {
      // Reset timer if demand drops or temperature is close enough to setpoint
      electric_on_timer_s_ = 0.0f;
    }

    if (!electric_on_ && electric_on_timer_s_ >= ELECTRIC_ON_DELAY_S)
    {
      electric_on_             = true;
      electric_settle_timer_s_ = ELECTRIC_SETTLE_S;  // Start settle window
    }

    // Turn off: demand dropped below hysteresis threshold or setpoint reached
    if (u < SPLIT_ELECTRIC_OFF || current_temperature_ >= effective_target)
    {
      electric_on_             = false;
      electric_on_timer_s_     = 0.0f;
      electric_settle_timer_s_ = 0.0f;
    }
  }
  else
  {
    // Degraded mode: hydraulic is unavailable, electric is the sole heat source.
    // Simple hysteresis control around the PID demand.
    if (!electric_on_ && u > SPLIT_ELECTRIC_ON_DEG)
    {
      electric_on_             = true;
      electric_settle_timer_s_ = ELECTRIC_SETTLE_S;
    }

    if (u < SPLIT_ELECTRIC_OFF_DEG)
    {
      electric_on_             = false;
      electric_settle_timer_s_ = 0.0f;
    }
  }

  // === BLOCK D: Apply outputs + periodic debug log ===
  electric_heater_->SetPower(electric_on_ ? 1.0f : 0.0f);

  // Log target and current temperature every 2 seconds
  debug_log_timer_s_ += dt;
  if (debug_log_timer_s_ >= 2.0f)
  {
    debug_log_timer_s_ = 0.0f;
    Logger::Info("TempMgr: target=%.1f°C  T=%.1f°C  u=%.1f%%  elec=%s%s",
                 effective_target, current_temperature_, u,
                 electric_on_ ? "ON" : "OFF",
                 electric_settle_timer_s_ > 0.0f ? " (settling)" : "");
  }

  Logger::Debug("TempMgr: sp=%.1f T=%.1f u=%.1f%% freeze=%d | %s | elec=%s timer=%.1fs settle=%.1fs",
                effective_target, current_temperature_, u, (int)freeze_int,
                hydraulic_available_ ? "NORMAL" : "DEGRADED",
                electric_on_ ? "ON" : "OFF",
                electric_on_timer_s_, electric_settle_timer_s_);
}

void TemperatureManager::PrintDebug() const
{
  Logger::Info("[TempMgr] err=%.2f P=%.2f I=%.2f D=%.2f u=%.1f%% | "
               "mode=%s | elec=%s | timer=%.1fs",
               pid_.GetLastError(),
               pid_.GetProportionalTerm(),
               pid_.GetIntegralTerm(),
               pid_.GetDerivativeTerm(),
               pid_.GetLastOutput(),
               hydraulic_available_ ? "NORMAL" : "DEGRADED",
               electric_on_ ? "ON" : "OFF",
               electric_on_timer_s_);
}

void TemperatureManager::SetTargetTemperature(float temperature)
{
  temperature = constrain(temperature, 20.0f, 45.0f);
  if (fabsf(temperature - params_.temperature_target) < 1.0f) return;

  Logger::Info("TemperatureManager: target %.1f°C -> %.1f°C, resetting PID",
               params_.temperature_target, temperature);
  pid_.Reset();
  electric_on_             = false;
  electric_on_timer_s_     = 0.0f;
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
  if (operating_mode_ != OperatingMode::ECO) return false;
  return (current_hour_ >= ECO_START_HOUR) || (current_hour_ < ECO_END_HOUR);
}

bool TemperatureManager::IsTemperatureInRange() const
{
  return fabs(current_temperature_ - params_.temperature_target) <= 2.0f;
}

void TemperatureManager::SetHydraulicAvailable(bool available)
{
  if (hydraulic_available_ == available) return;
  hydraulic_available_ = available;
  // Reset timers and electric state when switching mode to avoid stale state
  electric_on_             = false;
  electric_on_timer_s_     = 0.0f;
  electric_settle_timer_s_ = 0.0f;
  Logger::Info("TemperatureManager: hydraulic %s — switching to %s mode",
               available ? "available" : "unavailable",
               available ? "NORMAL" : "DEGRADED");
}

void TemperatureManager::SetElectricEnabled(bool enabled)
{
  if (electric_enabled_ == enabled) return;
  electric_enabled_ = enabled;
  if (!enabled)
  {
    electric_heater_->SetPower(0.0f);
    electric_on_             = false;
    electric_on_timer_s_     = 0.0f;
    electric_settle_timer_s_ = 0.0f;
    pid_.Reset();
  }
  Logger::Info("TemperatureManager: electric heating %s", enabled ? "enabled" : "disabled");
}

void TemperatureManager::SetFanActive(bool active)
{
  if (fan_active_ == active) return;
  fan_active_ = active;
  if (!active)
  {
    electric_heater_->SetPower(0.0f);
    electric_on_             = false;
    electric_on_timer_s_     = 0.0f;
    electric_settle_timer_s_ = 0.0f;
  }
  Logger::Info("TemperatureManager: fan %s", active ? "active" : "inactive — electric blocked");
}

void TemperatureManager::SetOperatingMode(OperatingMode mode)
{
  if (operating_mode_ == mode) return;
  operating_mode_ = mode;
  Logger::Info("TemperatureManager: mode -> %s",
               mode == OperatingMode::ECO ? "ECO" : "PERFORMANCE");
}
