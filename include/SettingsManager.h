#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include "TemperatureManager.h"

/**
 * Structure to save all persistent settings (v4 - SessionManager format)
 */
struct PersistentSettings {
  // Version identifier for settings structure
  uint16_t version;

  // Session state
  bool session_running;
  uint8_t cycle_index;
  uint8_t phase_index_in_cycle;
  uint32_t phase_elapsed_time_s;
  uint32_t cycle_elapsed_time_s;

  // Temperature parameters
  TemperatureParams temperature_params;

  // Duty time tracking (total accumulated time in seconds)
  uint32_t total_duty_time_s;

  // Air recycling rate (0-100%)
  float recycling_rate;

  // Checksum for data integrity
  uint16_t checksum;

  PersistentSettings()
    : version(4),
      session_running(false),
      cycle_index(0),
      phase_index_in_cycle(0),
      phase_elapsed_time_s(0),
      cycle_elapsed_time_s(0),
      temperature_params(),
      total_duty_time_s(0),
      recycling_rate(50.0f),
      checksum(0) {}
};

/**
 * Manages EEPROM persistence for dryer settings
 */
class SettingsManager {
 public:
  SettingsManager();

  void Begin();

  // Save session state to EEPROM (new format)
  void SaveSessionState(bool session_running,
                        uint8_t cycle_index,
                        uint8_t phase_index_in_cycle,
                        uint32_t phase_elapsed_time_s,
                        uint32_t cycle_elapsed_time_s,
                        const TemperatureParams& temperature_params,
                        uint32_t total_duty_time_s,
                        float recycling_rate);

  // Load session state from EEPROM (new format)
  bool LoadSessionState(bool& session_running,
                        uint8_t& cycle_index,
                        uint8_t& phase_index_in_cycle,
                        uint32_t& phase_elapsed_time_s,
                        uint32_t& cycle_elapsed_time_s,
                        TemperatureParams& temperature_params,
                        uint32_t& total_duty_time_s,
                        float& recycling_rate);

  // Quick save of running state only
  void SaveDryerState(bool running);

  // Reset to factory defaults
  void ResetToDefaults();

 private:
  static constexpr uint16_t kSettingsVersion = 4;
  static constexpr size_t kEepromSize = sizeof(PersistentSettings);
  static constexpr uint16_t kEepromAddress = 0;

  PersistentSettings settings_;

  // Calculate checksum for data integrity
  uint16_t CalculateChecksum(const PersistentSettings& settings) const;

  // Verify checksum
  bool VerifyChecksum(const PersistentSettings& settings) const;

  // Write settings to EEPROM
  void WriteToEeprom();

  // Read settings from EEPROM
  bool ReadFromEeprom();
};

#endif  // SETTINGS_MANAGER_H
