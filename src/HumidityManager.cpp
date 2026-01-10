#include "HumidityManager.h"

HumidityManager::HumidityManager(AirRecyclingManager* air_recycling_manager)
  : air_recycling_manager_(air_recycling_manager),
    params_(),
    target_humidity_(0.0f),
    current_inlet_humidity_(0.0f),
    current_outlet_humidity_(0.0f),
    action_next_allowed_ms_(0) {
}

void HumidityManager::Begin() {
  target_humidity_ = 0.0f;  // No control by default
  current_inlet_humidity_ = 0.0f;
  current_outlet_humidity_ = 0.0f;
  action_next_allowed_ms_ = 0;

  Serial.println("Humidity manager initialized");
}

void HumidityManager::Update(float inlet_humidity, float outlet_humidity) {
  current_inlet_humidity_ = inlet_humidity;
  current_outlet_humidity_ = outlet_humidity;

  // No control if target is 0
  if (target_humidity_ == 0.0f) {
    return;
  }

  // Check cooldown
  if (!IsActionAllowed()) {
    return;
  }

  // Mode: minimize humidity (target = -1)
  if (target_humidity_ < 0.0f) {
    // Maximize fresh air intake (recycling = 0%)
    if (DecreaseRecycling()) {
      ArmActionCooldown();
    }
    return;
  }

  // Mode: target specific humidity
  float error = current_outlet_humidity_ - target_humidity_;

  if (error > params_.humidity_deadband) {
    // Too humid: need more fresh air (decrease recycling)
    if (DecreaseRecycling()) {
      ArmActionCooldown();
      Serial.print("Humidity too high (");
      Serial.print(current_outlet_humidity_);
      Serial.print("% > ");
      Serial.print(target_humidity_);
      Serial.println("%) - decreasing recycling");
    }
  } else if (error < -params_.humidity_deadband) {
    // Too dry: can increase recycling (less fresh air)
    if (IncreaseRecycling()) {
      ArmActionCooldown();
      Serial.print("Humidity too low (");
      Serial.print(current_outlet_humidity_);
      Serial.print("% < ");
      Serial.print(target_humidity_);
      Serial.println("%) - increasing recycling");
    }
  }
}

void HumidityManager::SetTargetHumidity(float target) {
  // Clamp to valid range: -1 (min) or 0-100%
  if (target < -1.0f) target = -1.0f;
  if (target > 100.0f) target = 100.0f;

  target_humidity_ = target;

  Serial.print("Humidity target set to: ");
  if (target < 0.0f) {
    Serial.println("MINIMUM");
  } else if (target == 0.0f) {
    Serial.println("OFF (no control)");
  } else {
    Serial.print(target);
    Serial.println("%");
  }
}

bool HumidityManager::IsHumidityTargetReached() const {
  // No target = always reached
  if (target_humidity_ == 0.0f) {
    return true;
  }

  // Minimize mode: consider reached when recycling is at minimum
  // and outlet humidity is reasonably low (below 50%)
  if (target_humidity_ < 0.0f) {
    return air_recycling_manager_->GetRecyclingRate() <= 0.0f;
  }

  // Target mode: check if within deadband
  float error = fabs(current_outlet_humidity_ - target_humidity_);
  return error <= params_.humidity_deadband;
}

void HumidityManager::ResetCooldown() {
  action_next_allowed_ms_ = 0;
}

bool HumidityManager::IsActionAllowed() const {
  return millis() >= action_next_allowed_ms_;
}

void HumidityManager::ArmActionCooldown() {
  action_next_allowed_ms_ = millis() + (unsigned long)(params_.action_min_wait_s * 1000.0f);
}

bool HumidityManager::IncreaseRecycling() {
  float current_rate = air_recycling_manager_->GetRecyclingRate();

  // Already at max
  if (current_rate >= 100.0f) {
    return false;
  }

  float step = CalculateStep();
  float new_rate = current_rate + step;

  if (new_rate > 100.0f) new_rate = 100.0f;

  // Check if value actually changed
  if (new_rate == current_rate) {
    return false;
  }

  air_recycling_manager_->SetRecyclingRate(new_rate);

  Serial.print("Increased recycling: ");
  Serial.print(current_rate);
  Serial.print("% -> ");
  Serial.print(new_rate);
  Serial.println("%");

  return true;
}

bool HumidityManager::DecreaseRecycling() {
  float current_rate = air_recycling_manager_->GetRecyclingRate();

  // Already at min
  if (current_rate <= 0.0f) {
    return false;
  }

  float step = CalculateStep();
  float new_rate = current_rate - step;

  if (new_rate < 0.0f) new_rate = 0.0f;

  // Check if value actually changed
  if (new_rate == current_rate) {
    return false;
  }

  air_recycling_manager_->SetRecyclingRate(new_rate);

  Serial.print("Decreased recycling: ");
  Serial.print(current_rate);
  Serial.print("% -> ");
  Serial.print(new_rate);
  Serial.println("%");

  return true;
}

float HumidityManager::CalculateStep() const {
  // For minimize mode, use max step
  if (target_humidity_ < 0.0f) {
    return params_.recycling_step_max;
  }

  float error = fabs(current_outlet_humidity_ - target_humidity_);

  // No step if within deadband
  if (error <= params_.humidity_deadband) {
    return 0.0f;
  }

  // Scale step based on error magnitude (larger error = larger step)
  // Error range: deadband to 30% considered full scale
  float full_scale_error = 30.0f;
  float scaled_error = (error - params_.humidity_deadband) / full_scale_error;

  if (scaled_error < 0.0f) scaled_error = 0.0f;
  if (scaled_error > 1.0f) scaled_error = 1.0f;

  float step = params_.recycling_step_min +
               (params_.recycling_step_max - params_.recycling_step_min) * scaled_error;

  return step;
}
