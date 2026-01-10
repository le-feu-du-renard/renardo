#include "TemperatureManager.h"

TemperatureManager::TemperatureManager(ElectricHeater* electric_heater, HydraulicHeater* hydraulic_heater)
  : electric_heater_(electric_heater),
    hydraulic_heater_(hydraulic_heater),
    params_(),
    heating_action_next_allowed_ms_(0),
    current_temperature_(0.0f) {
}

void TemperatureManager::Begin() {
  electric_heater_->Begin();
  hydraulic_heater_->Begin();

  heating_action_next_allowed_ms_ = 0;

  Serial.println("Temperature manager initialized");
}

void TemperatureManager::Update(float current_temperature) {
  current_temperature_ = current_temperature;

  // Check if heating action is allowed
  if (!IsHeatingActionAllowed()) {
    return;
  }

  // Temperature too high - decrease heating
  if (IsTemperatureTooHigh()) {
    Serial.println("Temperature too high - decreasing heating");
    if (DecreaseHeating()) {
      ArmHeatingActionCooldown();
    }
    return;
  }

  // Temperature too low - increase heating
  if (IsTemperatureTooLow()) {
    Serial.println("Temperature too low - increasing heating");
    if (IncreaseHeating()) {
      ArmHeatingActionCooldown();
    }
    return;
  }

  // Temperature OK - no action needed
}

void TemperatureManager::SetTargetTemperature(float temperature) {
  // Clamp to valid range
  if (temperature < 20.0f) temperature = 20.0f;
  if (temperature > 45.0f) temperature = 45.0f;

  params_.temperature_target = temperature;
}

void TemperatureManager::ResetCooldown() {
  heating_action_next_allowed_ms_ = 0;
}

bool TemperatureManager::IsTemperatureTooHigh() const {
  return current_temperature_ > (params_.temperature_target + params_.temperature_deadband);
}

bool TemperatureManager::IsTemperatureTooLow() const {
  return current_temperature_ < (params_.temperature_target - params_.temperature_deadband);
}

bool TemperatureManager::IsTemperatureInRange() const {
  return !IsTemperatureTooHigh() && !IsTemperatureTooLow();
}

bool TemperatureManager::IsHeatingActionAllowed() const {
  return millis() >= heating_action_next_allowed_ms_;
}

void TemperatureManager::ArmHeatingActionCooldown() {
  heating_action_next_allowed_ms_ = millis() + (unsigned long)(params_.heating_action_min_wait_s * 1000.0f);

  Serial.print("Heating action cooldown armed - next allowed ms: ");
  Serial.println(heating_action_next_allowed_ms_);
}

bool TemperatureManager::IncreaseHeating() {
  // Priority: Hydraulic first, then electric
  float hydraulic_power = hydraulic_heater_->GetPower();

  if (hydraulic_power < 100.0f) {
    // Increase hydraulic heater
    float step = CalculateStep();
    float new_power = hydraulic_power + step;
    if (new_power > 100.0f) new_power = 100.0f;

    hydraulic_heater_->SetPower(new_power);

    Serial.print("Increased hydraulic heater: ");
    Serial.print(hydraulic_power);
    Serial.print("% -> ");
    Serial.print(new_power);
    Serial.println("%");
    return true;
  } else {
    // Hydraulic at max, turn on electric heater
    float electric_power = electric_heater_->GetPower();

    if (electric_power < 1.0f) {
      electric_heater_->SetPower(1.0f);
      Serial.println("Turned on electric heater");
      return true;
    } else {
      Serial.println("Both heaters at maximum - cannot increase");
      return false;
    }
  }
}

bool TemperatureManager::DecreaseHeating() {
  // Priority: Electric first, then hydraulic
  float electric_power = electric_heater_->GetPower();

  if (electric_power > 0.0f) {
    // Turn off electric heater
    electric_heater_->SetPower(0.0f);
    Serial.println("Turned off electric heater");
    return true;
  } else {
    // Electric off, decrease hydraulic heater
    float hydraulic_power = hydraulic_heater_->GetPower();

    if (hydraulic_power > 0.0f) {
      float step = CalculateStep();
      float new_power = hydraulic_power - step;
      if (new_power < 0.0f) new_power = 0.0f;

      hydraulic_heater_->SetPower(new_power);

      Serial.print("Decreased hydraulic heater: ");
      Serial.print(hydraulic_power);
      Serial.print("% -> ");
      Serial.print(new_power);
      Serial.println("%");
      return true;
    } else {
      Serial.println("Both heaters at minimum - cannot decrease");
      return false;
    }
  }
}

float TemperatureManager::CalculateStep() const {
  float diff_abs = fabs(params_.temperature_target - current_temperature_);

  // No step if within deadband
  if (diff_abs <= params_.temperature_deadband) {
    return 0.0f;
  }

  // Calculate scaled difference
  float diff_scaled = (diff_abs - params_.temperature_deadband) / params_.heater_full_scale_delta;

  // Clamp to 0-1
  float k = diff_scaled;
  if (k < 0.0f) k = 0.0f;
  if (k > 1.0f) k = 1.0f;

  // Calculate adaptive step
  float step = params_.heater_step_min + (params_.heater_step_max - params_.heater_step_min) * k;

  return step;
}
