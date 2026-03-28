#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>
#include "IndicatorLEDs.h"

// Reads all physical inputs: potentiometers, mode selector, and START/STOP buttons.
// Call Update() regularly (every INPUT_UPDATE_INTERVAL ms) from the main loop.

class InputHandler
{
public:
  InputHandler();

  void Begin(IndicatorLEDs &leds);

  // Must be called periodically to debounce buttons and sample ADC.
  void Update();

  // --- Potentiometer readings (mapped to physical ranges) ---
  float GetTargetTemperature() const { return target_temperature_; }
  float GetTargetHumidity()    const { return target_humidity_; }

  // --- Mode selector (true = ECO, false = PERFORMANCE) ---
  bool IsEcoMode() const { return eco_mode_; }

  // --- Buttons (return true once per press, edge-triggered) ---
  bool IsStartPressed();
  bool IsStopPressed();

  // --- Button LEDs ---
  void SetStartLed(bool state);
  void SetStopLed(bool state);

private:
  // Potentiometer values (updated each call to Update)
  float target_temperature_;
  float target_humidity_;

  // Mode selector
  bool eco_mode_;

  // Button debounce state
  bool     start_raw_prev_;
  bool     stop_raw_prev_;
  bool     start_pending_;   // Unconsumed press event
  bool     stop_pending_;
  uint32_t start_debounce_ms_;
  uint32_t stop_debounce_ms_;

  static constexpr uint32_t kDebounceMs = 50;

  IndicatorLEDs *leds_;

  // Map raw 12-bit ADC value to [min, max]
  static float MapAdc(uint16_t raw, float min_val, float max_val);
};

#endif // INPUT_HANDLER_H
