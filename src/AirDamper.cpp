#include "AirDamper.h"
#include "Logger.h"

AirDamper::AirDamper()
    : is_open_(false),
      raw_position_(0),
      has_sample_(false),
      raw_closed_(DAMPER_RAW_CLOSED_DEFAULT),
      raw_open_(DAMPER_RAW_OPEN_DEFAULT) {}

void AirDamper::Open()
{
  if (!is_open_)
  {
    is_open_ = true;
    Logger::Info("AirDamper: extraction");
  }
}

void AirDamper::Close()
{
  if (is_open_)
  {
    is_open_ = false;
    Logger::Info("AirDamper: recirculation");
  }
}

void AirDamper::SetRawPosition(uint16_t raw)
{
  raw_position_ = raw;
  has_sample_ = true;
}

void AirDamper::SetCalibration(uint16_t raw_closed, uint16_t raw_open)
{
  raw_closed_ = raw_closed;
  raw_open_ = raw_open;
  Logger::Info("AirDamper: calibration closed=%d open=%d", raw_closed_, raw_open_);
}

float AirDamper::GetPositionPercent() const
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

bool AirDamper::IsMoving() const
{
  float position = GetPositionPercent();
  if (isnan(position))
  {
    return false;
  }

  float target = is_open_ ? 100.0f : 0.0f;
  return fabsf(position - target) > DAMPER_POSITION_TOLERANCE;
}
