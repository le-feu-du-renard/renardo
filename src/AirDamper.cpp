#include "AirDamper.h"
#include "Logger.h"

AirDamper::AirDamper()
    : is_open_(false),
      count_(DAMPER_COUNT_DEFAULT),
      command_inverted_(DAMPER_COMMAND_INVERTED_DEFAULT),
      feedback_low_is_open_(DAMPER_FEEDBACK_LOW_IS_OPEN_DEFAULT),
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
  // Announced, because it is the one damper setting that changes where the air
  // goes, and the log is where someone works out afterwards why a machine that
  // said recirculation was extracting.
  if (config.command_inverted != command_inverted_)
  {
    Logger::Info("AirDamper: command polarity -> %s",
                 config.command_inverted ? "inverted" : "normal");
  }
  command_inverted_ = config.command_inverted;

  feedback_low_is_open_ = config.feedback_low_is_open;
  extraction_inverted_  = config.extraction_inverted;
  recycling_inverted_   = config.recycling_inverted;

  // The direction switch mirrors the feedback as well as the travel, so a
  // register set the other way round reports the other end of its signal as the
  // open one. Without this the two channels of a complementary pair — which put
  // out the same voltage, since each reports position in its own mirrored frame
  // — resolve to the same opening, and the dryer believes both registers are
  // shut for half of every session. `feedback_low_is_open` describes the
  // actuator on a register whose switch is Normal; each register's own sense is
  // that, turned round again if its switch is not.
  extraction_.SetCalibration(config.extraction_raw_min, config.extraction_raw_max,
                             config.feedback_low_is_open != config.extraction_inverted);
  recycling_.SetCalibration(config.recycling_raw_min, config.recycling_raw_max,
                            config.feedback_low_is_open != config.recycling_inverted);

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
      // "Not shut" and "cannot tell" arrive here as the same answer, because
      // IsClosed() reads a dead feedback as not-shut. Only the first is a
      // recovery. Announcing one for the second would put the single most
      // misleading line this firmware can print into the log at the exact
      // moment the interlock goes blind — and it is the line an operator would
      // reach for afterwards to work out what the machine thought it was doing.
      //
      // Clearing the flag either way is right: it may only be asserted on a
      // positive reading, and there is none. The blind case is not left
      // unguarded — it becomes kDamperFeedback, which stops the session on the
      // same pass and outranks nothing it should not.
      Logger::Info(IsFeedbackUsable()
                       ? "AirDamper: airflow restored"
                       : "AirDamper: feedback lost — airflow can no longer be judged");
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
