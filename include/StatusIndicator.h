#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H

#include <Arduino.h>
#include "config.h"

// What the panel LEDs say, and when.
//
// Hardware-free on purpose, the same split as QuadratureDecoder against
// RotaryEncoder: what the states mean and how they blink is the part worth
// testing on the host, and StatusLed is left with nothing but two pins.

// The session axis: what the dryer is doing.
enum class DryerStatus : uint8_t
{
  kStopped = 0,
  kRunning = 1,
  kCooling = 2, // session stopped, fan still cooling the electric heater
};

// The fault axis: what is wrong, worst first.
//
// Every value here refuses a start — that is the whole point of the list. The
// panel and the interlock read from the same enum precisely so the operator can
// never be refused a start with nothing on the machine saying why: if the red
// LED blinks, the button will not take, and the screen names the reason.
enum class DryerFault : uint8_t
{
  kNone = 0,
  kAirflowBlocked,   // both registers shut: the only one that also stops a run
  kDamperFeedback,   // a declared register reports no usable opening
  kSensorStale,      // inlet probe silent, heating blocked
  kHydraulicOffline, // module enabled at the menu but not answering
};

// The panel's two axes, resolved. Held together because the log line and the
// LED pattern both need the pair, and because reading either alone is how the
// old single-state panel came to hide a running session behind a fault.
struct PanelState
{
  DryerStatus status;
  DryerFault  fault;
};

// Which LED is lit this instant. Two independent lines rather than a colour:
// the panel carries two discrete LEDs, not one RGB part.
struct LedPattern
{
  bool green;
  bool red;
};

// Returned for a fault that never brings a running session down.
constexpr uint32_t kFaultNeverStops = UINT32_MAX;

// How long a running session may continue with this fault before it is stopped,
// counted from the moment the fault appears.
//
// Three of the four end the batch. The dividing line is not severity but
// whether the dryer can still be trusted to be doing what it says:
//
//   - kAirflowBlocked: a fan pushing against two shut vanes moves no air, and
//     the electric heater is sitting in that duct.
//   - kDamperFeedback: the recopy is the *only* evidence the air path is open.
//     Losing it does not leave one indicator short — DamperFeedback::IsClosed()
//     reads NAN as "not known to be shut", so the airflow interlock above is
//     silently disarmed by the very failure it exists to catch. A dryer that
//     cannot tell whether air is moving must not keep heating on the assumption
//     that it is.
//   - kSensorStale: the inlet probe is the control input. Blocking the heat
//     leaves the fan turning and the session advancing its phases on a frozen
//     humidity — a batch that runs to a finish it never actually reached.
//
// kHydraulicOffline is the exception, and deliberately so: the module is
// optional at runtime, and losing it degrades to electric-only. Nothing about
// the dryer has become unknowable.
//
// Only the probe gets a hold-off, and only because it is the one fault whose
// detection threshold is a *timeout* rather than a measurement. The other two
// are already confirmed upstream before they are ever reported — 30 s of both
// registers reading shut, three consecutive samples below the signal floor — so
// by the time they arrive there is nothing left to wait for. Waiting again here
// would just be the same debounce twice, on a dryer that is provably unfit.
uint32_t FaultStopHoldoffMs(DryerFault fault);

// Drop the faults that a freshly booted dryer cannot yet be sure of.
//
// For the first STATUS_FAULT_GRACE_MS the probe and the hydraulic module have
// simply not answered yet, which is not a fault, it is a boot. The two air-path
// faults are not graced: they come off the ADC, which reads from the first loop.
//
// The interlock and the panel both go through here, so a start is never refused
// for a fault the LED is still holding back.
DryerFault ApplyFaultGrace(DryerFault fault, uint32_t uptime_ms);

// Fold the dryer's condition into the two axes.
//
// The fault no longer wins over the session — it sits beside it. A stale probe
// blocks the heat sources but not the fan, so the dryer can be turning while
// something is wrong, and the panel has to say both things at once: green for
// what the machine is doing, red for what is wrong with it.
PanelState ResolvePanel(bool running, bool fan_active, DryerFault fault);

// The two LED levels for a panel state at a given instant.
//
// Green carries the session, red carries the fault, and the one overlap is
// resolved in the blink: red steady means idle, red blinking means fault. So a
// running dryer in fault shows steady green *and* blinking red, which no other
// condition does.
//
// The blink is a pure function of the clock — no phase is carried between
// calls, so a state change mid-beat is not a special case and neither is a
// caller that skips a few milliseconds.
LedPattern PatternFor(const PanelState &state, uint32_t now_ms);

// For the log line: "running", "cooling", ...
const char *StatusName(DryerStatus status);

// For the log line and the start refusal: "hydraulic module offline", ...
// The screen has its own French copy — this half is diagnostics.
const char *FaultName(DryerFault fault);

#endif // STATUS_INDICATOR_H
