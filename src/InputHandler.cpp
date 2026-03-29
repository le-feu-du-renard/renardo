#include "InputHandler.h"
#include "config.h"
#include "Logger.h"

InputHandler::InputHandler()
    : target_temperature_(DEFAULT_TEMPERATURE_TARGET),
      target_humidity_(50.0f),
      eco_mode_(false),
      start_raw_prev_(false),
      stop_raw_prev_(false),
      start_pending_(false),
      stop_pending_(false),
      start_consumed_(false),
      stop_consumed_(false),
      start_debounce_ms_(0),
      stop_debounce_ms_(0),
      leds_(nullptr) {}

void InputHandler::Begin(IndicatorLEDs &leds)
{
  leds_ = &leds;

  // Potentiometer pins – 12-bit ADC resolution (0-4095), analog input
  analogReadResolution(12);
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

  // Read potentiometers: averaged + mapped to physical range
  target_temperature_ = ReadPot(POT_TEMPERATURE_PIN, POT_TEMP_MIN, POT_TEMP_MAX);
  target_humidity_    = ReadPot(POT_HUMIDITY_PIN,    POT_HUM_MIN,  POT_HUM_MAX);

  // Read mode selector (LOW = ECO because of internal pullup when switch open)
  eco_mode_ = (digitalRead(MODE_SELECTOR_PIN) == LOW);

  // --- START button debounce (active LOW, fires once per press) ---
  bool start_raw = (digitalRead(BTN_START_PIN) == LOW);
  if (!start_raw)
  {
    start_consumed_ = false;  // Button released: allow next press
  }
  else if (!start_raw_prev_)
  {
    start_debounce_ms_ = now;  // Rising edge: start debounce timer
  }
  else if (!start_consumed_ && (now - start_debounce_ms_) >= kDebounceMs)
  {
    start_pending_  = true;
    start_consumed_ = true;  // Block re-fire while button remains held
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
    stop_pending_  = true;
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

void InputHandler::SetStartLed(bool state)
{
  leds_->SetOutput(MCP_BTN_START_LED_PIN, state);
}

void InputHandler::SetStopLed(bool state)
{
  leds_->SetOutput(MCP_BTN_STOP_LED_PIN, state);
}

float InputHandler::ReadPot(uint8_t pin, float min_val, float max_val)
{
  // Average 8 samples to suppress RP2040 ADC noise
  constexpr uint8_t kSamples = 8;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < kSamples; i++)
  {
    sum += analogRead(pin);
  }
  return MapAdc(sum / kSamples, min_val, max_val);
}

float InputHandler::MapAdc(uint16_t raw, float min_val, float max_val)
{
  // RP2040 ADC is 12-bit (0-4095)
  constexpr float kAdcMax = 4095.0f;
  float ratio = static_cast<float>(raw) / kAdcMax;
  return min_val + ratio * (max_val - min_val);
}
