#include <unity.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * Unit tests for SettingsManager checksum logic
 * Tests data integrity verification
 */

// Simplified test structure matching PersistentSettings layout
struct __attribute__((packed)) TestSettings
{
  uint16_t version;
  bool dryer_running;
  float temperature_target;
  float temperature_deadband;
  uint32_t total_duty_time_s;
  float recycling_rate;
  uint16_t checksum;

  TestSettings()
      : version(1),
        dryer_running(false),
        temperature_target(40.0f),
        temperature_deadband(0.5f),
        total_duty_time_s(0),
        recycling_rate(50.0f),
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
  settings1.version = 1;
  settings1.dryer_running = true;
  settings1.temperature_target = 42.5f;

  TestSettings settings2;
  settings2.version = 1;
  settings2.dryer_running = true;
  settings2.temperature_target = 42.5f;

  // Same data should produce same checksum
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_EQUAL_UINT16(checksum1, checksum2);
}

void test_checksum_changes_with_data(void)
{
  TestSettings settings1;
  settings1.temperature_target = 40.0f;

  TestSettings settings2;
  settings2.temperature_target = 41.0f;

  // Different data should produce different checksums
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

void test_checksum_not_affected_by_checksum_field(void)
{
  TestSettings settings1;
  settings1.temperature_target = 40.0f;
  settings1.checksum = 0;

  TestSettings settings2;
  settings2.temperature_target = 40.0f;
  settings2.checksum = 12345; // Different checksum field value

  // Checksum field itself should not affect calculation
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
  settings1.dryer_running = false;

  TestSettings settings2;
  settings2.dryer_running = true;

  // Different bool value should produce different checksum
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

void test_checksum_sensitive_to_uint32_field(void)
{
  TestSettings settings1;
  settings1.total_duty_time_s = 1000;

  TestSettings settings2;
  settings2.total_duty_time_s = 2000;

  // Different uint32 value should produce different checksum
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

void test_checksum_sensitive_to_float_field(void)
{
  TestSettings settings1;
  settings1.recycling_rate = 50.0f;

  TestSettings settings2;
  settings2.recycling_rate = 75.0f;

  // Different float value should produce different checksum
  uint16_t checksum1 = CalculateChecksum(settings1);
  uint16_t checksum2 = CalculateChecksum(settings2);

  TEST_ASSERT_NOT_EQUAL(checksum1, checksum2);
}

// ===== Checksum Verification Tests =====

void test_verify_checksum_valid(void)
{
  TestSettings settings;
  settings.version = 1;
  settings.temperature_target = 42.0f;
  settings.checksum = CalculateChecksum(settings);

  TEST_ASSERT_TRUE(VerifyChecksum(settings));
}

void test_verify_checksum_invalid(void)
{
  TestSettings settings;
  settings.version = 1;
  settings.temperature_target = 42.0f;
  settings.checksum = CalculateChecksum(settings);

  // Corrupt the data
  settings.temperature_target = 43.0f;

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
  settings.temperature_target = 40.0f;
  settings.checksum = 12345; // Arbitrary wrong value

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
  original.version = 1;
  original.dryer_running = true;
  original.temperature_target = 42.5f;
  original.temperature_deadband = 1.0f;
  original.total_duty_time_s = 3600;
  original.recycling_rate = 75.0f;

  // Calculate and store checksum
  original.checksum = CalculateChecksum(original);

  // Verify checksum
  TEST_ASSERT_TRUE(VerifyChecksum(original));

  // Simulate serialization/deserialization
  uint8_t buffer[sizeof(TestSettings)];
  memcpy(buffer, &original, sizeof(TestSettings));

  TestSettings restored;
  memcpy(&restored, buffer, sizeof(TestSettings));

  // Verify restored data
  TEST_ASSERT_TRUE(VerifyChecksum(restored));
  TEST_ASSERT_EQUAL_UINT16(original.version, restored.version);
  TEST_ASSERT_EQUAL(original.dryer_running, restored.dryer_running);
  TEST_ASSERT_EQUAL_FLOAT(original.temperature_target, restored.temperature_target);
  TEST_ASSERT_EQUAL_FLOAT(original.temperature_deadband, restored.temperature_deadband);
  TEST_ASSERT_EQUAL_UINT32(original.total_duty_time_s, restored.total_duty_time_s);
  TEST_ASSERT_EQUAL_FLOAT(original.recycling_rate, restored.recycling_rate);
  TEST_ASSERT_EQUAL_UINT16(original.checksum, restored.checksum);
}

void test_checksum_detects_single_bit_corruption(void)
{
  TestSettings settings;
  settings.version = 1;
  settings.temperature_target = 40.0f;
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
  settings.version = 1;
  settings.dryer_running = false;
  settings.temperature_target = 40.0f;
  settings.recycling_rate = 50.0f;
  settings.checksum = CalculateChecksum(settings);

  TEST_ASSERT_TRUE(VerifyChecksum(settings));

  // Change multiple fields
  settings.dryer_running = true;
  settings.temperature_target = 45.0f;
  settings.recycling_rate = 75.0f;

  // Should detect changes
  TEST_ASSERT_FALSE(VerifyChecksum(settings));

  // Recalculate checksum
  settings.checksum = CalculateChecksum(settings);

  // Should now verify
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
  RUN_TEST(test_checksum_sensitive_to_uint32_field);
  RUN_TEST(test_checksum_sensitive_to_float_field);

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
