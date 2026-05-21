#ifndef LOGGER_H
#define LOGGER_H

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

#endif // LOGGER_H
