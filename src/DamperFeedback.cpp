#include "DamperFeedback.h"
#include "Logger.h"

DamperFeedback::DamperFeedback(const char *name)
    : name_(name),
      raw_position_(0),
      has_sample_(false),
      raw_closed_(DAMPER_RAW_CLOSED_DEFAULT),
      raw_open_(DAMPER_RAW_OPEN_DEFAULT),
      target_open_(false) {}

void DamperFeedback::SetRawPosition(uint16_t raw)
{
  raw_position_ = raw;
  has_sample_ = true;
}

void DamperFeedback::SetCalibration(uint16_t raw_closed, uint16_t raw_open)
{
  raw_closed_ = raw_closed;
  raw_open_ = raw_open;
  Logger::Info("Damper %s: calibration closed=%d open=%d", name_, raw_closed_,
               raw_open_);
}

float DamperFeedback::GetPositionPercent() const
{
  if (!has_sample_)
  {
    return NAN;
  }

  // A degenerate calibration means the feedback wire is unusable; report no
  // position rather than a meaningless number.
  int32_t span = static_cast<int32_t>(raw_open_) - static_cast<int32_t>(raw_closed_);
  if (span > -DAMPER_CALIBRATION_MIN_SPAN && span < DAMPER_CALIBRATION_MIN_SPAN)
  {
    return NAN;
  }

  float ratio = static_cast<float>(static_cast<int32_t>(raw_position_) -
                                   static_cast<int32_t>(raw_closed_)) /
                static_cast<float>(span);
  return constrain(ratio * 100.0f, 0.0f, 100.0f);
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
