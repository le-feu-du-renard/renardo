#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>
#include "RotaryEncoder.h"

// Reads every physical input: the START/STOP buttons and the rotary encoder.
// Call Update() regularly (every INPUT_UPDATE_INTERVAL ms) from the main loop.
//
// Setpoints now come from the menu rather than from potentiometers, so the
// encoder is the only way to change a value.

class InputHandler
{
public:
  InputHandler();

  void Begin();

  // Must be called periodically to debounce the buttons and the encoder switch.
  void Update();

  // Buttons — return true once per press, edge-triggered.
  bool IsStartPressed();
  bool IsStopPressed();

  // Encoder — detents since the last call (positive clockwise), and the click.
  int32_t ConsumeEncoderDelta() { return encoder_.ConsumeDelta(); }
  bool    IsEncoderClicked()    { return encoder_.IsClicked(); }

private:
  RotaryEncoder encoder_;

  bool     start_raw_prev_;
  bool     stop_raw_prev_;
  bool     start_pending_;    // Unconsumed press event
  bool     stop_pending_;
  bool     start_consumed_;   // True after event fired; cleared on button release
  bool     stop_consumed_;
  uint32_t start_debounce_ms_;
  uint32_t stop_debounce_ms_;

  static constexpr uint32_t kDebounceMs = 50;
};

#endif // INPUT_HANDLER_H
