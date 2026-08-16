#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>
#include "RotaryEncoder.h"

// Reads every physical input: the single START/STOP button and the encoder.
// Call Update() regularly (every INPUT_UPDATE_INTERVAL ms) from the main loop.
//
// One button now serves both roles: pressing it starts a stopped dryer and
// stops a running one. Setpoints come from the menu, so the encoder is the only
// way to change a value.

class InputHandler
{
public:
  InputHandler();

  void Begin();

  // Must be called periodically to debounce the button and the encoder switch.
  void Update();

  // Returns true once per press. The caller decides whether that means start or
  // stop, from the current session state.
  bool IsButtonPressed();

  // Encoder — detents since the last call (positive clockwise), and the click.
  int32_t ConsumeEncoderDelta() { return encoder_.ConsumeDelta(); }
  bool    IsEncoderClicked()    { return encoder_.IsClicked(); }

private:
  RotaryEncoder encoder_;

  bool     button_raw_prev_;
  bool     button_pending_;   // Unconsumed press event
  bool     button_consumed_;  // True after the event fired; cleared on release
  uint32_t button_debounce_ms_;

  static constexpr uint32_t kDebounceMs = 50;
};

#endif // INPUT_HANDLER_H
