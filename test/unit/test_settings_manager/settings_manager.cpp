#include <unity.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * Unit tests for SettingsManager checksum logic
 * Tests data integrity verification
 */

// Mirrors the actual PersistentState struct from PersistentStateManager.h.
// DryerPhase enum (uint8_t) replaced by uint8_t to avoid Arduino includes.
struct __attribute__((packed)) TestSettings
{
  uint16_t version;
  bool     session_running;
  uint8_t  phase;           // DryerPhase (kStop=0, kInit=1, kBrassage=2, kExtraction=3)
  uint32_t phase_elapsed_s;
  uint32_t total_elapsed_s;
  uint16_t checksum;

  TestSettings()
      : version(9),
        session_running(false),
        phase(0),
        phase_elapsed_s(0),
        total_elapsed_s(0),
        checksum(0) {}
};

// Replicate CalculateChecksum logic
uint16_t CalculateChecksum(const TestSettings &settings)
{
  uint16_t checksum = 0;
  const uint8_t *data = reinterpret_cast<const uint8_t *>(&settings);
  size_t checksum_offset = offsetof(TestSettings, checksum);

  for (size_t i = 0; i < checksum_offset; i++)
  {
    checksum += data[i];
  }

  return checksum;
}

// Replicate VerifyChecksum logic
bool VerifyChecksum(const TestSettings &settings)
{
  uint16_t calculated = CalculateChecksum(settings);
  return calculated == settings.checksum;
}

void setUp(void)
{
  // Called before each test
}

void tearDown(void)
{
  // Called after each test
}

// ===== Checksum Calculation Tests =====

void test_checksum_calculation_default_settings(void)
{
  TestSettings settings;
  uint16_t checksum = CalculateChecksum(settings);

  // Checksum should be non-zero for default settings
  TEST_ASSERT_GREATER_THAN_UINT16(0, checksum);
}

void test_checksum_deterministic(void)
{
  TestSettings settings1;
  settings1.version = 9;
  settings1.session_running = true;
  settings1.phase = 2;
  settings1.total_elapsed_s = 3600;

  TestSettings settings2;
  settings2.version = 9;
  settings2.session_running = true;
  settings2.phase = 2;
  settings2.total_elapsed_s = 3600;

  // Same data should produce same checksum
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_EQUAL_UINT16(checksum1, checksum2);
}

