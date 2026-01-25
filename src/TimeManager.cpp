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

  // Check current RTC time
  DateTime now = rtc_.now();
  DateTime compileTime(F(__DATE__), F(__TIME__));

  Serial.print("Current RTC time: ");
  Serial.print(now.year());
  Serial.print("-");
  Serial.print(now.month());
  Serial.print("-");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());

  // Update RTC time only if it's invalid or outdated
  bool needsUpdate = false;

  // Check if year is 2000 (default reset value)
  if (now.year() < 2020)
  {
    Serial.println("RTC time is invalid (year < 2020), updating...");
    needsUpdate = true;
  }
  // Check if RTC time is in the future compared to compile time
  else if (now.unixtime() > compileTime.unixtime() + 86400)
  {
    Serial.println("RTC time is more than 1 day in the future, updating...");
    needsUpdate = true;
  }
  // Check if RTC is not running (lost power)
  else if (!rtc_.isrunning())
  {
    Serial.println("RTC oscillator not running, updating time...");
    needsUpdate = true;
  }

  if (needsUpdate)
  {
    Serial.print("Setting RTC to compile time: ");
    Serial.print(compileTime.year());
    Serial.print("-");
    Serial.print(compileTime.month());
    Serial.print("-");
    Serial.print(compileTime.day());
    Serial.print(" ");
    Serial.print(compileTime.hour());
    Serial.print(":");
    Serial.print(compileTime.minute());
    Serial.print(":");
    Serial.println(compileTime.second());

    rtc_.adjust(compileTime);
    delay(100); // Wait for I2C write to complete

    now = rtc_.now();
    Serial.print("RTC time updated to: ");
    Serial.print(now.year());
    Serial.print("-");
    Serial.print(now.month());
    Serial.print("-");
    Serial.print(now.day());
    Serial.print(" ");
    Serial.print(now.hour());
    Serial.print(":");
    Serial.print(now.minute());
    Serial.print(":");
    Serial.println(now.second());
  }
  else
  {
    Serial.println("RTC time is valid, no update needed");
  }

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
