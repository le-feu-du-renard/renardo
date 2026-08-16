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

} // namespace UiTheme
