#ifndef AIR_DAMPER_H
#define AIR_DAMPER_H

#include <Arduino.h>
#include "config.h"
#include "DamperFeedback.h"

// The air path — two Belimo LM24A-SR registers driven by one command.
//
// The command is strictly binary: recirculation or extraction. The two registers
// are **complementary**, since air is either extracted or recycled and never
// both, so a single relay drives both actuators with one of them wired to travel
// the other way. That complementarity is applied here, in Open() and Close(),
// and nowhere else — v3's mistake was inverting the damper in two separate
// places, which made the real direction impossible to follow.
//
// The registers are **asymmetric** in geometry, so each carries its own
// calibration and reports its own opening: see DamperFeedback. The control logic
// depends on none of it — the readback exists to be displayed.
class AirDamper
{
public:
  AirDamper();

  void Open();   // extraction: extraction register opens, recycling closes
  void Close();  // recirculation: the other way round
  bool IsOpen() const { return is_open_; }

  // The two registers, each with its own calibration, sample and opening.
  DamperFeedback       &Extraction()       { return extraction_; }
  const DamperFeedback &Extraction() const { return extraction_; }
  DamperFeedback       &Recycling()        { return recycling_; }
  const DamperFeedback &Recycling() const  { return recycling_; }

  // True while either register is still travelling. Both are driven by the same
  // relay, so they move together and stop within a few seconds of each other.
  bool IsMoving() const;

private:
  bool           is_open_;
  DamperFeedback extraction_;
  DamperFeedback recycling_;
};

#endif // AIR_DAMPER_H
