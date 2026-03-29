#include "IndicatorLEDs.h"
#include "Logger.h"

IndicatorLEDs::IndicatorLEDs() : state_(0), initialized_(false) {}

bool IndicatorLEDs::Begin(uint8_t i2c_address, TwoWire &wire)
{
  if (!mcp_.begin_I2C(i2c_address, &wire))
  {
    Logger::Error("IndicatorLEDs: MCP23017 not found at 0x%02X", i2c_address);
    initialized_ = false;
    return false;
  }
  initialized_ = true;

  // Port A – all 8 indicator LEDs as outputs, off
  for (uint8_t pin = 0; pin < 8; pin++)
  {
    mcp_.pinMode(pin, OUTPUT);
    mcp_.digitalWrite(pin, LOW);
  }
  state_ = 0;

  // Port B – outputs (GPB0-GPB4), all off
  for (uint8_t pin = 8; pin <= 12; pin++)
  {
    mcp_.pinMode(pin, OUTPUT);
    mcp_.digitalWrite(pin, LOW);
  }

  Logger::Info("IndicatorLEDs: MCP23017 ready at 0x%02X", i2c_address);
  return true;
}

void IndicatorLEDs::Set(LedId id, bool state)
{
  if (!initialized_) return;
  uint8_t bit = static_cast<uint8_t>(id);
  if (state)
  {
    state_ |= (1u << bit);
  }
  else
  {
    state_ &= ~(1u << bit);
  }
  mcp_.digitalWrite(bit, state ? HIGH : LOW);
}

void IndicatorLEDs::UpdateAll(uint8_t bitmask)
{
  if (!initialized_) return;
  if (bitmask == state_) return;  // No change, skip I2C write
  state_ = bitmask;
  mcp_.writeGPIOA(bitmask);  // Single atomic I2C transaction for all 8 Port A pins
}

void IndicatorLEDs::Clear()
{
  UpdateAll(0);
}

void IndicatorLEDs::SetOutput(uint8_t mcp_pin, bool state)
{
  if (!initialized_) return;
  mcp_.digitalWrite(mcp_pin, state ? HIGH : LOW);
}
