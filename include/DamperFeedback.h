#ifndef DAMPER_FEEDBACK_H
#define DAMPER_FEEDBACK_H

#include <Arduino.h>
#include "config.h"

// One register's position readback — raw ADC, its own two-point calibration,
// and the opening derived from them.
//
// Split out of AirDamper because a dryer can carry two registers, extraction and
// recycling, and they are asymmetric: a single calibration pair cannot describe
// both. The command is not here — one signal drives every register, so it stays
// in AirDamper, which pushes each register's *target* down here whenever it
// changes. That is what keeps the direction rules in exactly one place instead
// of being re-derived at every call site.
//
// The calibration is two **ordered marks**, min and max, plus a flag saying
// which end is the open one. The flag is not this register's business — it is a
// property of the actuator model, shared by all of them — but it is stored here
// because this is where the arithmetic that uses it lives. Naming the two marks
// "closed" and "open" instead put the direction inside the order of the pair,
// where entering them the wrong way round reported every opening inside out
// while looking entirely plausible.
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
  // end of its travel and capturing the raw value there: `raw_min` is the
  // smaller of the two readings, `raw_max` the larger. `low_is_open` says which
  // one is the open end, and is the same for every register on the dryer.
  void     SetCalibration(uint16_t raw_min, uint16_t raw_max, bool low_is_open);
  uint16_t GetRawMin() const { return raw_min_; }
  uint16_t GetRawMax() const { return raw_max_; }
  bool     GetLowIsOpen() const { return low_is_open_; }

  // Where this register belongs under the current command, pushed in by
  // AirDamper. Only travel detection uses it; the measured opening does not.
  void SetTargetOpen(bool open) { target_open_ = open; }
  bool GetTargetOpen() const { return target_open_; }

  // True while this channel is carrying a signal at all.
  //
  // The divider's lower resistor sits between the tap and ground, so a feedback
  // wire that is absent, cut or dead reads near zero rather than floating, well
  // below the actuator's own 2V floor. False until the first sample arrives, and
  // it takes several consecutive samples below the threshold to go false again:
  // one noisy conversion must not declare a working feedback dead.
  bool HasSignal() const;

  // Measured opening, 0 % = closed, 100 % = fully open.
  // Returns NAN while there is no signal, before the first sample, or when the
  // calibration is unusable — a span too small to be real, which includes a pair
  // entered in descending order.
  float GetPositionPercent() const;

  // True when this register is measurably shut. NAN counts as "not known to be
  // shut": the airflow interlock must trip on a positive reading, never on the
  // absence of one.
  bool IsClosed() const;

  // True while the measured opening has not yet reached the commanded end.
  // Used to show travel on the main screen and nothing else.
  bool IsMoving() const;

  const char *GetName() const { return name_; }

private:
  const char *name_;
  uint16_t    raw_position_;
  bool        has_sample_;
  uint8_t     quiet_samples_;  // consecutive samples below the signal floor
  uint16_t    raw_min_;
  uint16_t    raw_max_;
  bool        low_is_open_;
  bool        target_open_;
};

#endif // DAMPER_FEEDBACK_H
