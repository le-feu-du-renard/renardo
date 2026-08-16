#include "RotaryEncoder.h"
#include "Logger.h"

RotaryEncoder *RotaryEncoder::instance_ = nullptr;

RotaryEncoder::RotaryEncoder(uint8_t pin_a, uint8_t pin_b, uint8_t pin_sw)
    : pin_a_(pin_a),
      pin_b_(pin_b),
      pin_sw_(pin_sw),
      delta_(0),
      sw_raw_prev_(false),
      sw_pending_(false),
      sw_consumed_(false),
      sw_debounce_ms_(0) {}

void RotaryEncoder::Begin()
{
  pinMode(pin_a_, INPUT_PULLUP);
  pinMode(pin_b_, INPUT_PULLUP);
  pinMode(pin_sw_, INPUT_PULLUP);

  // Seed the decoder with the resting position so the first movement is not
  // read as a transition from an imaginary state.
  decoder_.Reset();
  decoder_.Step(digitalRead(pin_a_) == HIGH, digitalRead(pin_b_) == HIGH);

  // A button held at boot is marked consumed so it does not fire immediately.
  sw_raw_prev_ = (digitalRead(pin_sw_) == LOW);
  sw_consumed_ = sw_raw_prev_;

  instance_ = this;
  attachInterrupt(digitalPinToInterrupt(pin_a_), InterruptTrampoline, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pin_b_), InterruptTrampoline, CHANGE);

  Logger::Info("RotaryEncoder: A=GP%d B=GP%d SW=GP%d", pin_a_, pin_b_, pin_sw_);
}

void RotaryEncoder::HandleInterrupt()
{
  int8_t movement = decoder_.Step(digitalRead(pin_a_) == HIGH,
                                  digitalRead(pin_b_) == HIGH);
  if (movement != 0)
  {
    delta_ += movement;
  }
}

void RotaryEncoder::InterruptTrampoline()
{
  if (instance_ != nullptr)
  {
    instance_->HandleInterrupt();
  }
}

int32_t RotaryEncoder::ConsumeDelta()
{
  // Interrupts touch delta_, so take and clear it atomically.
  noInterrupts();
  int32_t value = delta_;
  delta_ = 0;
  interrupts();
  return value;
}

void RotaryEncoder::Update()
{
  uint32_t now = millis();
  bool raw = (digitalRead(pin_sw_) == LOW);

  if (!raw)
  {
    sw_consumed_ = false; // released: allow the next press
  }
  else if (!sw_raw_prev_)
  {
    sw_debounce_ms_ = now; // pressed: start the debounce window
  }
  else if (!sw_consumed_ && (now - sw_debounce_ms_) >= kDebounceMs)
  {
    sw_pending_  = true;
    sw_consumed_ = true;
  }
  sw_raw_prev_ = raw;
}

bool RotaryEncoder::IsClicked()
{
  if (sw_pending_)
  {
    sw_pending_ = false;
    return true;
  }
  return false;
}
