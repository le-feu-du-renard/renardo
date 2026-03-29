#include "AirDamper.h"
#include "Logger.h"

AirDamper::AirDamper() : is_open_(false) {}

void AirDamper::Open()
{
  if (!is_open_)
  {
    is_open_ = true;
    Logger::Info("AirDamper: opened");
  }
}

void AirDamper::Close()
{
  if (is_open_)
  {
    is_open_ = false;
    Logger::Info("AirDamper: closed");
  }
}
