// Unit tests for the persisted records.
//
// SettingsStore itself needs LittleFS, so what is tested here is the part that
// decides whether a record read back from flash may be trusted: the checksum,
// the version gate, and the fact that padding does not make the checksum
// unreproducible.

#include <unity.h>
#include <string.h>

#include "DryerSettings.h"

void setUp(void) {}
void tearDown(void) {}

void test_defaults_are_sealed_and_valid(void)
{
  DryerSettings settings;
  settings.Reset();

  TEST_ASSERT_TRUE(IsRecordValid(settings, static_cast<uint16_t>(SETTINGS_VERSION)));
  TEST_ASSERT_EQUAL_FLOAT(TEMPERATURE_TARGET, settings.target_temperature);
}

void test_v6_defaults(void)
{
  // The four v6 fields, each factory value chosen to leave a dryer that has
  // never been configured behaving exactly as it did before they existed.
  DryerSettings settings;
  settings.Reset();

  TEST_ASSERT_EQUAL_UINT8(HEAT_SOURCE_ELECTRIC, settings.heat_source);
  TEST_ASSERT_EQUAL_UINT8(DRYER_PROGRAM_DRYING, settings.program);
  TEST_ASSERT_EQUAL_FLOAT(DEHUM_EXTRACTION_THRESHOLD_DEFAULT,
                          settings.dehum_extraction_threshold);

  // The connector costs nothing unused; the radio is a deliberate act.
  TEST_ASSERT_TRUE(settings.telemetry_rs485);
  TEST_ASSERT_FALSE(settings.telemetry_wifi);
}

void test_checksum_is_reproducible_across_instances(void)
{
  // Reset() zeroes the whole record before assigning, so padding bytes are
  // deterministic and two independently built records checksum identically.
  DryerSettings a;
  DryerSettings b;
  a.Reset();
  b.Reset();

  TEST_ASSERT_EQUAL_UINT16(ComputeRecordChecksum(a), ComputeRecordChecksum(b));
  TEST_ASSERT_EQUAL_INT(0, memcmp(&a, &b, sizeof(DryerSettings)));
}

void test_any_field_change_invalidates_the_record(void)
{
  DryerSettings settings;
  settings.Reset();
  TEST_ASSERT_TRUE(IsRecordValid(settings, static_cast<uint16_t>(SETTINGS_VERSION)));

  settings.target_temperature += 1.0f;
  TEST_ASSERT_FALSE(IsRecordValid(settings, static_cast<uint16_t>(SETTINGS_VERSION)));

  SealRecord(settings);
  TEST_ASSERT_TRUE(IsRecordValid(settings, static_cast<uint16_t>(SETTINGS_VERSION)));
}

void test_record_from_an_older_version_is_rejected(void)
{
  DryerSettings settings;
  settings.Reset();
  settings.version = SETTINGS_VERSION - 1;
  SealRecord(settings);  // checksum is fine, the layout is not

  TEST_ASSERT_FALSE(IsRecordValid(settings, static_cast<uint16_t>(SETTINGS_VERSION)));
}

void test_corrupt_record_is_rejected(void)
{
  DryerSettings settings;
  settings.Reset();

  uint8_t *bytes = reinterpret_cast<uint8_t *>(&settings);
  bytes[sizeof(DryerSettings) / 2] ^= 0xFF;

  TEST_ASSERT_FALSE(IsRecordValid(settings, static_cast<uint16_t>(SETTINGS_VERSION)));
}

void test_session_snapshot_round_trip(void)
{
  SessionSnapshot session;
  session.Reset();

  session.running         = true;
  session.phase           = 2;
  session.phase_elapsed_s = 450;
  session.total_elapsed_s = 7200;
  SealRecord(session);

  // Simulate a write to flash and a read back.
  uint8_t buffer[sizeof(SessionSnapshot)];
  memcpy(buffer, &session, sizeof(buffer));

  SessionSnapshot restored;
  memcpy(&restored, buffer, sizeof(buffer));

  TEST_ASSERT_TRUE(IsRecordValid(restored, static_cast<uint16_t>(SESSION_VERSION)));
  TEST_ASSERT_TRUE(restored.running);
  TEST_ASSERT_EQUAL_UINT8(2, restored.phase);
  TEST_ASSERT_EQUAL_UINT32(450, restored.phase_elapsed_s);
  TEST_ASSERT_EQUAL_UINT32(7200, restored.total_elapsed_s);
}

void test_cleared_session_is_valid_and_not_running(void)
{
  SessionSnapshot session;
  session.Reset();

  TEST_ASSERT_TRUE(IsRecordValid(session, static_cast<uint16_t>(SESSION_VERSION)));
  TEST_ASSERT_FALSE(session.running);
  TEST_ASSERT_EQUAL_UINT32(0, session.total_elapsed_s);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_defaults_are_sealed_and_valid);
  RUN_TEST(test_v6_defaults);
  RUN_TEST(test_checksum_is_reproducible_across_instances);
  RUN_TEST(test_any_field_change_invalidates_the_record);
  RUN_TEST(test_record_from_an_older_version_is_rejected);
  RUN_TEST(test_corrupt_record_is_rejected);
  RUN_TEST(test_session_snapshot_round_trip);
  RUN_TEST(test_cleared_session_is_valid_and_not_running);
  return UNITY_END();
}
