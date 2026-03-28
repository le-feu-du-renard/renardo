#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <ArduinoLog.h>

/**
 * @brief Logger wrapper for ArduinoLog
 *
 * This class provides a centralized logging interface that outputs to Serial.
 * It uses ArduinoLog for formatting and level management.
 *
 * Usage:
 *   Logger::Init(LOG_LEVEL_VERBOSE);  // Initialize logger
 *   Log.trace("Trace message");       // Use ArduinoLog macros
 *   Log.info("Info: %d", value);      // With formatting
 *   Log.error("Error occurred!");     // Error level
 */
class Logger
{
public:
  static void Init(int level = LOG_LEVEL_VERBOSE);
  static void SetLevel(int level);
  static void Flush();

  // Convenience wrappers matching ArduinoLog levels
  template<class... Args> static void Debug(const char* fmt, Args... args)   { Log.verbose(fmt, args...); }
  template<class... Args> static void Info(const char* fmt, Args... args)    { Log.notice(fmt, args...); }
  template<class... Args> static void Warning(const char* fmt, Args... args) { Log.warning(fmt, args...); }
  template<class... Args> static void Error(const char* fmt, Args... args)   { Log.error(fmt, args...); }

  // No-arg overloads (message only)
  static void Debug(const char* msg)   { Log.verbose(msg); }
  static void Info(const char* msg)    { Log.notice(msg); }
  static void Warning(const char* msg) { Log.warning(msg); }
  static void Error(const char* msg)   { Log.error(msg); }
};

#endif // LOGGER_H