void test_checksum_changes_with_data(void)
{
  TestSettings settings1;
  settings1.total_elapsed_s = 1000;

  TestSettings settings2;
  settings2.total_elapsed_s = 2000;

  // Different data should produce different checksums
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

void test_checksum_not_affected_by_checksum_field(void)
{
  TestSettings settings1;
  settings1.total_elapsed_s = 3600;
  settings1.checksum = 0;

  TestSettings settings2;
  settings2.total_elapsed_s = 3600;
  settings2.checksum = 12345;  // Different checksum field value, same payload

  // Checksum field itself must not affect calculation
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_EQUAL_UINT16(checksum1, checksum2);
}

void test_checksum_sensitive_to_version(void)
{
  TestSettings settings1;
  settings1.version = 1;

  TestSettings settings2;
  settings2.version = 2;

  // Different version should produce different checksum
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

void test_checksum_sensitive_to_bool_field(void)
{
  TestSettings settings1;
  settings1.session_running = false;

  TestSettings settings2;
  settings2.session_running = true;

  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

void test_checksum_sensitive_to_phase_field(void)
{
  TestSettings settings1;
  settings1.phase = 1;  // kInit

  TestSettings settings2;
  settings2.phase = 2;  // kBrassage

  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

void test_checksum_sensitive_to_phase_elapsed_field(void)
{
  TestSettings settings1;
  settings1.phase_elapsed_s = 100;

  TestSettings settings2;
  settings2.phase_elapsed_s = 200;

  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

void test_checksum_sensitive_to_total_elapsed_field(void)
{
  TestSettings settings1;
  settings1.total_elapsed_s = 1000;

  TestSettings settings2;
  settings2.total_elapsed_s = 2000;

  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

// ===== Checksum Verification Tests =====

void test_verify_checksum_valid(void)
{
  TestSettings settings;
  settings.version = 9;
  settings.session_running = true;
  settings.phase = 2;
  settings.phase_elapsed_s = 450;
  settings.total_elapsed_s = 7200;
  settings.checksum = CalculateChecksum(settings);

  TEST_ASSERT_TRUE(VerifyChecksum(settings));
}

void test_verify_checksum_invalid(void)
{
  TestSettings settings;
  settings.version = 9;
  settings.phase = 2;
  settings.total_elapsed_s = 7200;
  settings.checksum = CalculateChecksum(settings);

  // Corrupt the data after checksum was computed
  settings.total_elapsed_s = 7201;

  TEST_ASSERT_FALSE(VerifyChecksum(settings));
}

void test_verify_checksum_zero(void)
{
  TestSettings settings;
  settings.checksum = 0;

  // Zero checksum should fail unless data actually checksums to zero
  bool valid = VerifyChecksum(settings);
  uint16_t actual_checksum = CalculateChecksum(settings);

  if (actual_checksum == 0)
  {
    TEST_ASSERT_TRUE(valid);
  }
  else
  {
    TEST_ASSERT_FALSE(valid);
  }
}

void test_verify_checksum_wrong_value(void)
{
  TestSettings settings;
  settings.total_elapsed_s = 3600;
  settings.checksum = 12345;  // Arbitrary wrong value

  // Should fail unless by incredible coincidence this matches
  bool valid = VerifyChecksum(settings);
  uint16_t actual_checksum = CalculateChecksum(settings);

  if (actual_checksum == 12345)
  {
    TEST_ASSERT_TRUE(valid);
  }
  else
  {
    TEST_ASSERT_FALSE(valid);
  }
}

// ===== Round-Trip Tests =====

void test_checksum_roundtrip(void)
{
  TestSettings original;
  original.version = 9;
  original.session_running = true;
  original.phase = 3;              // kExtraction
  original.phase_elapsed_s = 120;
  original.total_elapsed_s = 5400;
  original.checksum = CalculateChecksum(original);

  TEST_ASSERT_TRUE(VerifyChecksum(original));

  // Simulate serialization/deserialization (e.g. EEPROM write/read)
  uint8_t buffer[sizeof(TestSettings)];
  memcpy(buffer, &original, sizeof(TestSettings));

  TestSettings restored;
  memcpy(&restored, buffer, sizeof(TestSettings));

  TEST_ASSERT_TRUE(VerifyChecksum(restored));
  TEST_ASSERT_EQUAL_UINT16(original.version, restored.version);
  TEST_ASSERT_EQUAL(original.session_running, restored.session_running);
  TEST_ASSERT_EQUAL_UINT8(original.phase, restored.phase);
  TEST_ASSERT_EQUAL_UINT32(original.phase_elapsed_s, restored.phase_elapsed_s);
  TEST_ASSERT_EQUAL_UINT32(original.total_elapsed_s, restored.total_elapsed_s);
  TEST_ASSERT_EQUAL_UINT16(original.checksum, restored.checksum);
}

void test_checksum_detects_single_bit_corruption(void)
{
  TestSettings settings;
  settings.version = 9;
  settings.total_elapsed_s = 7200;
  settings.checksum = CalculateChecksum(settings);

  TEST_ASSERT_TRUE(VerifyChecksum(settings));

  // Corrupt a single byte
  uint8_t *data = reinterpret_cast<uint8_t *>(&settings);
  data[2] ^= 0x01; // Flip one bit in some field (not checksum)

  // Should detect corruption
  TEST_ASSERT_FALSE(VerifyChecksum(settings));
}

void test_checksum_multiple_field_changes(void)
{
  TestSettings settings;
  settings.version = 9;
  settings.session_running = false;
  settings.phase = 1;
  settings.phase_elapsed_s = 300;
  settings.total_elapsed_s = 300;
  settings.checksum = CalculateChecksum(settings);

  TEST_ASSERT_TRUE(VerifyChecksum(settings));

  // Change multiple fields
  settings.session_running = true;
  settings.phase = 2;
  settings.total_elapsed_s = 1500;

  TEST_ASSERT_FALSE(VerifyChecksum(settings));

  // Recalculate → should verify again
  settings.checksum = CalculateChecksum(settings);
  TEST_ASSERT_TRUE(VerifyChecksum(settings));
}

// ===== Edge Cases =====

void test_checksum_all_zeros(void)
{
  TestSettings settings;
  memset(&settings, 0, sizeof(TestSettings));

  uint16_t checksum = CalculateChecksum(settings);
  settings.checksum = checksum;

  TEST_ASSERT_TRUE(VerifyChecksum(settings));
}

void test_checksum_all_ones(void)
{
  TestSettings settings;
  memset(&settings, 0xFF, sizeof(TestSettings));

  uint16_t checksum = CalculateChecksum(settings);
  settings.checksum = checksum;

  TEST_ASSERT_TRUE(VerifyChecksum(settings));
}

void test_checksum_offset_calculation(void)
{
  // Verify that checksum field is at the expected offset
  size_t checksum_offset = offsetof(TestSettings, checksum);
  size_t total_size = sizeof(TestSettings);

  // Checksum should be the last field
  TEST_ASSERT_EQUAL_size_t(total_size - sizeof(uint16_t), checksum_offset);
}

// ===== Test Runner =====

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  // Checksum calculation tests
  RUN_TEST(test_checksum_calculation_default_settings);
  RUN_TEST(test_checksum_deterministic);
  RUN_TEST(test_checksum_changes_with_data);
  RUN_TEST(test_checksum_not_affected_by_checksum_field);
  RUN_TEST(test_checksum_sensitive_to_version);
  RUN_TEST(test_checksum_sensitive_to_bool_field);
  RUN_TEST(test_checksum_sensitive_to_phase_field);
  RUN_TEST(test_checksum_sensitive_to_phase_elapsed_field);
  RUN_TEST(test_checksum_sensitive_to_total_elapsed_field);

  // Checksum verification tests
  RUN_TEST(test_verify_checksum_valid);
  RUN_TEST(test_verify_checksum_invalid);
  RUN_TEST(test_verify_checksum_zero);
  RUN_TEST(test_verify_checksum_wrong_value);

  // Round-trip tests
  RUN_TEST(test_checksum_roundtrip);
  RUN_TEST(test_checksum_detects_single_bit_corruption);
  RUN_TEST(test_checksum_multiple_field_changes);

  // Edge cases
  RUN_TEST(test_checksum_all_zeros);
  RUN_TEST(test_checksum_all_ones);
  RUN_TEST(test_checksum_offset_calculation);

  return UNITY_END();
}
