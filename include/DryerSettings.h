#ifndef DRYER_SETTINGS_H
#define DRYER_SETTINGS_H

#include <Arduino.h>
#include <stddef.h>
#include <string.h>
#include "config.h"

// Persisted state, split into two independent records.
//
// Configuration and session progress are saved to separate files: a settings
// write must never be able to corrupt a running session, and the session record
// is rewritten far more often than the configuration.
//
// Neither struct is packed. Padding bytes are zeroed by Reset() before any
// field is assigned, so the checksum over the raw bytes stays reproducible
// without forcing unaligned float access on the Cortex-M0+.

// Bump when the layout changes; a record with a different version is discarded
// and the defaults are used instead.
#define SETTINGS_VERSION 1
#define SESSION_VERSION 1

// Everything the menu can change.
struct DryerSettings
{
  uint16_t version;

  // Setpoints
  float target_temperature;
  float target_humidity;
  float water_target;        // fixed setpoint pushed to the hydraulic module

  // Sources
  bool hydraulic_enabled;
  bool electric_enabled;

  // ECO mode — only reachable when an RTC is present
  bool    eco_enabled;
  uint8_t eco_start_hour;
  uint8_t eco_end_hour;
  float   eco_target_percentage;

  // Phase durations (seconds)
  uint32_t init_phase_duration;
  uint32_t brassage_phase_duration;
  uint32_t extraction_phase_duration;
  uint32_t extraction_damper_open_duration;

  // Regulation
  float band_hydraulic;
  float band_electric;
  float horizon_hydraulic;
  float horizon_electric;
  float hydraulic_t_on_min;
  float hydraulic_t_off_min;
  float electric_t_on_min;
  float electric_t_off_min;
  float safety_max;

  // Air damper feedback calibration
  uint16_t damper_raw_closed;
  uint16_t damper_raw_open;

  // LoRa
  uint32_t lora_telemetry_interval_ms;

  uint16_t checksum;

  // Zero the whole record — padding included — then apply the factory values.
  void Reset();
};

// Session progress, so a reboot mid-cycle resumes where it left off.
struct SessionSnapshot
{
  uint16_t version;
  bool     running;
  uint8_t  phase;
  uint32_t phase_elapsed_s;
  uint32_t total_elapsed_s;
  uint16_t checksum;

  void Reset();
};

// 16-bit sum over every byte preceding the checksum field.
//
// The range is taken with offsetof rather than sizeof(T) - sizeof(checksum):
// alignment puts trailing padding *after* the checksum, so subtracting from the
// total size would fold the checksum's own bytes back into the sum and no
// record would ever validate.
template <typename T>
uint16_t ComputeRecordChecksum(const T &record)
{
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  size_t length = offsetof(T, checksum);
  uint16_t sum = 0;
  for (size_t i = 0; i < length; i++)
  {
    sum = static_cast<uint16_t>(sum + bytes[i]);
  }
  return sum;
}

template <typename T>
void SealRecord(T &record)
{
  record.checksum = ComputeRecordChecksum(record);
}

template <typename T>
bool IsRecordValid(const T &record, uint16_t expected_version)
{
  return record.version == expected_version &&
         record.checksum == ComputeRecordChecksum(record);
}

#endif // DRYER_SETTINGS_H
