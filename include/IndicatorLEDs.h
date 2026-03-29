#ifndef INDICATOR_LEDS_H
#define INDICATOR_LEDS_H

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include "config.h"

// Controls all MCP23017 outputs:
//   Port A (GPA0-7) – 8 indicator LEDs
//   Port B (GPB0-4) – button LEDs and relay/damper outputs
//
// MCP23017 Port A mapping:
//   GPA0 - ECO mode
//   GPA1 - Phase initialisation
//   GPA2 - Phase brassage
//   GPA3 - Phase extraction
//   GPA4 - Chauffage électrique
//   GPA5 - Chauffage hydraulique
//   GPA6 - Ventilation
//   GPA7 - Renouvellement d'air
//
// MCP23017 Port B mapping:
//   GPB0 - Voyant bouton START
//   GPB1 - Voyant bouton STOP
//   GPB2 - Relais chauffage électrique
//   GPB3 - Relais ventilation
//   GPB4 - Registre d'air

enum class LedId : uint8_t
{
  kEcoMode         = LED_PIN_ECO_MODE,
  kPhaseInit       = LED_PIN_PHASE_INIT,
  kPhaseBrassage   = LED_PIN_PHASE_BRASSAGE,
  kPhaseExtraction = LED_PIN_PHASE_EXTRACTION,
  kElectricHeater  = LED_PIN_ELECTRIC_HEATER,
  kHydroHeater     = LED_PIN_HYDRO_HEATER,
  kFan             = LED_PIN_FAN,
  kAirRenewal      = LED_PIN_AIR_RENEWAL,
};

class IndicatorLEDs
{
public:
  IndicatorLEDs();

  // Initialise the MCP23017 at the given I2C address on the given I2C bus.
  // Returns false if the device is not found.
  bool Begin(uint8_t i2c_address, TwoWire &wire);

  // Set a single LED on or off.
  void Set(LedId id, bool state);

  // Write all 8 Port A LEDs at once from a bitmask (bit 0 = GPA0, ...).
  // Uses a single atomic I2C transaction; skips write if state is unchanged.
  void UpdateAll(uint8_t bitmask);

  // Turn all LEDs off.
  void Clear();

  // Set a single Port B output (use MCP_* pin constants from config.h).
  void SetOutput(uint8_t mcp_pin, bool state);

private:
  Adafruit_MCP23X17 mcp_;
  uint8_t           state_;        // Current Port A bitmask
  bool              initialized_;  // True if MCP23017 was found
};

#endif // INDICATOR_LEDS_H
