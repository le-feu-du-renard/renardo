#include "Logger.h"

void Logger::Init(int level)
{
  // Initialize ArduinoLog
  Log.begin(level, &Serial);

  // Configure log format
  // Format: [LEVEL] message
  Log.setPrefix([](Print *_logOutput, int logLevel)
                {
    const char* levelStr;
    switch (logLevel) {
      case LOG_LEVEL_FATAL:   levelStr = "FATAL"; break;
      case LOG_LEVEL_ERROR:   levelStr = "ERROR"; break;
      case LOG_LEVEL_WARNING: levelStr = "WARN "; break;
      case LOG_LEVEL_NOTICE:  levelStr = "INFO "; break;
      case LOG_LEVEL_TRACE:   levelStr = "TRACE"; break;
      case LOG_LEVEL_VERBOSE: levelStr = "DEBUG"; break;
      default:                levelStr = "?????"; break;
    }
    _logOutput->print("[");
    _logOutput->print(levelStr);
    _logOutput->print("] "); });

  Log.setSuffix([](Print *_logOutput, int logLevel)
                { _logOutput->print('\n'); });

  Log.notice("Logger initialized (level=%d)", level);
}

void Logger::SetLevel(int level)
{
  Log.setLevel(level);
  Log.notice("Log level changed to %d", level);
}

void Logger::Flush()
{
  Serial.flush();
}
