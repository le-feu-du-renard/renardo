#ifndef DAMPER_FEEDBACK_H
#define DAMPER_FEEDBACK_H

#include <Arduino.h>
#include "config.h"

// One register's position readback — raw ADC, its own two-point calibration,
// and the opening derived from them.
//
// Split out of AirDamper because this version has two registers, extraction and
// recycling, and they are asymmetric: a single calibration pair cannot describe
// both. The command is not here — one signal drives both registers, so it stays
// in AirDamper, which pushes each register's *target* down here whenever it
// changes. That is what keeps the "extraction opens while recycling closes"
// rule in exactly one place instead of being re-derived at every call site.
//
// Hardware-free by design: main.cpp samples the ADC and pushes the raw value in
// through SetRawPosition(), the same way it writes the command pin.
class DamperFeedback
{
public:
  // `name` only ever appears in log lines, so a literal is expected.
  explicit DamperFeedback(const char *name);

  // Latest raw ADC sample from the actuator's feedback output.
  void     SetRawPosition(uint16_t raw);
  uint16_t GetRawPosition() const { return raw_position_; }

  // Two-point calibration, set from the menu by driving this register to each
  // end stop and capturing the raw value there.
  void     SetCalibration(uint16_t raw_closed, uint16_t raw_open);
  uint16_t GetRawClosed() const { return raw_closed_; }
  uint16_t GetRawOpen() const { return raw_open_; }

  // Where this register belongs under the current command, pushed in by
  // AirDamper. Only travel detection uses it; the measured opening does not.
  void SetTargetOpen(bool open) { target_open_ = open; }
  bool GetTargetOpen() const { return target_open_; }

  // Measured opening, 0 % = closed, 100 % = fully open.
  // Returns NAN while no sample has been taken or if the calibration is unusable.
  float GetPositionPercent() const;

  // True while the measured opening has not yet reached the commanded end.
  // Used to show travel on the main screen and nothing else.
  bool IsMoving() const;

  const char *GetName() const { return name_; }

private:
  const char *name_;
  uint16_t    raw_position_;
  bool        has_sample_;
  uint16_t    raw_closed_;
  uint16_t    raw_open_;
  bool        target_open_;
};

#endif // DAMPER_FEEDBACK_H
