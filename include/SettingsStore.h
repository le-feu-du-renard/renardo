#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include <Arduino.h>
#include "DryerSettings.h"

// Persistence on the RP2040's internal flash through LittleFS.
//
// Replaces the v3 SD card, which carried both /state.bin and the CSV session
// logs. Logging moves to the server over LoRa, so only two small records are
// left. The partition size is set by board_build.filesystem_size in
// platformio.ini.
//
// Each record is written to a temporary file and then renamed over the target,
// so a power cut during a write leaves the previous copy intact rather than a
// truncated one. A record that fails its version or checksum test is discarded.
class SettingsStore
{
public:
  SettingsStore();

  // Mounts the filesystem, formatting it on first use.
  bool Begin();
  bool IsReady() const { return ready_; }

  // Fall back to factory values when the record is missing or corrupt; both
  // return false in that case, with `out` left holding usable defaults.
  bool LoadSettings(DryerSettings &out);
  bool SaveSettings(const DryerSettings &settings);

  bool LoadSession(SessionSnapshot &out);
  bool SaveSession(const SessionSnapshot &session);
  bool ClearSession();

private:
  bool ready_;

  bool ReadRecord(const char *path, void *buffer, size_t length);
  bool WriteRecord(const char *path, const void *buffer, size_t length);

  static constexpr const char *kSettingsPath = "/settings.bin";
  static constexpr const char *kSessionPath  = "/session.bin";
  static constexpr const char *kTempPath     = "/tmp.bin";
};

#endif // SETTINGS_STORE_H
