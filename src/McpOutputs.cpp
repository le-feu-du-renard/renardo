#include "McpOutputs.h"
#include "Logger.h"

McpOutputs::McpOutputs() : state_(0), initialized_(false) {}

bool McpOutputs::Begin(uint8_t i2c_address, TwoWire &wire, uint8_t initial_portb)
{
  if (!mcp_.begin_I2C(i2c_address, &wire))
  {
    Logger::Error("McpOutputs: MCP23017 not found at 0x%02X", i2c_address);
    initialized_ = false;
    return false;
  }
  initialized_ = true;

  // Port A – indicator LEDs: output, all off
  for (uint8_t pin = 0; pin < 8; pin++)
  {
    mcp_.pinMode(pin, OUTPUT);
    mcp_.digitalWrite(pin, LOW);
  }
  state_ = 0;

  // Port B – relay + button outputs.
  // Write initial_portb into OLAT_B while pins are still in INPUT mode.
  // On MCP23017, writing to GPIO while IODIR=INPUT updates OLAT without
  // affecting physical pins. When we then switch to OUTPUT, each pin
  // immediately drives the correct level — no LOW glitch on relay pins.
  mcp_.writeGPIOB(initial_portb);
  for (uint8_t pin = 8; pin <= 15; pin++)
  {
    mcp_.pinMode(pin, OUTPUT);
  }

  Logger::Info("McpOutputs: MCP23017 ready at 0x%02X (portb=0x%02X)", i2c_address, initial_portb);
  return true;
}

void McpOutputs::UpdateAll(uint8_t bitmask)
{
  if (!initialized_) return;
  if (bitmask == state_) return;  // No change, skip I2C write
  state_ = bitmask;
  mcp_.writeGPIOA(bitmask);  // Single atomic I2C transaction for all 8 Port A pins
}

void McpOutputs::Clear()
{
  UpdateAll(0);
}

void McpOutputs::SetOutput(uint8_t mcp_pin, bool state)
{
  if (!initialized_) return;
  mcp_.digitalWrite(mcp_pin, state ? HIGH : LOW);
}
