#include "DamperFeedback.h"
#include "Logger.h"

DamperFeedback::DamperFeedback(const char *name)
    : name_(name),
      raw_position_(0),
      has_sample_(false),
      quiet_samples_(DAMPER_SIGNAL_CONFIRM_SAMPLES),
      raw_min_(DAMPER_RAW_MIN_DEFAULT),
      raw_max_(DAMPER_RAW_MAX_DEFAULT),
      low_is_open_(DAMPER_FEEDBACK_LOW_IS_OPEN_DEFAULT),
      target_open_(false) {}

void DamperFeedback::SetRawPosition(uint16_t raw)
{
  raw_position_ = raw;
  has_sample_ = true;

  if (raw < DAMPER_SIGNAL_MIN_RAW)
  {
    // Saturating, so a channel that has been dead for hours does not wrap round
    // and announce a signal.
    if (quiet_samples_ < DAMPER_SIGNAL_CONFIRM_SAMPLES)
    {
      quiet_samples_++;
    }
  }
  else
  {
    quiet_samples_ = 0;
  }
}

void DamperFeedback::SetCalibration(uint16_t raw_min, uint16_t raw_max, bool low_is_open)
{
  raw_min_     = raw_min;
  raw_max_     = raw_max;
  low_is_open_ = low_is_open;
  Logger::Info("Damper %s: calibration min=%d max=%d (%s = open)", name_,
               raw_min_, raw_max_, low_is_open_ ? "min" : "max");
}

bool DamperFeedback::HasSignal() const
{
  return has_sample_ && quiet_samples_ < DAMPER_SIGNAL_CONFIRM_SAMPLES;
}

float DamperFeedback::GetPositionPercent() const
{
  if (!HasSignal())
  {
    return NAN;
  }

  // A span too small to be real means the feedback wire is unusable, and a
  // negative one means the pair was entered in descending order — which the two
  // marks are meant to make impossible, but a menu can still be turned the wrong
  // way. Report no position rather than a meaningless number in either case.
  int32_t span = static_cast<int32_t>(raw_max_) - static_cast<int32_t>(raw_min_);
  if (span < DAMPER_CALIBRATION_MIN_SPAN)
  {
    return NAN;
  }

  float ratio = static_cast<float>(static_cast<int32_t>(raw_position_) -
                                   static_cast<int32_t>(raw_min_)) /
                static_cast<float>(span);
  float percent = low_is_open_ ? 100.0f - ratio * 100.0f : ratio * 100.0f;
  return constrain(percent, 0.0f, 100.0f);
}

bool DamperFeedback::IsClosed() const
{
  float position = GetPositionPercent();
  if (isnan(position))
  {
    return false;
  }
  return position <= DAMPER_CLOSED_THRESHOLD;
}

bool DamperFeedback::IsMoving() const
{
  float position = GetPositionPercent();
  if (isnan(position))
  {
    return false;
  }

  float target = target_open_ ? 100.0f : 0.0f;
  return fabsf(position - target) > DAMPER_POSITION_TOLERANCE;
}
