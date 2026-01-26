#ifndef SESSION_MONITOR_H
#define SESSION_MONITOR_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"
#include "Dryer.h"
#include "TimeManager.h"

// Use SDLib namespace to avoid ambiguity with fs::File
using SDFile = SDLib::File;

/**
 * @brief Session monitor for recording drying session data to SD card
 *
 * This class handles:
 * - SD card initialization
 * - Creating CSV files with batch numbers in organized directory structure
 * - Logging session data at configurable intervals
 * - Writing headers and data rows
 */
class SessionMonitor
{
public:
  /**
   * @brief Construct a new Session Monitor object
   * @param dryer Pointer to the Dryer instance
   * @param time_manager Pointer to the TimeManager instance
   */
  SessionMonitor(Dryer *dryer, TimeManager *time_manager);

  /**
   * @brief Initialize the SD card and SPI interface
   * @return true if initialization successful, false otherwise
   */
  bool Begin();

  /**
   * @brief Start logging a new session
   * Creates a new CSV file with timestamp
   * @return true if file created successfully, false otherwise
   */
  bool StartSession();

  /**
   * @brief Stop logging the current session
   */
  void StopSession();

  /**
   * @brief Update function to call in main loop
   * Logs data at the configured interval when session is active
   */
  void Update();

  /**
   * @brief Check if logger is currently logging
   * @return true if logging active, false otherwise
   */
  bool IsLogging() const { return logging_active_; }

  /**
   * @brief Check if SD card is initialized and ready
   * @return true if ready, false otherwise
   */
  bool IsReady() const { return sd_initialized_; }

  /**
   * @brief Get the current log filename
   * @return String containing the current log file path
   */
  String GetCurrentFilename() const { return current_filename_; }

private:
  Dryer *dryer_;
  TimeManager *time_manager_;

  bool sd_initialized_;
  bool logging_active_;
  String current_filename_;
  unsigned long last_log_time_;
  unsigned long log_interval_;

  // Failure tracking
  uint8_t consecutive_failures_;
  static constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 5;

  /**
   * @brief Write CSV header to the file
   * @param file File object to write to
   */
  void WriteHeader(SDFile &file);

  /**
   * @brief Write a data row to the file
   * @param file File object to write to
   */
  void WriteDataRow(SDFile &file);

  /**
   * @brief Get current session data as CSV row
   * @return String containing CSV row data
   */
  String GetDataRowString();
};

#endif // SESSION_MONITOR_H
