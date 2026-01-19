#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"
#include "Dryer.h"
#include "TimeManager.h"

// Use SDLib namespace to avoid ambiguity with fs::File
using SDFile = SDLib::File;

/**
 * @brief Data logger for recording drying session data to SD card
 *
 * This class handles:
 * - SD card initialization
 * - Creating CSV files with timestamps
 * - Logging session data at configurable intervals
 * - Writing headers and data rows
 */
class DataLogger
{
public:
  /**
   * @brief Construct a new Data Logger object
   * @param dryer Pointer to the Dryer instance
   * @param time_manager Pointer to the TimeManager instance
   */
  DataLogger(Dryer *dryer, TimeManager *time_manager);

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

private:
  Dryer *dryer_;
  TimeManager *time_manager_;

  bool sd_initialized_;
  bool logging_active_;
  String current_filename_;
  unsigned long last_log_time_;
  unsigned long log_interval_;

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

#endif // DATA_LOGGER_H
