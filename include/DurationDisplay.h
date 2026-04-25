#ifndef DURATION_DISPLAY_H
#define DURATION_DISPLAY_H

#include <Arduino.h>
#include <TM1637Display.h>
#include "config.h"

// Drives a TM1637 4-digit LED display to show session duration.
//
// Display format:
//   seconds < 3600  → MM:SS  (e.g. "05:23")
//   seconds >= 3600 → HH:MM  (e.g. "01:30"), clamped to 99h59m

class DurationDisplay
{
public:
  DurationDisplay();

  // Configure CLK/DIO pins and set initial brightness.
  void Begin();

  // Update display from elapsed seconds.
  void SetDuration(uint32_t seconds);

  // Turn off all segments.
  void Clear();

private:
  TM1637Display display_;

  static constexpr uint8_t kBrightness = 7;
  static constexpr uint32_t kMaxSeconds = 99 * 3600 + 59 * 60; // 99h59m
};

#endif // DURATION_DISPLAY_H
