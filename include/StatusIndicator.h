#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H

#include <Arduino.h>
#include "config.h"

// What the panel LEDs say, and when.
//
// Hardware-free on purpose, the same split as QuadratureDecoder against
// RotaryEncoder: what the four states mean and how they blink is the part worth
// testing on the host, and StatusLed is left with nothing but two pins.

enum class DryerStatus : uint8_t
{
  kStopped = 0,
  kRunning = 1,
  kCooling = 2, // session stopped, fan still cooling the electric heater
  kFault   = 3,
};

// Which LED is lit this instant. Two independent lines rather than a colour:
// the panel carries two discrete LEDs, not one RGB part.
struct LedPattern
{
  bool green;
  bool red;
};

// Fold the dryer's condition into one state.
//
// The fault wins over everything, including a session that is still running —
// a stale probe blocks the heat sources but not the fan, so the dryer can be
// turning while something is wrong, and that is precisely when the panel must
// say so rather than show a reassuring steady green.
//
// `uptime_ms` gates the fault, not the whole state: for the first
// STATUS_FAULT_GRACE_MS the probe and the hydraulic module have simply not
// answered yet, which is not a fault, it is a boot.
DryerStatus ResolveStatus(bool running, bool fan_active, bool fault, uint32_t uptime_ms);

// The two LED levels for a state at a given instant.
//
// The blink is a pure function of the clock — no phase is carried between
// calls, so a state change mid-beat is not a special case and neither is a
// caller that skips a few milliseconds.
LedPattern PatternFor(DryerStatus status, uint32_t now_ms);

// For the log line: "running", "cooling", ...
const char *StatusName(DryerStatus status);

#endif // STATUS_INDICATOR_H
