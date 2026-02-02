#include "TemperatureManager.h"
#include "Logger.h"

TemperatureManager::TemperatureManager(ElectricHeater* electric_heater, HydraulicHeater* hydraulic_heater)
  : electric_heater_(electric_heater),
    hydraulic_heater_(hydraulic_heater),
    params_(),
    hydraulic_pid_(DEFAULT_HYDRAULIC_KP, DEFAULT_HYDRAULIC_KI, DEFAULT_HYDRAULIC_KD,
                   0.0f, 100.0f, DEFAULT_PID_INTEGRAL_MAX, DEFAULT_PID_DERIVATIVE_FILTER),
    electric_pid_(DEFAULT_ELECTRIC_KP, DEFAULT_ELECTRIC_KI, DEFAULT_ELECTRIC_KD,
                  0.0f, 1.0f, DEFAULT_PID_INTEGRAL_MAX, DEFAULT_PID_DERIVATIVE_FILTER),
    current_temperature_(0.0f),
    water_temperature_(0.0f),
    last_update_ms_(0),
    hydraulic_enabled_(true),
    electric_enabled_(true) {
}

void TemperatureManager::Begin() {
  electric_heater_->Begin();
  hydraulic_heater_->Begin();

  // Initialize PIDs with parameters
  hydraulic_pid_.SetParameters(params_.hydraulic_kp, params_.hydraulic_ki, params_.hydraulic_kd);
  hydraulic_pid_.SetOutputLimits(0.0f, 100.0f);
  hydraulic_pid_.SetIntegralLimit(params_.pid_integral_max);
  hydraulic_pid_.SetDerivativeFilter(params_.pid_derivative_filter);

  electric_pid_.SetParameters(params_.electric_kp, params_.electric_ki, params_.electric_kd);
  electric_pid_.SetOutputLimits(0.0f, 1.0f);
  electric_pid_.SetIntegralLimit(params_.pid_integral_max);
  electric_pid_.SetDerivativeFilter(params_.pid_derivative_filter);

  // Reset PID states
  hydraulic_pid_.Reset();
  electric_pid_.Reset();

  last_update_ms_ = millis();

  Log.notice("Temperature manager initialized with PID control");
  Log.notice("Hydraulic PID: Kp=%.2f, Ki=%.2f, Kd=%.2f", params_.hydraulic_kp, params_.hydraulic_ki, params_.hydraulic_kd);
  Log.notice("Electric PID: Kp=%.2f, Ki=%.2f, Kd=%.2f", params_.electric_kp, params_.electric_ki, params_.electric_kd);
}

void TemperatureManager::Update(float current_temperature) {
  current_temperature_ = current_temperature;

  // Calculate time delta
  unsigned long now = millis();
  float dt = (now - last_update_ms_) / 1000.0f;  // Convert to seconds
  last_update_ms_ = now;

  // Ensure valid dt
  if (dt <= 0.0f || dt > 10.0f) {
    Log.warning("Invalid dt (%.3f), skipping PID update", dt);
    return;
  }

  UpdateHeating();
}

