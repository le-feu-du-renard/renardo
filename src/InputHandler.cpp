#include "InputHandler.h"
#include "config.h"
#include "Logger.h"

InputHandler::InputHandler()
    : target_temperature_(DEFAULT_TEMPERATURE_TARGET),
      target_humidity_(50.0f),
      eco_mode_(false),
      start_raw_prev_(true),
      stop_raw_prev_(true),
      start_pending_(false),
      stop_pending_(false),
      start_debounce_ms_(0),
      stop_debounce_ms_(0),
      leds_(nullptr) {}

void InputHandler::Begin(IndicatorLEDs &leds)
{
  leds_ = &leds;

  // Potentiometer pins – configure as analog inputs (default on RP2040, but explicit)
  pinMode(POT_TEMPERATURE_PIN, INPUT);
  pinMode(POT_HUMIDITY_PIN,    INPUT);

  // Mode selector – digital input, internal pullup
  pinMode(MODE_SELECTOR_PIN, INPUT_PULLUP);

  // Buttons – active LOW with internal pullup
  pinMode(BTN_START_PIN, INPUT_PULLUP);
  pinMode(BTN_STOP_PIN,  INPUT_PULLUP);

  // Button LEDs – via MCP23017 Port B, start off
  leds_->SetOutput(MCP_BTN_START_LED_PIN, LOW);
  leds_->SetOutput(MCP_BTN_STOP_LED_PIN,  LOW);

  Logger::Info("InputHandler: initialized");
}

void InputHandler::Update()
{
  uint32_t now = millis();

  // Read potentiometers (12-bit ADC on RP2040: 0-4095)
  target_temperature_ = MapAdc(analogRead(POT_TEMPERATURE_PIN), POT_TEMP_MIN, POT_TEMP_MAX);
  target_humidity_    = MapAdc(analogRead(POT_HUMIDITY_PIN),    POT_HUM_MIN,  POT_HUM_MAX);

  // Read mode selector (LOW = ECO because of internal pullup when switch open)
  eco_mode_ = (digitalRead(MODE_SELECTOR_PIN) == LOW);

  // --- START button debounce (active LOW) ---
  bool start_raw = (digitalRead(BTN_START_PIN) == LOW);
  if (start_raw && !start_raw_prev_)
  {
    start_debounce_ms_ = now;
  }
  if (start_raw && !start_pending_ && (now - start_debounce_ms_) >= kDebounceMs)
  {
    start_pending_ = true;
  }
  start_raw_prev_ = start_raw;

  // --- STOP button debounce (active LOW) ---
  bool stop_raw = (digitalRead(BTN_STOP_PIN) == LOW);
  if (stop_raw && !stop_raw_prev_)
  {
    stop_debounce_ms_ = now;
  }
  if (stop_raw && !stop_pending_ && (now - stop_debounce_ms_) >= kDebounceMs)
  {
    stop_pending_ = true;
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

void InputHandler::SetStartLed(bool state)
{
  leds_->SetOutput(MCP_BTN_START_LED_PIN, state);
}

void InputHandler::SetStopLed(bool state)
{
  leds_->SetOutput(MCP_BTN_STOP_LED_PIN, state);
}

float InputHandler::MapAdc(uint16_t raw, float min_val, float max_val)
{
  // RP2040 ADC is 12-bit (0-4095)
  constexpr float kAdcMax = 4095.0f;
  float ratio = static_cast<float>(raw) / kAdcMax;
  return min_val + ratio * (max_val - min_val);
}
