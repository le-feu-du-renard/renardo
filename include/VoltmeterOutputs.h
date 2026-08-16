#ifndef VOLTMETER_OUTPUTS_H
#define VOLTMETER_OUTPUTS_H

#include <Arduino.h>
#include "config.h"

// Drives 4 analog panel voltmeters (0-3 V) directly via PWM.
//
// PWM frequency: ~50 kHz.  Duty cycle 0-91% maps to 0-3.0 V on a 3.3 V rail.
//
// Channel assignment and pin mapping defined in config.h:
//   VOLTMETER_INLET_TEMPERATURE_PIN  (0 V -> 0,  3 V -> VOLTMETER_TEMPERATURE_MAX C)
//   VOLTMETER_INLET_HUMIDITY_PIN     (0 V -> 0,  3 V -> VOLTMETER_HUMIDITY_MAX %RH)
//   VOLTMETER_OUTLET_TEMPERATURE_PIN (0 V -> 0,  3 V -> VOLTMETER_TEMPERATURE_MAX C)
//   VOLTMETER_OUTLET_HUMIDITY_PIN    (0 V -> 0,  3 V -> VOLTMETER_HUMIDITY_MAX %RH)

class VoltmeterOutputs
{
public:
  VoltmeterOutputs();

  // Configure PWM on all four output pins.
  void Begin();

  // Set individual channels (values are clamped to their configured range).
  void SetInletTemperature(float celsius);
  void SetInletHumidity(float percent);
  void SetOutletTemperature(float celsius);
  void SetOutletHumidity(float percent);

private:
  // Write a normalised ratio [0.0, 1.0] to a channel using its calibrated raw range.
  static void WriteRatio(uint8_t pin, float ratio, int16_t val_min, uint16_t val_max);
};

#endif // VOLTMETER_OUTPUTS_H
