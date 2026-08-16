#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>
#include "QuadratureDecoder.h"

// EC11 rotary encoder with push switch, all three lines active LOW with
// internal pull-ups. The two quadrature lines are sampled from a pin-change
// interrupt so no rotation is missed while the display is being redrawn.
//
// Only one instance is supported: the Arduino interrupt API takes a bare
// function pointer with no context.
class RotaryEncoder
{
public:
  RotaryEncoder(uint8_t pin_a, uint8_t pin_b, uint8_t pin_sw);

  void Begin();

  // Debounces the push switch; call regularly from the main loop.
  void Update();

  // Detents accumulated since the last call: positive clockwise.
  int32_t ConsumeDelta();

  // True once per press.
  bool IsClicked();

private:
  uint8_t pin_a_;
  uint8_t pin_b_;
  uint8_t pin_sw_;

  QuadratureDecoder decoder_;
  volatile int32_t  delta_;

  bool     sw_raw_prev_;
  bool     sw_pending_;
  bool     sw_consumed_;
  uint32_t sw_debounce_ms_;

  static constexpr uint32_t kDebounceMs = 30;

  void HandleInterrupt();

  static RotaryEncoder *instance_;
  static void InterruptTrampoline();
};

#endif // ROTARY_ENCODER_H
