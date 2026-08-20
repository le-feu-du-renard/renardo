#include "StatusIndicator.h"

DryerStatus ResolveStatus(bool running, bool fan_active, bool fault, uint32_t uptime_ms)
{
  if (fault && uptime_ms >= STATUS_FAULT_GRACE_MS)
  {
    return DryerStatus::kFault;
  }
  if (running)
  {
    return DryerStatus::kRunning;
  }
  // The fan outlives the session by FAN_COOLDOWN_DURATION_S. Saying "stopped"
  // while it is still turning would have the operator open the machine on a
  // running fan and a heater that has not given up its heat yet.
  if (fan_active)
  {
    return DryerStatus::kCooling;
  }
  return DryerStatus::kStopped;
}

LedPattern PatternFor(DryerStatus status, uint32_t now_ms)
{
  bool beat = ((now_ms / STATUS_BLINK_INTERVAL) % 2) == 0;

  switch (status)
  {
    case DryerStatus::kRunning: return {true, false};
    case DryerStatus::kCooling: return {beat, false};
    case DryerStatus::kStopped: return {false, true};
    case DryerStatus::kFault:   return {false, beat};
  }
  return {false, false};
}

const char *StatusName(DryerStatus status)
{
  switch (status)
  {
    case DryerStatus::kRunning: return "running";
    case DryerStatus::kCooling: return "cooling";
    case DryerStatus::kStopped: return "stopped";
    case DryerStatus::kFault:   return "fault";
  }
  return "unknown";
}
