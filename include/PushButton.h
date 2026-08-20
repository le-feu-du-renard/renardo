#ifndef PUSH_BUTTON_H
#define PUSH_BUTTON_H

#include <Arduino.h>

// One debounced momentary button, active LOW with the internal pullup.
//
// Fires once per press and stays quiet while the button is held: these are
// session controls, and a held button must not repeat. The press is latched
// until read, so a caller polling slower than Update() cannot miss one.
//
// The logic was written for the single START/STOP button and lived inside
// InputHandler. It moved out when the second button arrived — two buttons
// wanting the same care is exactly the point where copying it becomes the
// wrong answer.
class PushButton
{
public:
  PushButton(uint8_t pin, const char *name);

  void Begin();

  // Call regularly — every INPUT_UPDATE_INTERVAL ms is what the debounce
  // window is sized for.
  void Update(uint32_t now_ms);

  // True once per press, then false until the button is released and pressed
  // again.
  bool IsPressed();

  const char *GetName() const { return name_; }

private:
  uint8_t     pin_;
  const char *name_;

  bool     raw_prev_;
  bool     pending_;   // Unconsumed press event
  bool     consumed_;  // True after the event fired; cleared on release
  uint32_t debounce_ms_;

  static constexpr uint32_t kDebounceMs = 50;
};

#endif // PUSH_BUTTON_H
