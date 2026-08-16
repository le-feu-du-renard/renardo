#ifndef OUTPUT_DRIVER_H
#define OUTPUT_DRIVER_H

#include <Arduino.h>

// One 2N2222 open-collector command output.
//
// Polarity follows how the load is wired, so it is a per-output property:
//   active HIGH — the collector sits in series with a contactor coil to +24V,
//     so a floating GPIO leaves the load off. Required for the fan and the
//     electric heater, which must stay off during the boot window.
//   active LOW  — the collector pulls down an input the receiver already pulls
//     up, as on the Belimo damper command line.
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
