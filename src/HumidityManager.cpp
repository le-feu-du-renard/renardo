#include "HumidityManager.h"
#include "Logger.h"

HumidityManager::HumidityManager(AirDamper *air_damper)
    : air_damper_(air_damper),
      mode_(Mode::kDisabled),
      force_open_(ForceOpen::kNone),
      target_humidity_(0.0f),
      current_inlet_humidity_(0.0f),
      action_next_allowed_ms_(0) {}

void HumidityManager::Begin()
{
  mode_                   = Mode::kDisabled;
  force_open_             = ForceOpen::kNone;
  target_humidity_        = 0.0f;
  current_inlet_humidity_ = 0.0f;
  action_next_allowed_ms_ = 0;
  air_damper_->Close();
  Logger::Info("HumidityManager: initialized (damper closed)");
}

void HumidityManager::Update(float inlet_humidity)
{
  current_inlet_humidity_ = inlet_humidity;

  // Above the mode, and before the threshold logic gets a look at a reading it
  // has no business acting on: under kPurge the humidity is by definition not to
  // be trusted, and under the other two the register is already answering a
  // question the mode did not ask.
  if (force_open_ != ForceOpen::kNone)
  {
    air_damper_->Open();
    return;
  }

  if (mode_ == Mode::kDisabled)
  {
    air_damper_->Close();
    return;
  }

  if (mode_ == Mode::kForceOpen)
  {
    air_damper_->Open();
    return;
  }

  // kThreshold: open when humidity exceeds target + deadband, close when at or below target
  if (!IsActionAllowed()) return;

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

bool HumidityManager::SetMode(Mode mode)
{
  if (mode == mode_) return false;
  const char *name = (mode == Mode::kDisabled)  ? "kDisabled"
                   : (mode == Mode::kForceOpen)  ? "kForceOpen"
                                                 : "kThreshold";
  Logger::Info("HumidityManager: mode -> %s", name);
  mode_ = mode;
  return true;
}

const char *HumidityManager::ForceOpenName(ForceOpen reason)
{
  switch (reason)
  {
    case ForceOpen::kPurge:      return "purge";
    case ForceOpen::kOverheat:   return "overheat";
    case ForceOpen::kAirRenewal: return "air renewal";
    default:                     return "none";
  }
}

bool HumidityManager::SetForceOpen(ForceOpen reason)
{
  if (reason == force_open_) return false;

  ForceOpen previous = force_open_;
  force_open_ = reason;

  // The cooldown is armed against threshold chatter, and the override has just
  // moved the register without consulting it. Clearing it lets kThreshold act on
  // the first good reading after the reason lifts, rather than sitting on a
  // wide-open register for another ten seconds.
  if (force_open_ == ForceOpen::kNone)
  {
    ResetCooldown();
  }

  Logger::Info("HumidityManager: force open %s -> %s",
               ForceOpenName(previous), ForceOpenName(force_open_));
  return true;
}

void HumidityManager::SetTargetHumidity(float target)
{
  target = constrain(target, 0.0f, 100.0f);
  if (fabsf(target - target_humidity_) < 1.0f) return;
  Logger::Debug("HumidityManager: target %F%% -> %F%%", target_humidity_, target);
  target_humidity_ = target;
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
