#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include "StatusIndicator.h"

// The two panel LEDs, green and red, driven straight from a GPIO through a
// series resistor. Active HIGH: anode on the pin, cathode to ground.
//
// OutputDriver was the obvious candidate for these two pins and is deliberately
// not used: it logs every transition, which would put two Logger::Info lines a
// second into the log for as long as a LED blinks. What is worth logging is the
// *state* changing, once, and main.cpp does that.
class StatusLed
{
public:
  StatusLed(uint8_t green_pin, uint8_t red_pin);

  // Drives both pins low *before* switching them to OUTPUT, so neither LED
  // flashes at boot.
  void Begin();

  // Writes only the pins whose level actually changes.
  void Apply(const LedPattern &pattern);

private:
  uint8_t green_pin_;
  uint8_t red_pin_;
  bool    green_on_;
  bool    red_on_;
};

#endif // STATUS_LED_H
