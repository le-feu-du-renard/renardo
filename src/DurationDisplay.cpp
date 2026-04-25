#include "DurationDisplay.h"
#include "Logger.h"

DurationDisplay::DurationDisplay()
    : display_(TM1637_CLK_PIN, TM1637_DIO_PIN)
{
}

void DurationDisplay::Begin()
{
  display_.setBrightness(kBrightness);
  display_.showNumberDecEx(0, 0x40, true);
  Logger::Info("DurationDisplay: TM1637 ready (CLK=%d DIO=%d)", TM1637_CLK_PIN, TM1637_DIO_PIN);
}

void DurationDisplay::SetDuration(uint32_t seconds)
{
  if (seconds > kMaxSeconds)
    seconds = kMaxSeconds;

  int value;
  if (seconds < 3600)
  {
    // MM:SS
    uint32_t minutes = seconds / 60;
    uint32_t secs = seconds % 60;
    value = static_cast<int>(minutes * 100 + secs);
  }
  else
  {
    // HH:MM
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    value = static_cast<int>(hours * 100 + minutes);
  }

  // 0x40 = colon segment between digit 1 and 2
  display_.showNumberDecEx(value, 0x40, true);
}

void DurationDisplay::Clear()
{
  display_.clear();
}
