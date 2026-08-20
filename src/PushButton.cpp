#include "PushButton.h"
#include "Logger.h"

PushButton::PushButton(uint8_t pin, const char *name)
    : pin_(pin),
      name_(name),
      raw_prev_(false),
      pending_(false),
      consumed_(false),
      debounce_ms_(0) {}

void PushButton::Begin()
{
  pinMode(pin_, INPUT_PULLUP);

  // Seed prev state from the actual pin level so the first Update() doesn't
  // misinterpret a held-low pin (power-up noise) as a rising edge.
  raw_prev_ = (digitalRead(pin_) == LOW);
  // A button held at init is marked consumed so the steady-held check doesn't
  // fire immediately — the debounce timer is still 0 here. A button stuck low
  // would otherwise act on a session restored from flash the instant the board
  // came back up.
  consumed_ = raw_prev_;

  Logger::Info("PushButton %s: GP%d, active LOW", name_, pin_);
}

void PushButton::Update(uint32_t now_ms)
{
  // Active LOW, fires once per press.
  bool raw = (digitalRead(pin_) == LOW);
  if (!raw)
  {
    consumed_ = false; // released: allow the next press
  }
  else if (!raw_prev_)
  {
    debounce_ms_ = now_ms; // rising edge: start the debounce window
  }
  else if (!consumed_ && (now_ms - debounce_ms_) >= kDebounceMs)
  {
    pending_  = true;
    consumed_ = true; // block re-fire while the button remains held
  }
  raw_prev_ = raw;
}

bool PushButton::IsPressed()
{
  if (pending_)
  {
    pending_ = false;
    return true;
  }
  return false;
}
