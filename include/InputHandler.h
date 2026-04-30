#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>
#include "McpOutputs.h"

// Reads all physical inputs: potentiometers, mode selector, and START/STOP buttons.
// Call Update() regularly (every INPUT_UPDATE_INTERVAL ms) from the main loop.

class InputHandler
{
public:
  InputHandler();

  void Begin(McpOutputs &leds);

  // Must be called periodically to debounce buttons and sample ADC.
  void Update();

  // --- Potentiometer readings (mapped to physical ranges) ---
  float GetTargetTemperature() const { return target_temperature_; }
  float GetTargetHumidity()    const { return target_humidity_; }

  // --- Mode selector (true = ECO, false = PERFORMANCE) ---
  bool IsEcoMode() const { return eco_mode_; }

  // --- Potentiometer adjustment detection ---
  // Returns true for kAdjustDisplayMs after the last potentiometer change.
  bool IsTemperatureBeingAdjusted() const;
  bool IsHumidityBeingAdjusted()    const;

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
  bool     eco_mode_;
  bool     selector_raw_prev_;
  uint32_t selector_debounce_ms_;

  // Button debounce state
  bool     start_raw_prev_;
  bool     stop_raw_prev_;
  bool     start_pending_;    // Unconsumed press event
  bool     stop_pending_;
  bool     start_consumed_;   // True after event fired; cleared on button release
  bool     stop_consumed_;
  uint32_t start_debounce_ms_;
  uint32_t stop_debounce_ms_;

  static constexpr uint32_t kDebounceMs      = 50;
  static constexpr uint32_t kAdjustDisplayMs = 3000;

  McpOutputs *leds_;

  float    prev_target_temperature_;
  float    prev_target_humidity_;
  uint32_t temp_adjust_until_ms_;
  uint32_t hum_adjust_until_ms_;

  // Average 8 ADC samples and map to [min, max]
  static float ReadPot(uint8_t pin, float min_val, float max_val);

  // Map raw 12-bit ADC value to [min, max]
  static float MapAdc(uint16_t raw, float min_val, float max_val);
};

#endif // INPUT_HANDLER_H
