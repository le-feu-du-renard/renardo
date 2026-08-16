#include "OutputDriver.h"
#include "Logger.h"

OutputDriver::OutputDriver(uint8_t pin, bool active_low, const char *name)
    : pin_(pin), active_low_(active_low), name_(name), active_(false) {}

void OutputDriver::Begin()
{
  Write(false);
  pinMode(pin_, OUTPUT);
  Write(false);  // re-assert now that the pin actually drives
  active_ = false;
  Logger::Info("OutputDriver %s: GP%d, active %s", name_, pin_,
               active_low_ ? "LOW" : "HIGH");
}

bool OutputDriver::Set(bool active)
{
  if (active == active_)
  {
    return false;
  }
  active_ = active;
  Write(active);
  Logger::Info("Output %s: %s", name_, active ? "ON" : "OFF");
  return true;
}

void OutputDriver::Write(bool active) const
{
  digitalWrite(pin_, (active != active_low_) ? HIGH : LOW);
}
