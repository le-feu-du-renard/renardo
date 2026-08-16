#include "InputHandler.h"
#include "config.h"
#include "Logger.h"

InputHandler::InputHandler()
    : encoder_(ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_SW_PIN),
      button_raw_prev_(false),
      button_pending_(false),
      button_consumed_(false),
      button_debounce_ms_(0) {}

void InputHandler::Begin()
{
  // Button – active LOW with internal pullup
  pinMode(BTN_START_PIN, INPUT_PULLUP);

  // Seed prev state from the actual pin level so the first Update() doesn't
  // misinterpret a held-low pin (power-up noise) as a rising edge.
  button_raw_prev_ = (digitalRead(BTN_START_PIN) == LOW);
  // A button held at init is marked consumed so the steady-held check doesn't
  // fire immediately — the debounce timer is still 0 here. This matters more
  // now that one press toggles: a stuck button would otherwise stop a session
  // restored from flash the instant the board came back up.
  button_consumed_ = button_raw_prev_;

  encoder_.Begin();

  Logger::Info("InputHandler: initialized");
}

void InputHandler::Update()
{
  uint32_t now = millis();

  encoder_.Update();

  // Active LOW, fires once per press.
  bool raw = (digitalRead(BTN_START_PIN) == LOW);
  if (!raw)
  {
    button_consumed_ = false; // released: allow the next press
  }
  else if (!button_raw_prev_)
  {
    button_debounce_ms_ = now; // rising edge: start the debounce window
  }
  else if (!button_consumed_ && (now - button_debounce_ms_) >= kDebounceMs)
  {
    button_pending_  = true;
    button_consumed_ = true; // block re-fire while the button remains held
  }
  button_raw_prev_ = raw;
}

bool InputHandler::IsButtonPressed()
{
  if (button_pending_)
  {
    button_pending_ = false;
    return true;
  }
  return false;
}
