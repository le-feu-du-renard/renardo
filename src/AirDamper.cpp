#include "AirDamper.h"
#include "Logger.h"

AirDamper::AirDamper()
    : is_open_(false),
      count_(DAMPER_COUNT_DEFAULT),
      extraction_inverted_(DAMPER_EXTRACTION_INVERTED_DEFAULT),
      recycling_inverted_(DAMPER_RECYCLING_INVERTED_DEFAULT),
      extraction_("extraction"),
      recycling_("recycling"),
      both_closed_since_ms_(0),
      both_closed_(false),
      airflow_blocked_(false)
{
  // Start consistent with is_open_ = false, so travel detection is right from
  // the first sample rather than after the first command.
  PushTargets();
}

void AirDamper::ApplyConfig(const DamperConfig &config)
{
  count_ = config.count < 1 ? 1
           : (config.count > DAMPER_COUNT_MAX ? DAMPER_COUNT_MAX : config.count);
  extraction_inverted_ = config.extraction_inverted;
  recycling_inverted_  = config.recycling_inverted;

  extraction_.SetCalibration(config.extraction_raw_min, config.extraction_raw_max,
                             config.feedback_low_is_open);
  recycling_.SetCalibration(config.recycling_raw_min, config.recycling_raw_max,
                            config.feedback_low_is_open);

  // A direction changed from the menu must reach the registers now, not at the
  // next command: travel detection and the interlock both read these targets.
  PushTargets();
}

void AirDamper::PushTargets()
{
  // The direction switch on each actuator decides which end of its travel the
  // command sends it to. Inverted means it goes the other way — which, for the
  // recycling register of a complementary pair, is the normal case.
  extraction_.SetTargetOpen(is_open_ != extraction_inverted_);
  recycling_.SetTargetOpen(is_open_ != recycling_inverted_);
}

void AirDamper::Open()
{
  if (!is_open_)
  {
    is_open_ = true;
    PushTargets();
    Logger::Info("AirDamper: extraction");
  }
}

void AirDamper::Close()
{
  if (is_open_)
  {
    is_open_ = false;
    PushTargets();
    Logger::Info("AirDamper: recirculation");
  }
}

bool AirDamper::IsMoving() const
{
  if (extraction_.IsMoving())
  {
    return true;
  }
  return count_ >= 2 && recycling_.IsMoving();
}

bool AirDamper::IsFeedbackUsable() const
{
  if (isnan(extraction_.GetPositionPercent()))
  {
    return false;
  }
  return count_ < 2 || !isnan(recycling_.GetPositionPercent());
}

void AirDamper::UpdateInterlock()
{
  // One register cannot shut the air path: whatever it reads, there is no
  // interlock to evaluate.
  bool both_shut = count_ >= 2 && extraction_.IsClosed() && recycling_.IsClosed();

  if (!both_shut)
  {
    if (airflow_blocked_)
    {
      Logger::Info("AirDamper: airflow restored");
    }
    both_closed_     = false;
    airflow_blocked_ = false;
    return;
  }

  uint32_t now = millis();
  if (!both_closed_)
  {
    both_closed_          = true;
    both_closed_since_ms_ = now;
    Logger::Warning("AirDamper: both registers read shut — confirming");
    return;
  }

  if (!airflow_blocked_ && now - both_closed_since_ms_ >= DAMPER_BLOCKED_CONFIRM_MS)
  {
    airflow_blocked_ = true;
    Logger::Error("AirDamper: both registers shut for %us — no airflow",
                  DAMPER_BLOCKED_CONFIRM_MS / 1000U);
  }
}
