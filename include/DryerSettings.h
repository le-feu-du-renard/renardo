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
// v2 split the single damper calibration into one pair per register, so a v1
// record's stored values no longer line up — it is discarded and the two
// registers start from the divider's theoretical end stops, awaiting calibration.
// v3 turned each pair from named ends (closed, open) into ordered marks (min,
// max) alongside an explicit signal direction, and added the register count and
// the two actuator direction flags. A v2 pair carries its direction in its own
// order, so it cannot be read as a v3 one: the record is discarded and the
// calibration has to be captured again.
// v4 dropped the LoRa telemetry interval along with the radio. The field sat
// immediately before the checksum, so the record is shorter and every stored one
// is discarded: the factory values come back, **and the register calibration
// with them** — it has to be captured again from the menu after this upgrade.
// v5 dropped the four hydraulic regulation knobs, which stopped meaning anything
// once the remote module took back its own start and regulation, and added the
// air-renewal window in their place. Same consequence as v4: every stored record
// is discarded and the register calibration has to be captured again.
#define SETTINGS_VERSION 5
#define SESSION_VERSION 1

// Everything the menu can change.
struct DryerSettings
{
  uint16_t version;

  // Setpoints
  float target_temperature;
  float target_humidity;
  float water_target;        // fixed setpoint pushed to the hydraulic module

  // Sources. `hydraulic_enabled` is a run permission, not a regulation input:
  // the module handles its own start and its own water loop, so there is
  // nothing here to tune it with.
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

  // Regulation — the electric is the only regulated source
  float band_electric;
  float horizon_electric;
  float electric_t_on_min;
  float electric_t_off_min;
  float air_renewal_window;
  float safety_max;

  // Air registers.
  //
  // `damper_count` describes the machine: how many registers it has, and so
  // whether the second feedback is sampled and whether the airflow interlock
  // exists at all. The direction flags describe the wiring: one for the feedback
  // signal, shared because every register uses the same actuator model, and one
  // per register for the actuator's own mechanical direction switch.
  //
  // The calibration is two ordered marks per register, min then max, with
  // `damper_feedback_low_is_open` saying which end is the open one — the
  // direction is deliberately not folded back into the order of the pair.
  uint8_t  damper_count;
  bool     damper_feedback_low_is_open;
  bool     damper_extraction_inverted;
  bool     damper_recycling_inverted;
  uint16_t extraction_raw_min;
  uint16_t extraction_raw_max;
  uint16_t recycling_raw_min;
  uint16_t recycling_raw_max;

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
