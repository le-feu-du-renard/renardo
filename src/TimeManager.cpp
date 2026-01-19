#include "TimeManager.h"

TimeManager::TimeManager(TwoWire *wire)
    : wire_(wire), initialized_(false)
{
}

bool TimeManager::Begin()
{
  if (!rtc_.begin(wire_))
  {
    Serial.println("ERROR: Couldn't find RTC DS1307");
    initialized_ = false;
    return false;
  }

  Serial.println("RTC DS1307 initialized successfully");

  // Check if RTC is running
  if (!rtc_.isrunning())
  {
    Serial.println("WARNING: RTC is not running, time needs to be set!");
    Serial.println("Time will be set to compile time as default");
    // Set to compile time as default
    rtc_.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Print current time
  DateTime now = rtc_.now();
  Serial.print("Current RTC time: ");
  Serial.println(GetDateTimeString());

  initialized_ = true;
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
    Serial.println("WARNING: RTC not initialized, returning invalid DateTime");
    return DateTime(2000, 1, 1, 0, 0, 0);
  }
  return rtc_.now();
}

void TimeManager::SetTime(const DateTime &dt)
{
  if (!initialized_)
  {
    Serial.println("ERROR: RTC not initialized");
    return;
  }

  rtc_.adjust(dt);
  Serial.print("RTC time set to: ");
  Serial.println(GetDateTimeString());
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
