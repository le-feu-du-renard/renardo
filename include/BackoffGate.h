#ifndef BACKOFF_GATE_H
#define BACKOFF_GATE_H

#include <stdint.h>

// Rate limiter for talking to a module that may not be there.
//
// ModbusMaster's response timeout is fixed at 2 s, so every transaction aimed at
// an absent slave costs the calling loop two whole seconds. Core 1 polls the
// inlet probe in that same loop, and Core 0 cuts the heating when the probe
// reading ages past SENSOR_TIMEOUT_MS — so a module nobody plugged in can starve
// the one reading regulation actually depends on.
//
// After kFailuresBeforeBackoff consecutive failures the gate lets one attempt
// through per retry interval. An absent module then costs 2 s per interval
// instead of 2 s per cycle, and a module plugged in later still gets picked up.
//
// Two failures rather than one: a single CRC error on a shared bus must not
// throw a healthy module into a multi-second hole.
//
// `now` is passed in rather than read from millis(), which keeps this header
// free of Arduino and lets the native tests drive the clock.
class BackoffGate
{
public:
  static constexpr uint8_t kFailuresBeforeBackoff = 2;

  explicit BackoffGate(uint32_t retry_ms)
      : retry_ms_(retry_ms), failures_(0), last_attempt_ms_(0) {}

  // True when the caller may spend bus time on this module now.
  bool ShouldAttempt(uint32_t now) const
  {
    if (failures_ < kFailuresBeforeBackoff)
    {
      return true;
    }
    return (now - last_attempt_ms_) >= retry_ms_;
  }

  void RecordSuccess() { failures_ = 0; }

  void RecordFailure(uint32_t now)
  {
    if (failures_ < kFailuresBeforeBackoff)
    {
      failures_++;
    }
    last_attempt_ms_ = now;
  }

  // True while attempts are being held back — for logging and diagnostics.
  bool IsBackedOff() const { return failures_ >= kFailuresBeforeBackoff; }

private:
  uint32_t retry_ms_;
  uint8_t  failures_;
  uint32_t last_attempt_ms_;
};

#endif // BACKOFF_GATE_H
