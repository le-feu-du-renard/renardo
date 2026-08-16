#ifndef LOGGER_H
#define LOGGER_H

#ifdef ARDUINO

#include <Arduino.h>
#include <ArduinoLog.h>
#include <pico/mutex.h>

class Logger
{
public:
  static void Init(int level = LOG_LEVEL_VERBOSE);
  static void SetLevel(int level);
  static void Flush();

  // Convenience wrappers matching ArduinoLog levels — all mutex-protected for dual-core safety
  template<class... Args> static void Debug(const char* fmt, Args... args)
    { mutex_enter_blocking(&mutex_); Log.verbose(fmt, args...); mutex_exit(&mutex_); }
  template<class... Args> static void Info(const char* fmt, Args... args)
    { mutex_enter_blocking(&mutex_); Log.notice(fmt, args...); mutex_exit(&mutex_); }
  template<class... Args> static void Warning(const char* fmt, Args... args)
    { mutex_enter_blocking(&mutex_); Log.warning(fmt, args...); mutex_exit(&mutex_); }
  template<class... Args> static void Error(const char* fmt, Args... args)
    { mutex_enter_blocking(&mutex_); Log.error(fmt, args...); mutex_exit(&mutex_); }

  // No-arg overloads (message only)
  static void Debug(const char* msg)
    { mutex_enter_blocking(&mutex_); Log.verbose(msg); mutex_exit(&mutex_); }
  static void Info(const char* msg)
    { mutex_enter_blocking(&mutex_); Log.notice(msg); mutex_exit(&mutex_); }
  static void Warning(const char* msg)
    { mutex_enter_blocking(&mutex_); Log.warning(msg); mutex_exit(&mutex_); }
  static void Error(const char* msg)
    { mutex_enter_blocking(&mutex_); Log.error(msg); mutex_exit(&mutex_); }

private:
  static mutex_t mutex_;
};

#else // !ARDUINO

// Native test build: the domain classes log freely, and the unit tests compile
// them as-is. Logging compiles away to nothing so no ArduinoLog, no Serial and
// no pico mutex are needed on the host.
class Logger
{
public:
  static void Init(int = 0) {}
  static void SetLevel(int) {}
  static void Flush() {}

  template<class... Args> static void Debug(const char*, Args...) {}
  template<class... Args> static void Info(const char*, Args...) {}
  template<class... Args> static void Warning(const char*, Args...) {}
  template<class... Args> static void Error(const char*, Args...) {}
};

#endif // ARDUINO

#endif // LOGGER_H
