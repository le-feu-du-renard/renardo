#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>
#include "PushButton.h"
#include "RotaryEncoder.h"

// Reads every physical input: the START and STOP buttons and the encoder.
// Call Update() regularly (every INPUT_UPDATE_INTERVAL ms) from the main loop.
//
// Two dedicated buttons rather than one toggling both ways: a press means the
// same thing whatever the dryer is doing. Setpoints come from the menu, so the
// encoder is the only way to change a value.

class InputHandler
{
public:
  InputHandler();

  void Begin();

  // Must be called periodically to debounce the buttons and the encoder switch.
  void Update();

  // Each returns true once per press. The caller decides what to do with it —
  // both Dryer::Start() and Dryer::Stop() already ignore a request for the
  // state they are already in.
  bool IsStartPressed() { return start_button_.IsPressed(); }
  bool IsStopPressed()  { return stop_button_.IsPressed(); }

  // Encoder — detents since the last call (positive clockwise), and the click.
  int32_t ConsumeEncoderDelta() { return encoder_.ConsumeDelta(); }
  bool    IsEncoderClicked()    { return encoder_.IsClicked(); }

private:
  PushButton    start_button_;
  PushButton    stop_button_;
  RotaryEncoder encoder_;
};

#endif // INPUT_HANDLER_H
