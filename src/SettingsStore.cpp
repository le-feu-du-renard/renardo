#include <LittleFS.h>

#include "SettingsStore.h"
#include "Logger.h"

SettingsStore::SettingsStore() : ready_(false) {}

bool SettingsStore::Begin()
{
  if (!LittleFS.begin())
  {
    // First boot on a blank partition: format once, then retry.
    Logger::Warning("SettingsStore: mount failed, formatting");
    if (!LittleFS.format() || !LittleFS.begin())
    {
      Logger::Error("SettingsStore: filesystem unavailable — settings will not persist");
      ready_ = false;
      return false;
    }
  }

  ready_ = true;
  Logger::Info("SettingsStore: LittleFS mounted");
  return true;
}

bool SettingsStore::ReadRecord(const char *path, void *buffer, size_t length)
{
  if (!ready_)
  {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file)
  {
    return false;
  }

  bool ok = (file.size() == length) && (file.read(static_cast<uint8_t *>(buffer), length) ==
                                        static_cast<int>(length));
  file.close();
  return ok;
}

bool SettingsStore::WriteRecord(const char *path, const void *buffer, size_t length)
{
  if (!ready_)
  {
    return false;
  }

  // Write to a temporary file first: a power cut mid-write then costs the new
  // values rather than the previous ones.
  File file = LittleFS.open(kTempPath, "w");
  if (!file)
  {
    Logger::Error("SettingsStore: cannot open %s for writing", kTempPath);
    return false;
  }

  size_t written = file.write(static_cast<const uint8_t *>(buffer), length);
  file.close();

  if (written != length)
  {
    Logger::Error("SettingsStore: short write to %s (%u/%u)", kTempPath, written, length);
    LittleFS.remove(kTempPath);
    return false;
  }

  LittleFS.remove(path);
  if (!LittleFS.rename(kTempPath, path))
  {
    Logger::Error("SettingsStore: cannot rename %s -> %s", kTempPath, path);
    LittleFS.remove(kTempPath);
    return false;
  }

  return true;
}

bool SettingsStore::LoadSettings(DryerSettings &out)
{
  DryerSettings stored;
  if (!ReadRecord(kSettingsPath, &stored, sizeof(stored)))
  {
    out.Reset();
    Logger::Info("SettingsStore: no settings stored — using defaults");
    return false;
  }

  if (!IsRecordValid(stored, static_cast<uint16_t>(SETTINGS_VERSION)))
  {
    out.Reset();
    Logger::Warning("SettingsStore: settings record rejected (version %d, bad checksum?) — using defaults",
                    stored.version);
    return false;
  }

  out = stored;
  Logger::Info("SettingsStore: settings loaded (target %FC / %F%%RH)",
               out.target_temperature, out.target_humidity);
  return true;
}

bool SettingsStore::SaveSettings(const DryerSettings &settings)
{
  DryerSettings record = settings;
  record.version = SETTINGS_VERSION;
  SealRecord(record);

  if (!WriteRecord(kSettingsPath, &record, sizeof(record)))
  {
    return false;
  }
  Logger::Debug("SettingsStore: settings saved");
  return true;
}

bool SettingsStore::LoadSession(SessionSnapshot &out)
{
  SessionSnapshot stored;
  if (!ReadRecord(kSessionPath, &stored, sizeof(stored)) ||
      !IsRecordValid(stored, static_cast<uint16_t>(SESSION_VERSION)))
  {
    out.Reset();
    return false;
  }

  out = stored;
  return true;
}

bool SettingsStore::SaveSession(const SessionSnapshot &session)
{
  SessionSnapshot record = session;
  record.version = SESSION_VERSION;
  SealRecord(record);
  return WriteRecord(kSessionPath, &record, sizeof(record));
}

bool SettingsStore::ClearSession()
{
  SessionSnapshot empty;
  empty.Reset();
  return SaveSession(empty);
}
