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

bool SettingsManager::LoadSettings(bool &dryer_running,
                                   DryerPhase &current_phase,
                                   uint32_t &phase_elapsed_time_s,
                                   HeatingParams &heating_params,
                                   PhaseParams &phase_params,
                                   uint32_t &total_duty_time_s,
                                   float &recycling_rate)
{
  if (!ReadFromEeprom())
  {
    Serial.println("Failed to load settings from EEPROM (checksum mismatch or invalid version)");
    Serial.println("Using default values");
    return false;
  }

  dryer_running = settings_.dryer_running;
  current_phase = static_cast<DryerPhase>(settings_.current_phase);
  phase_elapsed_time_s = settings_.phase_elapsed_time_s;
  heating_params = settings_.heating_params;
  phase_params = settings_.phase_params;
  total_duty_time_s = settings_.total_duty_time_s;
  recycling_rate = settings_.recycling_rate;

  Serial.println("Settings loaded from EEPROM:");
  Serial.print("  Dryer running: ");
  Serial.println(dryer_running ? "true" : "false");
  Serial.print("  Current phase: ");
  Serial.println(settings_.current_phase);
  Serial.print("  Target temperature: ");
  Serial.println(heating_params.temperature_target);
  Serial.print("  Total duty time: ");
  Serial.print(total_duty_time_s);
  Serial.println(" seconds");
  Serial.print("  Recycling rate: ");
  Serial.print(recycling_rate);
  Serial.println("%");

  return true;
}

void SettingsManager::SaveSettings(bool dryer_running,
                                   DryerPhase current_phase,
                                   uint32_t phase_elapsed_time_s,
                                   const HeatingParams &heating_params,
                                   const PhaseParams &phase_params,
                                   uint32_t total_duty_time_s,
                                   float recycling_rate)
{
  settings_.version = kSettingsVersion;
  settings_.dryer_running = dryer_running;
  settings_.current_phase = static_cast<uint8_t>(current_phase);
  settings_.phase_elapsed_time_s = phase_elapsed_time_s;
  settings_.heating_params = heating_params;
  settings_.phase_params = phase_params;
  settings_.total_duty_time_s = total_duty_time_s;
  settings_.recycling_rate = recycling_rate;

  WriteToEeprom();

  Serial.println("Settings saved to EEPROM");
}

void SettingsManager::SaveDryerState(bool running)
{
  if (ReadFromEeprom())
  {
    settings_.dryer_running = running;
    WriteToEeprom();
    Serial.print("Dryer state saved: ");
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

  EEPROM.commit();
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
