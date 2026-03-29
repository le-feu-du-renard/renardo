#include "TemperatureManager.h"
#include "Logger.h"

TemperatureManager::TemperatureManager(ElectricHeater  *electric_heater,
                                       HydraulicHeater *hydraulic_heater)
    : electric_heater_(electric_heater),
      hydraulic_heater_(hydraulic_heater),
      params_(),
      hydraulic_pid_(DEFAULT_HYDRAULIC_KP, DEFAULT_HYDRAULIC_KI, DEFAULT_HYDRAULIC_KD,
                     0.0f, 100.0f, DEFAULT_PID_INTEGRAL_MAX, DEFAULT_PID_DERIVATIVE_FILTER),
      electric_pid_(DEFAULT_ELECTRIC_KP, DEFAULT_ELECTRIC_KI, DEFAULT_ELECTRIC_KD,
                    0.0f, 1.0f, DEFAULT_PID_INTEGRAL_MAX, DEFAULT_PID_DERIVATIVE_FILTER),
      current_temperature_(0.0f),
      last_update_ms_(0),
      hydraulic_enabled_(DEFAULT_HYDRAULIC_ENABLED),
      electric_enabled_(DEFAULT_ELECTRIC_ENABLED),
      operating_mode_(OperatingMode::PERFORMANCE),
      current_hour_(0) {}

void TemperatureManager::Begin()
{
  electric_heater_->Begin();
  hydraulic_heater_->Begin();

  hydraulic_pid_.SetParameters(params_.hydraulic_kp, params_.hydraulic_ki, params_.hydraulic_kd);
  hydraulic_pid_.SetOutputLimits(0.0f, 100.0f);
  hydraulic_pid_.SetIntegralLimit(params_.pid_integral_max);
  hydraulic_pid_.SetDerivativeFilter(params_.pid_derivative_filter);

  electric_pid_.SetParameters(params_.electric_kp, params_.electric_ki, params_.electric_kd);
  electric_pid_.SetOutputLimits(0.0f, 1.0f);
  electric_pid_.SetIntegralLimit(params_.pid_integral_max);
  electric_pid_.SetDerivativeFilter(params_.pid_derivative_filter);

  hydraulic_pid_.Reset();
  electric_pid_.Reset();

  last_update_ms_ = millis();

  Logger::Info("TemperatureManager: initialized");
  Logger::Info("Hydraulic PID: Kp=%F Ki=%F Kd=%F",
               params_.hydraulic_kp, params_.hydraulic_ki, params_.hydraulic_kd);
  Logger::Info("Electric PID: Kp=%F Ki=%F Kd=%F",
               params_.electric_kp, params_.electric_ki, params_.electric_kd);
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

  // === Hydraulic heater (always available in both modes) ===
  if (hydraulic_enabled_)
  {
    float output = hydraulic_pid_.Compute(effective_target, current_temperature_, dt);
    hydraulic_heater_->SetPower(static_cast<uint8_t>(output + 0.5f));
    Logger::Debug("HydraulicPID: sp=%F T=%F out=%F%%",
                  effective_target, current_temperature_, output);
  }
  else
  {
    hydraulic_heater_->SetPower(0);
    hydraulic_pid_.Reset();
  }

  // === Electric heater (available in all modes) ===
  bool electric_active = electric_enabled_;
  if (electric_active)
  {
    float output = electric_pid_.Compute(effective_target, current_temperature_, dt);
    float power  = (output > 0.5f) ? 1.0f : 0.0f;
    electric_heater_->SetPower(power);
    Logger::Debug("ElectricPID: sp=%F T=%F out=%F (%s)",
                  effective_target, current_temperature_, output,
                  power > 0.5f ? "ON" : "OFF");
  }
  else
  {
    electric_heater_->SetPower(0.0f);
    electric_pid_.Reset();
  }
}

void TemperatureManager::SetTargetTemperature(float temperature)
{
  temperature = constrain(temperature, 20.0f, 45.0f);
  if (fabsf(temperature - params_.temperature_target) < 1.0f) return;

  if (fabs(temperature - params_.temperature_target) > 1.0f)
  {
    Logger::Info("TemperatureManager: target %F C -> %F C, resetting PIDs",
                 params_.temperature_target, temperature);
    hydraulic_pid_.Reset();
    electric_pid_.Reset();
  }
  else
  {
    Logger::Debug("TemperatureManager: target %F C -> %F C",
                  params_.temperature_target, temperature);
  }
  params_.temperature_target = temperature;
}

float TemperatureManager::GetEffectiveTargetTemperature() const
{
  if (IsEcoWindowActive())
    return params_.temperature_target * (DEFAULT_ECO_NIGHT_TARGET_PERCENTAGE / 100.0f);
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

void TemperatureManager::SetHydraulicEnabled(bool enabled)
{
  if (hydraulic_enabled_ == enabled) return;
  hydraulic_enabled_ = enabled;
  if (!enabled)
  {
    hydraulic_heater_->SetPower(0);
    hydraulic_pid_.Reset();
  }
  Logger::Info("TemperatureManager: hydraulic heater %s", enabled ? "enabled" : "disabled");
}

void TemperatureManager::SetElectricEnabled(bool enabled)
{
  if (electric_enabled_ == enabled) return;
  electric_enabled_ = enabled;
  if (!enabled)
  {
    electric_heater_->SetPower(0.0f);
    electric_pid_.Reset();
  }
  Logger::Info("TemperatureManager: electric heater %s", enabled ? "enabled" : "disabled");
}

void TemperatureManager::SetOperatingMode(OperatingMode mode)
{
  if (operating_mode_ == mode) return;
  operating_mode_ = mode;
  Logger::Info("TemperatureManager: mode -> %s",
               mode == OperatingMode::ECO ? "ECO" : "PERFORMANCE");
}
