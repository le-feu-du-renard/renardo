#include "SettingsManager.h"

SettingsManager::SettingsManager()
    : settings_()
{
}

void SettingsManager::Begin()
{
  EEPROM.begin(kEepromSize);
  Serial.println("SettingsManager initialized");
  Serial.print("EEPROM size: ");
  Serial.print(kEepromSize);
  Serial.println(" bytes");
}

bool SettingsManager::LoadSessionState(bool &session_running,
                                        uint8_t &cycle_index,
                                        uint8_t &phase_index_in_cycle,
                                        uint32_t &phase_elapsed_time_s,
                                        uint32_t &cycle_elapsed_time_s,
                                        TemperatureParams &temperature_params,
                                        uint32_t &total_duty_time_s,
                                        float &recycling_rate)
{
  if (!ReadFromEeprom())
  {
    Serial.println("Failed to load settings from EEPROM (checksum mismatch or invalid version)");
    Serial.println("Using default values");
    return false;
  }

  session_running = settings_.session_running;
  cycle_index = settings_.cycle_index;
  phase_index_in_cycle = settings_.phase_index_in_cycle;
  phase_elapsed_time_s = settings_.phase_elapsed_time_s;
  cycle_elapsed_time_s = settings_.cycle_elapsed_time_s;
  temperature_params = settings_.temperature_params;
  total_duty_time_s = settings_.total_duty_time_s;
  recycling_rate = settings_.recycling_rate;

  Serial.println("Settings loaded from EEPROM:");
  Serial.print("  Session running: ");
  Serial.println(session_running ? "true" : "false");
  Serial.print("  Cycle index: ");
  Serial.println(cycle_index);
  Serial.print("  Phase index: ");
  Serial.println(phase_index_in_cycle);
  Serial.print("  Target temperature: ");
  Serial.println(temperature_params.temperature_target);
  Serial.print("  Total duty time: ");
  Serial.print(total_duty_time_s);
  Serial.println(" seconds");
  Serial.print("  Recycling rate: ");
  Serial.print(recycling_rate);
  Serial.println("%");

  return true;
}

void SettingsManager::SaveSessionState(bool session_running,
                                        uint8_t cycle_index,
                                        uint8_t phase_index_in_cycle,
                                        uint32_t phase_elapsed_time_s,
                                        uint32_t cycle_elapsed_time_s,
                                        const TemperatureParams &temperature_params,
                                        uint32_t total_duty_time_s,
                                        float recycling_rate)
{
  settings_.version = kSettingsVersion;
  settings_.session_running = session_running;
  settings_.cycle_index = cycle_index;
  settings_.phase_index_in_cycle = phase_index_in_cycle;
  settings_.phase_elapsed_time_s = phase_elapsed_time_s;
  settings_.cycle_elapsed_time_s = cycle_elapsed_time_s;
  settings_.temperature_params = temperature_params;
  settings_.total_duty_time_s = total_duty_time_s;
  settings_.recycling_rate = recycling_rate;

  WriteToEeprom();

  Serial.println("Settings saved to EEPROM");
}

void SettingsManager::SaveDryerState(bool running)
{
  if (ReadFromEeprom())
  {
    settings_.session_running = running;
    WriteToEeprom();
    Serial.print("Dryer state saved: ");
    Serial.println(running ? "running" : "stopped");
  }
  else
  {
    // If we can't read, create new settings with just the running state
    settings_ = PersistentSettings();
    settings_.session_running = running;
    WriteToEeprom();
    Serial.print("Dryer state saved (new): ");
    Serial.println(running ? "running" : "stopped");
  }
}

void SettingsManager::ResetToDefaults()
{
  settings_ = PersistentSettings();
  WriteToEeprom();
  Serial.println("Settings reset to factory defaults");
}

uint16_t SettingsManager::CalculateChecksum(const PersistentSettings &settings) const
{
  uint16_t checksum = 0;
  const uint8_t *data = reinterpret_cast<const uint8_t *>(&settings);
  size_t checksum_offset = offsetof(PersistentSettings, checksum);

  for (size_t i = 0; i < checksum_offset; i++)
  {
    checksum += data[i];
  }

  return checksum;
}

bool SettingsManager::VerifyChecksum(const PersistentSettings &settings) const
{
  uint16_t calculated = CalculateChecksum(settings);
  return calculated == settings.checksum;
}

void SettingsManager::WriteToEeprom()
{
  settings_.checksum = CalculateChecksum(settings_);

  const uint8_t *data = reinterpret_cast<const uint8_t *>(&settings_);
  for (size_t i = 0; i < kEepromSize; i++)
  {
    EEPROM.write(kEepromAddress + i, data[i]);
  }

  Serial.println("EEPROM write complete, committing...");
  Serial.flush(); // Ensure message is sent before potential hang

  unsigned long commit_start = millis();
  bool commit_result = EEPROM.commit();
  unsigned long commit_time = millis() - commit_start;

  if (commit_result)
  {
    Serial.print("EEPROM commit successful (");
    Serial.print(commit_time);
    Serial.println("ms)");
  }
  else
  {
    Serial.println("WARNING: EEPROM commit failed!");
  }

  // If commit takes too long, warn user
  if (commit_time > 500)
  {
    Serial.print("WARNING: EEPROM commit took ");
    Serial.print(commit_time);
    Serial.println("ms (expected < 500ms)");
  }
}

bool SettingsManager::ReadFromEeprom()
{
  uint8_t *data = reinterpret_cast<uint8_t *>(&settings_);
  for (size_t i = 0; i < kEepromSize; i++)
  {
    data[i] = EEPROM.read(kEepromAddress + i);
  }

  if (settings_.version != kSettingsVersion)
  {
    Serial.print("Settings version mismatch: expected ");
    Serial.print(kSettingsVersion);
    Serial.print(", got ");
    Serial.println(settings_.version);
    return false;
  }

  if (!VerifyChecksum(settings_))
  {
    Serial.println("Settings checksum verification failed");
    return false;
  }

  return true;
}
