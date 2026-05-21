#include "InputHandler.h"
#include "config.h"
#include "Logger.h"

InputHandler::InputHandler()
    : target_temperature_(TEMPERATURE_TARGET),
      target_humidity_(50.0f),
      eco_mode_(false),
      selector_raw_prev_(true),
      selector_debounce_ms_(0),
      start_raw_prev_(false),
      stop_raw_prev_(false),
      start_pending_(false),
      stop_pending_(false),
      start_consumed_(false),
      stop_consumed_(false),
      start_debounce_ms_(0),
      stop_debounce_ms_(0),
      leds_(nullptr),
      prev_target_temperature_(TEMPERATURE_TARGET),
      prev_target_humidity_(50.0f),
      temp_adjust_until_ms_(0),
      hum_adjust_until_ms_(0),
      temp_candidate_(TEMPERATURE_TARGET),
      temp_stable_count_(0),
      hum_candidate_(50.0f),
      hum_stable_count_(0) {}

void InputHandler::Begin(McpOutputs &leds)
{
  leds_ = &leds;

  // Potentiometer pins – 12-bit ADC resolution (0-4095), analog input
  analogReadResolution(12);
  pinMode(POT_TEMPERATURE_PIN, INPUT);
  pinMode(POT_HUMIDITY_PIN, INPUT);

  // Mode selector – digital input, internal pullup
  pinMode(MODE_SELECTOR_PIN, INPUT_PULLUP);

  // Buttons – active LOW with internal pullup
  pinMode(BTN_START_PIN, INPUT_PULLUP);
  pinMode(BTN_STOP_PIN, INPUT_PULLUP);

  // Seed prev state from actual pin levels so the first Update() doesn't
  // misinterpret a held-low pin (power-up noise) as a rising edge.
  start_raw_prev_ = (digitalRead(BTN_START_PIN) == LOW);
  stop_raw_prev_  = (digitalRead(BTN_STOP_PIN)  == LOW);

  // Button LEDs – via MCP23017 Port B, start off
  leds_->SetOutput(MCP_BTN_START_LED, LOW);
  leds_->SetOutput(MCP_BTN_STOP_LED, LOW);

  Logger::Info("InputHandler: initialized");
}

void InputHandler::Update()
{
  uint32_t now = millis();

  // Read potentiometers: averaged + quantized, committed only after kPotStableReads
  float raw_temp = ReadPot(POT_TEMPERATURE_PIN, POT_TEMP_MIN, POT_TEMP_MAX, 1.0f);
  if (raw_temp == temp_candidate_)
  {
    if (temp_stable_count_ < kPotStableReads) temp_stable_count_++;
    if (temp_stable_count_ == kPotStableReads) target_temperature_ = temp_candidate_;
  }
  else if (fabsf(raw_temp - target_temperature_) > 1.0f)
  {
    temp_candidate_    = raw_temp;
    temp_stable_count_ = 1;
  }

  float raw_hum = ReadPot(POT_HUMIDITY_PIN, POT_HUM_MIN, POT_HUM_MAX, 1.0f);
  if (raw_hum == hum_candidate_)
  {
    if (hum_stable_count_ < kPotStableReads) hum_stable_count_++;
    if (hum_stable_count_ == kPotStableReads) target_humidity_ = hum_candidate_;
  }
  else if (fabsf(raw_hum - target_humidity_) > 1.0f)
  {
    hum_candidate_    = raw_hum;
    hum_stable_count_ = 1;
  }

  // Detect potentiometer movement and extend the voltmeter display window
  constexpr float kTempThreshold = 0.3f;
  constexpr float kHumThreshold = 1.0f;

  if (fabsf(target_temperature_ - prev_target_temperature_) >= kTempThreshold)
  {
    prev_target_temperature_ = target_temperature_;
    temp_adjust_until_ms_ = now + kAdjustDisplayMs;
  }

  if (fabsf(target_humidity_ - prev_target_humidity_) >= kHumThreshold)
  {
    prev_target_humidity_ = target_humidity_;
    hum_adjust_until_ms_ = now + kAdjustDisplayMs;
  }

  // Read mode selector with debounce
  // Contact NO + pull-up: open (ECO) = HIGH, closed (PERFORMANCE) = LOW
  bool selector_raw = (digitalRead(MODE_SELECTOR_PIN) == HIGH);
  if (selector_raw != selector_raw_prev_)
  {
    selector_debounce_ms_ = now;
    selector_raw_prev_ = selector_raw;
  }
  else if ((now - selector_debounce_ms_) >= kDebounceMs)
  {
    eco_mode_ = selector_raw;
  }

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

void InputHandler::SetStartLed(bool state)
{
  leds_->SetOutput(MCP_BTN_START_LED, state);
}

void InputHandler::SetStopLed(bool state)
{
  leds_->SetOutput(MCP_BTN_STOP_LED, state);
}

float InputHandler::ReadPot(uint8_t pin, float min_val, float max_val, float step)
{
  // Average samples to suppress RP2040 ADC noise
  constexpr uint8_t kSamples = 20;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < kSamples; i++)
  {
    sum += analogRead(pin);
  }
  float val = MapAdc(sum / kSamples, min_val, max_val);
  return roundf(val / step) * step;
}

float InputHandler::MapAdc(uint16_t raw, float min_val, float max_val)
{
  // RP2040 ADC is 12-bit (0-4095)
  constexpr float kAdcMax = 4095.0f;
  float ratio = static_cast<float>(raw) / kAdcMax;
  return min_val + ratio * (max_val - min_val);
}

bool InputHandler::IsTemperatureBeingAdjusted() const
{
  return millis() < temp_adjust_until_ms_;
}

bool InputHandler::IsHumidityBeingAdjusted() const
{
  return millis() < hum_adjust_until_ms_;
}
