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
  /**
   * @brief Initialize the logger
   * @param level Log level (LOG_LEVEL_SILENT, LOG_LEVEL_FATAL, LOG_LEVEL_ERROR,
   *              LOG_LEVEL_WARNING, LOG_LEVEL_NOTICE, LOG_LEVEL_TRACE, LOG_LEVEL_VERBOSE)
   */
  static void Init(int level = LOG_LEVEL_VERBOSE);

  /**
   * @brief Set log level
   * @param level New log level
   */
  static void SetLevel(int level);

  /**
   * @brief Flush any buffered log data
   */
  static void Flush();
};

// Convenience macros for logging
#define LOG_TRACE(...) Log.trace(__VA_ARGS__)
#define LOG_VERBOSE(...) Log.verbose(__VA_ARGS__)
#define LOG_NOTICE(...) Log.notice(__VA_ARGS__)
#define LOG_WARNING(...) Log.warning(__VA_ARGS__)
#define LOG_ERROR(...) Log.error(__VA_ARGS__)
#define LOG_FATAL(...) Log.fatal(__VA_ARGS__)

#endif // LOGGER_H