void TemperatureManager::UpdateHeating() {
  float dt = 1.0f;  // Assume 1s control loop (actual dt calculated in Update)

  // === Hydraulic Heater Control ===
  float hydraulic_output = 0.0f;

  if (hydraulic_enabled_) {
    // Compute PID output
    hydraulic_output = hydraulic_pid_.Compute(params_.temperature_target, current_temperature_, dt);

    // Apply water temperature constraint
    // Only constrain if water temperature is valid (circulator running)
    if (IsWaterTemperatureValid()) {
      float min_water_temp = params_.temperature_target + params_.water_temp_margin;

      if (water_temperature_ < min_water_temp) {
        // Water too cold to heat the air effectively - disable hydraulic heater
        Log.verbose("Water temp constraint active: T_water=%.1f°C < T_min=%.1f°C - hydraulic output forced to 0",
                   water_temperature_, min_water_temp);
        hydraulic_output = 0.0f;

        // Reset PID integral to prevent windup when constrained
        hydraulic_pid_.Reset();
      }
    }

    // Set hydraulic heater power
    hydraulic_heater_->SetPower((uint8_t)(hydraulic_output + 0.5f));  // Round to nearest integer

    Log.verbose("Hydraulic PID: setpoint=%.1f°C, temp=%.1f°C, output=%.1f%%, P=%.2f, I=%.2f, D=%.2f",
               params_.temperature_target, current_temperature_, hydraulic_output,
               hydraulic_pid_.GetProportionalTerm(),
               hydraulic_pid_.GetIntegralTerm(),
               hydraulic_pid_.GetDerivativeTerm());
  } else {
    // Hydraulic disabled
    hydraulic_heater_->SetPower(0);
    hydraulic_pid_.Reset();
  }

  // === Electric Heater Control ===
  float electric_output = 0.0f;

  if (electric_enabled_) {
    // Compute PID output
    electric_output = electric_pid_.Compute(params_.temperature_target, current_temperature_, dt);

    // Convert to binary (on/off) with threshold at 0.5
    float electric_power = (electric_output > 0.5f) ? 1.0f : 0.0f;
    electric_heater_->SetPower(electric_power);

    Log.verbose("Electric PID: setpoint=%.1f°C, temp=%.1f°C, output=%.2f, power=%s, P=%.2f, I=%.2f, D=%.2f",
               params_.temperature_target, current_temperature_, electric_output,
               (electric_power > 0.5f) ? "ON" : "OFF",
               electric_pid_.GetProportionalTerm(),
               electric_pid_.GetIntegralTerm(),
               electric_pid_.GetDerivativeTerm());
  } else {
    // Electric disabled
    electric_heater_->SetPower(0.0f);
    electric_pid_.Reset();
  }
}

void TemperatureManager::SetTargetTemperature(float temperature) {
  // Clamp to valid range
  if (temperature < 20.0f) temperature = 20.0f;
  if (temperature > 45.0f) temperature = 45.0f;

  float old_target = params_.temperature_target;
  params_.temperature_target = temperature;

  // Reset PIDs when setpoint changes significantly
  if (fabs(temperature - old_target) > 1.0f) {
    Log.notice("Target temperature changed: %.1f°C -> %.1f°C - resetting PIDs", old_target, temperature);
    hydraulic_pid_.Reset();
    electric_pid_.Reset();
  }
}

bool TemperatureManager::IsTemperatureInRange() const {
  // Legacy method - use a simple tolerance
  float tolerance = 2.0f;  // °C
  return fabs(current_temperature_ - params_.temperature_target) <= tolerance;
}

void TemperatureManager::SetHydraulicEnabled(bool enabled) {
  if (hydraulic_enabled_ != enabled) {
    hydraulic_enabled_ = enabled;

    if (!enabled) {
      // Reset PID when disabling
      hydraulic_heater_->SetPower(0);
      hydraulic_pid_.Reset();
      Log.notice("Hydraulic heater disabled");
    } else {
      Log.notice("Hydraulic heater enabled");
    }
  }
}

void TemperatureManager::SetElectricEnabled(bool enabled) {
  if (electric_enabled_ != enabled) {
    electric_enabled_ = enabled;

    if (!enabled) {
      // Reset PID when disabling
      electric_heater_->SetPower(0.0f);
      electric_pid_.Reset();
      Log.notice("Electric heater disabled");
    } else {
      Log.notice("Electric heater enabled");
    }
  }
}

bool TemperatureManager::IsWaterTemperatureValid() const {
  // Water temperature is valid if:
  // 1. It's within reasonable range (5-95°C)
  // 2. Hydraulic heater is running (circulator active, power > 0)

  // Check if temperature is in valid range
  if (water_temperature_ < 5.0f || water_temperature_ > 95.0f) {
    return false;
  }

  // Check if circulator is running (hydraulic heater power > 0)
  uint8_t hydraulic_power = hydraulic_heater_->GetPower();
  if (hydraulic_power == 0) {
    return false;  // Circulator off, water is stagnant
  }

  return true;
}
