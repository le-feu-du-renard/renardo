#ifndef OUTPUT_DRIVER_H
#define OUTPUT_DRIVER_H

#include <Arduino.h>

// One BC337 command output, NPN in common emitter, switching the low side.
//
// Polarity follows how the load is wired, so it is a per-output property:
//   active HIGH — the collector sits in series with a contactor coil to +24V,
//     so a floating GPIO leaves the load off. Required for the fan and the
//     electric heater, which must stay off during the boot window. The damper
//     reaches the same polarity the other way round: its stage inverts into a
//     relay input that is itself active LOW.
//   active LOW  — the collector pulls down an input the receiver already pulls
//     up and that energises on a high level. Nothing on this board is wired that
//     way today; the case is kept because the polarity is per output, not a
//     board-wide constant.
//
// Writes are edge-detected: the pin is only touched when the state changes.
// v3 spread this logic across main.cpp and inverted the damper in two separate
// places, which made the real polarity very easy to lose track of.
class OutputDriver
{
public:
  OutputDriver(uint8_t pin, bool active_low, const char *name);

  // Drives the pin to its inactive level *before* switching to OUTPUT, so the
  // load is never briefly energised at boot.
  void Begin();

  // Returns true when the state actually changed.
  bool Set(bool active);

  bool IsActive() const { return active_; }
  const char *GetName() const { return name_; }

private:
  uint8_t     pin_;
  bool        active_low_;
  const char *name_;
  bool        active_;

  void Write(bool active) const;
};

#endif // OUTPUT_DRIVER_H
