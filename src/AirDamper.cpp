#include "AirDamper.h"
#include "Logger.h"

AirDamper::AirDamper()
    : is_open_(false),
      extraction_("extraction"),
      recycling_("recycling")
{
  // Start consistent with is_open_ = false, so travel detection is right from
  // the first sample rather than after the first command.
  extraction_.SetTargetOpen(false);
  recycling_.SetTargetOpen(true);
}

void AirDamper::Open()
{
  if (!is_open_)
  {
    is_open_ = true;
    extraction_.SetTargetOpen(true);
    recycling_.SetTargetOpen(false);
    Logger::Info("AirDamper: extraction");
  }
}

void AirDamper::Close()
{
  if (is_open_)
  {
    is_open_ = false;
    extraction_.SetTargetOpen(false);
    recycling_.SetTargetOpen(true);
    Logger::Info("AirDamper: recirculation");
  }
}

bool AirDamper::IsMoving() const
{
  return extraction_.IsMoving() || recycling_.IsMoving();
}
