#include "TimeManager.h"

#include "Logger.h"

TimeManager::TimeManager(TwoWire *wire)
    : wire_(wire), initialized_(false)
{
}

bool TimeManager::Begin()
{
  if (!rtc_.begin(wire_))
  {
    Logger::Error("RTC: DS1307 not found");
    initialized_ = false;
    return false;
  }

  // The chip answered, so the accessors below are usable for the rest of this
  // function — including the ones that log the time.
  initialized_ = true;

  DateTime now = rtc_.now();
  Logger::Info("RTC: current time %s", GetDateTimeString().c_str());

  // Seeded with the compile time only when the chip clearly has no time of its
  // own: a clock the user set from the menu must outlive both reboots and
  // firmware updates, and comparing against the compile time would overwrite it
  // on the very next boot.
  bool needs_seed = false;
  if (now.year() < 2020)
  {
    Logger::Warning("RTC: time never set (year < 2020), seeding from build");
    needs_seed = true;
  }
  else if (!rtc_.isrunning())
  {
    Logger::Warning("RTC: oscillator stopped, seeding from build");
    needs_seed = true;
  }

  if (needs_seed)
  {
    DateTime compile_time(F(__DATE__), F(__TIME__));
    rtc_.adjust(compile_time);
    delay(100); // Let the I2C write complete before reading back

    Logger::Info("RTC: seeded to %s — set the real time from Systeme > Date / Heure",
                 GetDateTimeString().c_str());
  }

  return true;
}

bool TimeManager::IsRunning()
{
  return initialized_ && rtc_.isrunning();
}

DateTime TimeManager::GetNow()
{
  if (!initialized_)
  {
    Logger::Warning("RTC not initialized, returning invalid DateTime");
    return DateTime(2000, 1, 1, 0, 0, 0);
  }
  return rtc_.now();
}

void TimeManager::SetTime(const DateTime &dt)
{
  if (!initialized_)
  {
    Logger::Error("RTC not initialized, time not set");
    return;
  }

  rtc_.adjust(dt);
  Logger::Info("RTC time set to %s", GetDateTimeString().c_str());
}

void TimeManager::SetTime(uint16_t year, uint8_t month, uint8_t day,
                          uint8_t hour, uint8_t minute, uint8_t second)
{
  DateTime dt(year, month, day, hour, minute, second);
  SetTime(dt);
}

String TimeManager::GetTimeString()
{
  if (!initialized_)
    return "00:00:00";

  DateTime now = rtc_.now();
  char buffer[9]; // HH:MM:SS + null terminator
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
           now.hour(), now.minute(), now.second());
  return String(buffer);
}

String TimeManager::GetDateString()
{
  if (!initialized_)
    return "2000-01-01";

  DateTime now = rtc_.now();
  char buffer[11]; // YYYY-MM-DD + null terminator
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
           now.year(), now.month(), now.day());
  return String(buffer);
}

String TimeManager::GetDateTimeString()
{
  if (!initialized_)
    return "2000-01-01 00:00:00";

  DateTime now = rtc_.now();
  char buffer[20]; // YYYY-MM-DD HH:MM:SS + null terminator
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buffer);
}

String TimeManager::GetTimestampFilename()
{
  if (!initialized_)
    return "20000101_000000";

  DateTime now = rtc_.now();
  char buffer[16]; // YYYYMMDD_HHMMSS + null terminator
  snprintf(buffer, sizeof(buffer), "%04d%02d%02d_%02d%02d%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buffer);
}

bool TimeManager::HasLostPower()
{
  if (!initialized_)
    return true;

  // Check if RTC is not running (indicates power loss)
  return !rtc_.isrunning();
}

uint32_t TimeManager::GetUnixTime()
{
  if (!initialized_)
    return 0;

  return rtc_.now().unixtime();
}
