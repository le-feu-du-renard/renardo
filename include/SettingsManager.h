#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include "HeatersManager.h"
#include "PhasesManager.h"

/**
 * Structure to save all persistent settings
 */
struct PersistentSettings {
  // Version identifier for settings structure
  uint16_t version;

  // Dryer state
  bool dryer_running;
  uint8_t current_phase;  // DryerPhase as uint8_t
  uint32_t phase_elapsed_time_s;  // Time elapsed in current phase (seconds)

  // Heating parameters
  HeatingParams heating_params;

  // Phase parameters
  PhaseParams phase_params;

  // Duty time tracking (total accumulated time in seconds)
  uint32_t total_duty_time_s;

  // Air recycling rate (0-100%)
  float recycling_rate;

  // Checksum for data integrity
  uint16_t checksum;

  PersistentSettings()
    : version(3),
      dryer_running(false),
      current_phase(0),  // kStop
      phase_elapsed_time_s(0),
      heating_params(),
      phase_params(),
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

  // Save all settings to EEPROM
  void SaveSettings(bool dryer_running,
                    DryerPhase current_phase,
                    uint32_t phase_elapsed_time_s,
                    const HeatingParams& heating_params,
                    const PhaseParams& phase_params,
                    uint32_t total_duty_time_s,
                    float recycling_rate);

  // Load all settings from EEPROM
  bool LoadSettings(bool& dryer_running,
                    DryerPhase& current_phase,
                    uint32_t& phase_elapsed_time_s,
                    HeatingParams& heating_params,
                    PhaseParams& phase_params,
                    uint32_t& total_duty_time_s,
                    float& recycling_rate);

  // Individual save operations (deprecated - use SaveSettings instead)
  // Only SaveDryerState is kept for immediate state changes in Start/Stop
  void SaveDryerState(bool running);

  // Reset to factory defaults
  void ResetToDefaults();

 private:
  static constexpr uint16_t kSettingsVersion = 3;
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
