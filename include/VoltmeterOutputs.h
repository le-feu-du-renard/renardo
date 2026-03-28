#ifndef VOLTMETER_OUTPUTS_H
#define VOLTMETER_OUTPUTS_H

#include <Arduino.h>
#include "config.h"

// Drives 4 analog panel voltmeters (0-3 V) via PWM + RC low-pass filters.
//
// Hardware: each GPIO drives an RC filter (10 kOhm / 100 uF 16V, fc ~0.16 Hz, tau ~1 s).
// PWM frequency: ~50 kHz.  Duty cycle 0-91% maps to 0-3.0 V on a 3.3 V rail.
//
// Channel assignment and pin mapping defined in config.h:
//   CH1 - VOLTMETER_CH1_TEMPERATURE_PIN    (0 V -> 0,  3 V -> VOLTMETER_TEMP_MAX C)
//   CH2 - VOLTMETER_CH2_HUMIDITY_PIN       (0 V -> 0,  3 V -> VOLTMETER_HUM_MAX %RH)
//   CH3 - VOLTMETER_CH3_TOTAL_DURATION_PIN (0 V -> 0,  3 V -> VOLTMETER_TOTAL_DUR_H hours)
//   CH4 - VOLTMETER_CH4_PHASE_DURATION_PIN (0 V -> 0,  3 V -> VOLTMETER_PHASE_DUR_MIN minutes)

class VoltmeterOutputs
{
public:
  VoltmeterOutputs();

  // Configure PWM on all four output pins.
  void Begin();

  // Set individual channels (values are clamped to their configured range).
  void SetTemperature(float celsius);
  void SetHumidity(float percent);
  void SetTotalDuration(float seconds);
  void SetPhaseDuration(float seconds);

private:
  // Write a duty cycle in the range [0.0, 1.0] to the given pin.
  static void WriteDuty(uint8_t pin, float duty);

  // Map a value [0, max] to duty cycle [0, kDutyMax].
  static float ValueToDuty(float value, float max_value);

  // Maximum duty cycle: 3.0 V / 3.3 V ~0.909
  static constexpr float kDutyMax = 3.0f / 3.3f;
};

#endif // VOLTMETER_OUTPUTS_H
