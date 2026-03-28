#include "AirDamper.h"
#include "config.h"
#include "Logger.h"

AirDamper::AirDamper() : leds_(nullptr), is_open_(false) {}

void AirDamper::Begin(IndicatorLEDs &leds)
{
  leds_ = &leds;
  leds_->SetOutput(MCP_AIR_DAMPER_PIN, LOW);  // Start closed
  Logger::Info("AirDamper: initialized (closed)");
}

void AirDamper::Open()
{
  if (!is_open_)
  {
    is_open_ = true;
    leds_->SetOutput(MCP_AIR_DAMPER_PIN, HIGH);
    Logger::Info("AirDamper: opened");
  }
}

void AirDamper::Close()
{
  if (is_open_)
  {
    is_open_ = false;
    leds_->SetOutput(MCP_AIR_DAMPER_PIN, LOW);
    Logger::Info("AirDamper: closed");
  }
}
