#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>

// Reads the physical START/STOP buttons.
// Call Update() regularly (every INPUT_UPDATE_INTERVAL ms) from the main loop.
//
// The rotary encoder is handled separately by RotaryEncoder; setpoints now come
// from the menu rather than from potentiometers.

class InputHandler
{
public:
  InputHandler();

  void Begin();

  // Must be called periodically to debounce the buttons.
  void Update();

  // Buttons — return true once per press, edge-triggered.
  bool IsStartPressed();
  bool IsStopPressed();

private:
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
