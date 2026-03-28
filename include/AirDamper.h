#ifndef AIR_DAMPER_H
#define AIR_DAMPER_H

#include <Arduino.h>
#include "IndicatorLEDs.h"

// Binary air damper control via MCP23017 GPB4 (MCP_AIR_DAMPER_PIN).

class AirDamper
{
public:
  AirDamper();
  void Begin(IndicatorLEDs &leds);

  void Open();
  void Close();
  bool IsOpen() const { return is_open_; }

private:
  IndicatorLEDs *leds_;
  bool           is_open_;
};

#endif // AIR_DAMPER_H
