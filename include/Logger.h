#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <ArduinoLog.h>

/**
 * @brief Logger wrapper for ArduinoLog with SD card support
 *
 * This class provides a centralized logging interface that can output to both
 * Serial and SD card. It uses ArduinoLog for formatting and level management.
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
   * @param enable_sd Enable SD card logging
   */
  static void Init(int level = LOG_LEVEL_VERBOSE, bool enable_sd = false);

  /**
   * @brief Set log level
   * @param level New log level
   */
  static void SetLevel(int level);

  /**
   * @brief Enable or disable SD card logging
   * @param enable true to enable SD logging
   */
  static void EnableSDLogging(bool enable);

  /**
   * @brief Check if SD logging is enabled
   * @return true if SD logging is enabled
   */
  static bool IsSDLoggingEnabled();

  /**
   * @brief Set the SD log file
   * @param filename Name of the log file on SD card
   * @return true if file was opened successfully
   */
  static bool SetSDLogFile(const char *filename);

  /**
   * @brief Close the SD log file
   */
  static void CloseSDLogFile();

  /**
   * @brief Flush any buffered log data
   */
  static void Flush();

private:
  static bool sd_logging_enabled;
  static void *sd_file; // Using void* to avoid including SD.h in header

  // Custom print function that outputs to both Serial and SD
  static void CustomPrintFunction(Print* _logOutput, int logLevel);
};

// Convenience macros for logging
#define LOG_TRACE(...) Log.trace(__VA_ARGS__)
#define LOG_VERBOSE(...) Log.verbose(__VA_ARGS__)
#define LOG_NOTICE(...) Log.notice(__VA_ARGS__)
#define LOG_WARNING(...) Log.warning(__VA_ARGS__)
#define LOG_ERROR(...) Log.error(__VA_ARGS__)
#define LOG_FATAL(...) Log.fatal(__VA_ARGS__)

#endif // LOGGER_H
