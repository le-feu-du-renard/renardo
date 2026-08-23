#include "StatusIndicator.h"

uint32_t FaultStopHoldoffMs(DryerFault fault)
{
  // The fault appears at SENSOR_TIMEOUT_MS of silence and the batch ends at
  // SENSOR_SESSION_TIMEOUT_MS, so what is left to wait is the difference.
  static_assert(SENSOR_SESSION_TIMEOUT_MS > SENSOR_TIMEOUT_MS,
                "the session must outlive the heating interlock, not precede it");

  switch (fault)
  {
    case DryerFault::kAirflowBlocked:
    case DryerFault::kDamperFeedback:
      return 0; // already confirmed upstream; nothing left to wait for
    case DryerFault::kSensorStale:
      return SENSOR_SESSION_TIMEOUT_MS - SENSOR_TIMEOUT_MS;
    case DryerFault::kHydraulicOffline: // degrades to electric-only
    case DryerFault::kNone:
      return kFaultNeverStops;
  }
  return kFaultNeverStops;
}

DryerFault ApplyFaultGrace(DryerFault fault, uint32_t uptime_ms)
{
  if (uptime_ms >= STATUS_FAULT_GRACE_MS)
  {
    return fault;
  }

  switch (fault)
  {
    // Nobody has been asked yet, so nobody is late.
    case DryerFault::kSensorStale:
    case DryerFault::kHydraulicOffline:
      return DryerFault::kNone;
    default:
      return fault;
  }
}

PanelState ResolvePanel(bool running, bool fan_active, DryerFault fault)
{
  PanelState state{DryerStatus::kStopped, fault};

  if (running)
  {
    state.status = DryerStatus::kRunning;
  }
  // The fan outlives the session by FAN_COOLDOWN_DURATION_S. Saying "stopped"
  // while it is still turning would have the operator open the machine on a
  // running fan and a heater that has not given up its heat yet.
  else if (fan_active)
  {
    state.status = DryerStatus::kCooling;
  }

  return state;
}

LedPattern PatternFor(const PanelState &state, uint32_t now_ms)
{
  bool beat = ((now_ms / STATUS_BLINK_INTERVAL) % 2) == 0;

  LedPattern pattern{false, false};

  switch (state.status)
  {
    case DryerStatus::kRunning: pattern.green = true; break;
    case DryerStatus::kCooling: pattern.green = beat; break;
    case DryerStatus::kStopped: break;
  }

  // Red says two things and the blink separates them: a dryer standing idle
  // holds it steady, a fault flashes it. A fault therefore reads the same
  // whatever the session is doing, which is what makes it readable across the
  // room — the operator looks at the red for trouble and the green for motion,
  // and never has to work out which of the two the panel decided to show.
  pattern.red = (state.fault != DryerFault::kNone)
                    ? beat
                    : (state.status == DryerStatus::kStopped);

  return pattern;
}

const char *StatusName(DryerStatus status)
{
  switch (status)
  {
    case DryerStatus::kRunning: return "running";
    case DryerStatus::kCooling: return "cooling";
    case DryerStatus::kStopped: return "stopped";
  }
  return "unknown";
}

const char *FaultName(DryerFault fault)
{
  switch (fault)
  {
    case DryerFault::kNone:             return "none";
    case DryerFault::kAirflowBlocked:   return "both registers shut, no airflow";
    case DryerFault::kDamperFeedback:   return "register feedback unusable";
    case DryerFault::kSensorStale:      return "inlet probe silent";
    case DryerFault::kHydraulicOffline: return "hydraulic module not answering";
  }
  return "unknown";
}
