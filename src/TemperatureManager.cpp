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
    electric_enabled_(true),
    operating_mode_(OperatingMode::PERFORMANCE),
    reduced_mode_active_(false),
    eco_night_start_hour_(DEFAULT_ECO_NIGHT_START_HOUR),
    eco_night_end_hour_(DEFAULT_ECO_NIGHT_END_HOUR),
    eco_night_percentage_(DEFAULT_ECO_NIGHT_TARGET_PERCENTAGE),
    current_hour_(12) {  // Default to noon
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

  // Determine effective target temperature based on operating mode
  float effective_target = params_.temperature_target;

  // ECO mode: reduce target during night hours (20h-10h by default)
  if (operating_mode_ == OperatingMode::ECO) {
    bool is_night = IsNightMode();

    // Update reduced mode status and log transitions
    if (is_night && !reduced_mode_active_) {
      reduced_mode_active_ = true;
      Log.notice("ECO mode: Entering night mode - reducing target to %.0f%% (%.1f°C)",
                 eco_night_percentage_, params_.temperature_target * (eco_night_percentage_ / 100.0f));
    } else if (!is_night && reduced_mode_active_) {
      reduced_mode_active_ = false;
      Log.notice("ECO mode: Exiting night mode - returning to full target (%.1f°C)",
                 params_.temperature_target);
    }

    // Apply night percentage if in night mode
    if (reduced_mode_active_) {
      effective_target = params_.temperature_target * (eco_night_percentage_ / 100.0f);
    }
  } else {
    // PERFORMANCE mode: always use full target
    if (reduced_mode_active_) {
      reduced_mode_active_ = false;
      Log.notice("Exiting night mode (switched to PERFORMANCE mode)");
    }
  }

  // === Hydraulic Heater Control ===
  float hydraulic_output = 0.0f;

  if (hydraulic_enabled_) {
    // Compute PID output with effective target
    hydraulic_output = hydraulic_pid_.Compute(effective_target, current_temperature_, dt);

    // Set hydraulic heater power
    hydraulic_heater_->SetPower((uint8_t)(hydraulic_output + 0.5f));  // Round to nearest integer

    Log.verbose("Hydraulic PID: setpoint=%.1f°C, temp=%.1f°C, output=%.1f%%, P=%.2f, I=%.2f, D=%.2f",
               effective_target, current_temperature_, hydraulic_output,
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
    // Compute PID output with effective target
    electric_output = electric_pid_.Compute(effective_target, current_temperature_, dt);

    // Convert to binary (on/off) with threshold at 0.5
    float electric_power = (electric_output > 0.5f) ? 1.0f : 0.0f;
    electric_heater_->SetPower(electric_power);

    Log.verbose("Electric PID: setpoint=%.1f°C, temp=%.1f°C, output=%.2f, power=%s, P=%.2f, I=%.2f, D=%.2f",
               effective_target, current_temperature_, electric_output,
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

bool TemperatureManager::IsNightMode() const {
  // Check if current hour is within night mode window
  // Handle wrap-around (e.g., 20h-10h crosses midnight)
  if (eco_night_start_hour_ < eco_night_end_hour_) {
    // Normal case: e.g., 2h-6h (no wrap-around)
    return (current_hour_ >= eco_night_start_hour_ && current_hour_ < eco_night_end_hour_);
  } else {
    // Wrap-around case: e.g., 20h-10h (crosses midnight)
    return (current_hour_ >= eco_night_start_hour_ || current_hour_ < eco_night_end_hour_);
  }
}

void TemperatureManager::SetOperatingMode(OperatingMode mode) {
  if (operating_mode_ != mode) {
    operating_mode_ = mode;
    reduced_mode_active_ = false;  // Reset night mode state when changing mode

    Log.notice("Operating mode changed to: %s",
               mode == OperatingMode::ECO ? "ECO (night mode)" : "PERFORMANCE");
  }
}

float TemperatureManager::GetEffectiveTargetTemperature() const {
  if (reduced_mode_active_) {
    return params_.temperature_target * (eco_night_percentage_ / 100.0f);
  }
  return params_.temperature_target;
}

void TemperatureManager::SetEcoNightStartHour(uint8_t hour) {
  if (hour > 23) hour = 23;  // Clamp to valid range
  if (eco_night_start_hour_ != hour) {
    eco_night_start_hour_ = hour;
    Log.notice("ECO night start hour set to: %02d:00", hour);
  }
}

void TemperatureManager::SetEcoNightEndHour(uint8_t hour) {
  if (hour > 23) hour = 23;  // Clamp to valid range
  if (eco_night_end_hour_ != hour) {
    eco_night_end_hour_ = hour;
    Log.notice("ECO night end hour set to: %02d:00", hour);
  }
}

void TemperatureManager::SetEcoNightPercentage(float percentage) {
  // Clamp to reasonable range (50% to 95%)
  if (percentage < 50.0f) percentage = 50.0f;
  if (percentage > 95.0f) percentage = 95.0f;

  if (eco_night_percentage_ != percentage) {
    eco_night_percentage_ = percentage;
    Log.notice("ECO night percentage set to: %.0f%%", percentage);
  }
}
