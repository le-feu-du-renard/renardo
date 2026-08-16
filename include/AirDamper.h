#ifndef AIR_DAMPER_H
#define AIR_DAMPER_H

#include <Arduino.h>
#include "config.h"

// Air damper (Belimo LM24A-SR) — commanded on/off, with position readback.
//
// The command is strictly binary: recirculation (closed) or extraction (open).
// The actuator's 2-10V feedback output is read on an ADC and used only to show
// the real position while the vane travels, which takes about 150 s each way.
// Nothing in the control logic depends on it.
//
// The class stays hardware-free: main.cpp samples the ADC and pushes the raw
// value in through SetRawPosition(), the same way it writes the command pin.
class AirDamper
{
public:
  AirDamper();

  void Open();   // extraction
  void Close();  // recirculation
  bool IsOpen() const { return is_open_; }

  // Latest raw ADC sample from the actuator's feedback output.
  void     SetRawPosition(uint16_t raw);
  uint16_t GetRawPosition() const { return raw_position_; }

  // Two-point calibration, set from the menu by driving the damper to each
  // end stop and capturing the raw value there.
  void SetCalibration(uint16_t raw_closed, uint16_t raw_open);
  uint16_t GetRawClosed() const { return raw_closed_; }
  uint16_t GetRawOpen() const { return raw_open_; }

  // Measured position, 0 % = recirculation, 100 % = extraction.
  // Returns NAN while no sample has been taken or if the calibration is unusable.
  float GetPositionPercent() const;

  // True while the measured position has not yet reached the commanded end.
  // Used to show a travel bar on the main screen and nothing else.
  bool IsMoving() const;

private:
  bool     is_open_;
  uint16_t raw_position_;
  bool     has_sample_;
  uint16_t raw_closed_;
  uint16_t raw_open_;
};

#endif // AIR_DAMPER_H
