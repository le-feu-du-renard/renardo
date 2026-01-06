#ifndef DISPLAY_HELPERS_H
#define DISPLAY_HELPERS_H

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cmath>

/**
 * Display helper functions
 * Port of ESPHome display helpers for exact layout matching
 */

inline const char *FormatDuration(uint32_t seconds)
{
  static char buf[16];
  uint32_t h = seconds / 3600U;
  uint32_t m = (seconds % 3600U) / 60U;
  uint32_t s = seconds % 60U;
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, s);
  return buf;
}

inline int16_t GetProgressBarPosition(int16_t x_start, int16_t bar_width,
                                      float percent)
{
  return x_start + (int16_t)(bar_width * percent);
}

inline float GetProgress(uint32_t current, uint32_t total)
{
  if (total == 0)
  {
    return 0.0f;
  }
  float ratio = float(current) / float(total);
  return std::clamp(ratio, 0.0f, 1.0f);
}

inline const char *FormatTemperature(float temperature)
{
  static char buf[16];

  if (std::isnan(temperature))
  {
    snprintf(buf, sizeof(buf), "--.-°");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%.1f%cC", temperature, (char)247);
  }

  return buf;
}

inline const char *FormatPercent(float percent)
{
  static char buf[16];

  if (std::isnan(percent))
  {
    snprintf(buf, sizeof(buf), "--%%");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%.0f%%", percent);
  }

  return buf;
}

inline const char *FormatTemperaturePercent(float temperature, float percent)
{
  static char buf[32];

  const char *t = FormatTemperature(temperature);
  const char *h = FormatPercent(percent);

  snprintf(buf, sizeof(buf), "%s %s", t, h);

  return buf;
}

inline const char *GetStateStr(bool state)
{
  return state ? "ON" : "OFF";
}

#endif // DISPLAY_HELPERS_H
