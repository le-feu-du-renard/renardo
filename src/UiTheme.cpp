#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "UiTheme.h"
#include "config.h"

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

void FormatDamperState(float position, bool target_open, char *out, size_t length)
{
  if (isnan(position))
  {
    snprintf(out, length, "--");
    return;
  }

  // Rounded before arrival is tested, so that a register reading 99.6 % is
  // called OUVERT rather than "OUV.100%", which would claim to be part way and
  // print a full opening in the same breath.
  int opening = static_cast<int>(lroundf(position));
  int target  = target_open ? 100 : 0;

  if (abs(opening - target) <= static_cast<int>(DAMPER_POSITION_TOLERANCE))
  {
    snprintf(out, length, target_open ? "OUVERT" : "FERME");
  }
  else
  {
    snprintf(out, length, target_open ? "OUV.%d%%" : "FER.%d%%", opening);
  }
}

} // namespace UiTheme
