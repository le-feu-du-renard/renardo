#include <math.h>
#include <stdio.h>

#include "UiTheme.h"

namespace UiTheme
{

void FormatDuration(uint32_t seconds, char *out, size_t length)
{
  uint32_t hours   = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs    = seconds % 60;

  // A session can legitimately run for days, so clamp the hours field rather
  // than let it widen and push the rest of the status bar around.
  if (hours > 99)
  {
    hours = 99;
    minutes = 59;
    secs = 59;
  }

  snprintf(out, length, "%02lu:%02lu:%02lu",
           static_cast<unsigned long>(hours),
           static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(secs));
}

void FormatValue(float value, const char *unit, char *out, size_t length)
{
  if (isnan(value))
  {
    snprintf(out, length, "--.-%s", unit);
    return;
  }
  snprintf(out, length, "%.1f%s", value, unit);
}

void FormatTemperature(float celsius, char *out, size_t length)
{
  FormatValue(celsius, UI_DEGREE, out, length);
}

void FormatRounded(float value, char *out, size_t length)
{
  if (isnan(value))
  {
    snprintf(out, length, "--");
    return;
  }
  snprintf(out, length, "%d", static_cast<int>(lroundf(value)));
}

void FormatPercent(float percent, char *out, size_t length)
{
  if (isnan(percent))
  {
    snprintf(out, length, "--%%");
    return;
  }
  snprintf(out, length, "%d%%", static_cast<int>(lroundf(percent)));
}

void FormatDamperState(float position, char *out, size_t length)
{
  if (isnan(position))
  {
    snprintf(out, length, "--");
    return;
  }

  // Rounded before the end stops are tested, so that a register reading 99.6 %
  // is called OUVERT rather than "OUV. 100%", which would claim to be part way
  // and print a full opening in the same breath.
  int opening = static_cast<int>(lroundf(position));
  if (opening <= 0)
  {
    snprintf(out, length, "FERME");
  }
  else if (opening >= 100)
  {
    snprintf(out, length, "OUVERT");
  }
  else
  {
    snprintf(out, length, "OUV. %d%%", opening);
  }
}

uint8_t LoraBars(float rssi_dbm, bool linked)
{
  if (!linked || isnan(rssi_dbm))
  {
    return 0;
  }

  // Thresholds for an SX1262 at 868 MHz. There is no floor below which the
  // count drops to zero: a linked radio has just decoded a frame, so one bar
  // is the truth however weak the signal was. Zero is reserved for the link
  // being down, which is a different thing and reads differently.
  if (rssi_dbm >= -70.0f)  return 4;
  if (rssi_dbm >= -85.0f)  return 3;
  if (rssi_dbm >= -100.0f) return 2;
  return 1;
}

} // namespace UiTheme
