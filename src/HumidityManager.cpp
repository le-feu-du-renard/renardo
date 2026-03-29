#include "HumidityManager.h"
#include "Logger.h"

HumidityManager::HumidityManager(AirDamper *air_damper)
    : air_damper_(air_damper),
      target_humidity_(0.0f),
      current_inlet_humidity_(0.0f),
      action_next_allowed_ms_(0) {}

void HumidityManager::Begin()
{
  target_humidity_        = 0.0f;
  current_inlet_humidity_ = 0.0f;
  action_next_allowed_ms_ = 0;
  air_damper_->Close();
  Logger::Info("HumidityManager: initialized (damper closed)");
}

void HumidityManager::Update(float inlet_humidity, float /*outlet_humidity*/)
{
  current_inlet_humidity_ = inlet_humidity;

  // No control: keep damper closed
  if (target_humidity_ <= 0.0f)
  {
    air_damper_->Close();
    return;
  }

  if (!IsActionAllowed()) return;

  // Open damper when humidity exceeds target + deadband (evacuate moisture)
  if (inlet_humidity > target_humidity_ + kDeadband)
  {
    if (!air_damper_->IsOpen())
    {
      air_damper_->Open();
      ArmCooldown();
      Logger::Info("HumidityManager: damper opened (humidity=%F%% > threshold=%F%%)",
                   inlet_humidity, target_humidity_);
    }
  }
  // Close damper when humidity drops at or below target
  else if (inlet_humidity <= target_humidity_)
  {
    if (air_damper_->IsOpen())
    {
      air_damper_->Close();
      ArmCooldown();
      Logger::Info("HumidityManager: damper closed (humidity=%F%% <= target=%F%%)",
                   inlet_humidity, target_humidity_);
    }
  }
}

void HumidityManager::SetTargetHumidity(float target)
{
  target = constrain(target, 0.0f, 100.0f);
  target_humidity_ = target;
  Logger::Info("HumidityManager: target set to %F%%", target);
}

bool HumidityManager::IsHumidityTargetReached() const
{
  if (target_humidity_ <= 0.0f) return true;
  return (current_inlet_humidity_ <= target_humidity_);
}

void HumidityManager::ResetCooldown()
{
  action_next_allowed_ms_ = 0;
}

bool HumidityManager::IsActionAllowed() const
{
  return millis() >= action_next_allowed_ms_;
}

void HumidityManager::ArmCooldown()
{
  action_next_allowed_ms_ = millis() + kCooldownMs;
}
