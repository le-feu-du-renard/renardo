#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

/**
 * @brief Manages the RTC (DS1307) for timekeeping and session logging
 *
 * This class provides an interface to the DS1307 RTC module connected
 * on I2C bus 1. It handles initialization, time setting, and time retrieval
 * for logging session data with timestamps.
 */
class TimeManager
{
public:
  /**
   * @brief Construct a new Time Manager object
   * @param wire Pointer to the I2C bus to use (should be i2c_bus_1)
   */
  TimeManager(TwoWire *wire);

  /**
   * @brief Initialize the RTC module
   * @return true if initialization successful, false otherwise
   */
  bool Begin();

  /**
   * @brief Check if RTC is running
   * @return true if RTC is running, false otherwise
   */
  bool IsRunning();

  /**
   * @brief Get current date and time
   * @return DateTime object with current time
   */
  DateTime GetNow();

  /**
   * @brief Set the RTC time
   * @param dt DateTime object to set
   */
  void SetTime(const DateTime &dt);

  /**
   * @brief Set the RTC time from components
   * @param year Year (e.g., 2026)
   * @param month Month (1-12)
   * @param day Day (1-31)
   * @param hour Hour (0-23)
   * @param minute Minute (0-59)
   * @param second Second (0-59)
   */
  void SetTime(uint16_t year, uint8_t month, uint8_t day,
               uint8_t hour, uint8_t minute, uint8_t second);

  /**
   * @brief Get formatted time string (HH:MM:SS)
   * @return String with formatted time
   */
  String GetTimeString();

  /**
   * @brief Get formatted date string (YYYY-MM-DD)
   * @return String with formatted date
   */
  String GetDateString();

  /**
   * @brief Get formatted date and time string (YYYY-MM-DD HH:MM:SS)
   * @return String with formatted date and time
   */
  String GetDateTimeString();

  /**
   * @brief Get formatted timestamp for filename (YYYYMMDD_HHMMSS)
   * @return String with formatted timestamp suitable for filenames
   */
  String GetTimestampFilename();

  /**
   * @brief Check if RTC has lost power and needs time reset
   * @return true if RTC lost power, false otherwise
   */
  bool HasLostPower();

  /**
   * @brief Get Unix timestamp (seconds since 1970-01-01)
   * @return uint32_t Unix timestamp
   */
  uint32_t GetUnixTime();

private:
  RTC_DS1307 rtc_;
  TwoWire *wire_;
  bool initialized_;
};

#endif // TIME_MANAGER_H
