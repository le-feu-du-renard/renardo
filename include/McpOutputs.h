#ifndef MCP_OUTPUTS_H
#define MCP_OUTPUTS_H

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include "config.h"

// Wraps all MCP23017 GPIO expander outputs:
//   Port A (GPA0-7) – 8 indicator LEDs
//   Port B (GPB0-7) – relay/damper outputs and button LEDs

enum class LedId : uint8_t
{
  kEcoMode         = MCP_LED_ECO_MODE,
  kPhaseInit       = MCP_LED_PHASE_INIT,
  kPhaseBrassage   = MCP_LED_PHASE_BRASSAGE,
  kPhaseExtraction = MCP_LED_PHASE_EXTRACTION,
  kElectricHeater  = MCP_LED_HEATER,
  kHydroHeater     = MCP_LED_HYDRO_HEATER,
  kFan             = MCP_LED_FAN,
  kAirRenewal      = MCP_LED_AIR_RENEWAL,
};

class McpOutputs
{
public:
  McpOutputs();

  // Initialise the MCP23017 at the given I2C address on the given I2C bus.
  // Returns false if the device is not found.
  bool Begin(uint8_t i2c_address, TwoWire &wire);

  // Write all 8 Port A LEDs at once from a bitmask (bit 0 = GPA0, ...).
  // Uses a single atomic I2C transaction; skips write if state is unchanged.
  void UpdateAll(uint8_t bitmask);

  // Turn all Port A LEDs off.
  void Clear();

  // Set a single Port B output (use MCP_* pin constants from config.h).
  void SetOutput(uint8_t mcp_pin, bool state);

private:
  Adafruit_MCP23X17 mcp_;
  uint8_t           state_;        // Current Port A bitmask
  bool              initialized_;  // True if MCP23017 was found
};

#endif // MCP_OUTPUTS_H
