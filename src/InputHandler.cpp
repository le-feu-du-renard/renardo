#include "InputHandler.h"
#include "config.h"
#include "Logger.h"

InputHandler::InputHandler()
    : start_raw_prev_(false),
      stop_raw_prev_(false),
      start_pending_(false),
      stop_pending_(false),
      start_consumed_(false),
      stop_consumed_(false),
      start_debounce_ms_(0),
      stop_debounce_ms_(0) {}

void InputHandler::Begin()
{
  // Buttons – active LOW with internal pullup
  pinMode(BTN_START_PIN, INPUT_PULLUP);
  pinMode(BTN_STOP_PIN, INPUT_PULLUP);

  // Seed prev state from actual pin levels so the first Update() doesn't
  // misinterpret a held-low pin (power-up noise) as a rising edge.
  start_raw_prev_ = (digitalRead(BTN_START_PIN) == LOW);
  stop_raw_prev_  = (digitalRead(BTN_STOP_PIN)  == LOW);
  // If a button is held at init, mark it consumed so the steady-held check
  // doesn't fire immediately — the debounce timers are still 0 here.
  start_consumed_ = start_raw_prev_;
  stop_consumed_  = stop_raw_prev_;

  Logger::Info("InputHandler: initialized");
}

void InputHandler::Update()
{
  uint32_t now = millis();

  // --- START button debounce (active LOW, fires once per press) ---
  bool start_raw = (digitalRead(BTN_START_PIN) == LOW);
  if (!start_raw)
  {
    start_consumed_ = false; // Button released: allow next press
  }
  else if (!start_raw_prev_)
  {
    start_debounce_ms_ = now; // Rising edge: start debounce timer
  }
  else if (!start_consumed_ && (now - start_debounce_ms_) >= kDebounceMs)
  {
    start_pending_ = true;
    start_consumed_ = true; // Block re-fire while button remains held
  }
  start_raw_prev_ = start_raw;

  // --- STOP button debounce (active LOW, fires once per press) ---
  bool stop_raw = (digitalRead(BTN_STOP_PIN) == LOW);
  if (!stop_raw)
  {
    stop_consumed_ = false;
  }
  else if (!stop_raw_prev_)
  {
    stop_debounce_ms_ = now;
  }
  else if (!stop_consumed_ && (now - stop_debounce_ms_) >= kDebounceMs)
  {
    stop_pending_ = true;
    stop_consumed_ = true;
  }
  stop_raw_prev_ = stop_raw;
}

bool InputHandler::IsStartPressed()
{
  if (start_pending_)
  {
    start_pending_ = false;
    return true;
  }
  return false;
}

bool InputHandler::IsStopPressed()
{
  if (stop_pending_)
  {
    stop_pending_ = false;
    return true;
  }
  return false;
}
