#include <unity.h>
#include <cmath>

/**
 * Unit tests for HeatersManager calculation logic
 * Tests the adaptive step calculation and temperature validation
 */

// We need to extract the calculation logic to test it independently
// Since HeatersManager has hardware dependencies, we'll test the core algorithms

// Replicate HeatingParams for testing
struct TestHeatingParams {
  float temperature_target;
  float temperature_deadband;
  float heater_step_min;
  float heater_step_max;
  float heater_full_scale_delta;

  TestHeatingParams()
    : temperature_target(40.0f),
      temperature_deadband(0.5f),
      heater_step_min(1.0f),
      heater_step_max(10.0f),
      heater_full_scale_delta(30.0f) {}
};

// Replicate CalculateStep logic for testing
float CalculateStep(const TestHeatingParams& params, float current_temperature) {
  float diff_abs = fabs(params.temperature_target - current_temperature);

  // No step if within deadband
  if (diff_abs <= params.temperature_deadband) {
    return 0.0f;
  }

  // Calculate scaled difference
  float diff_scaled = (diff_abs - params.temperature_deadband) / params.heater_full_scale_delta;

  // Clamp to 0-1
  float k = diff_scaled;
  if (k < 0.0f) k = 0.0f;
  if (k > 1.0f) k = 1.0f;

  // Calculate adaptive step
  float step = params.heater_step_min + (params.heater_step_max - params.heater_step_min) * k;

  return step;
}

// Replicate temperature validation logic
bool IsTemperatureTooHigh(const TestHeatingParams& params, float current_temperature) {
  return current_temperature > (params.temperature_target + params.temperature_deadband);
}

bool IsTemperatureTooLow(const TestHeatingParams& params, float current_temperature) {
  return current_temperature < (params.temperature_target - params.temperature_deadband);
}

bool IsTemperatureInRange(const TestHeatingParams& params, float current_temperature) {
  return !IsTemperatureTooHigh(params, current_temperature) &&
         !IsTemperatureTooLow(params, current_temperature);
}

void setUp(void) {
  // Called before each test
}

void tearDown(void) {
  // Called after each test
}

// ===== Temperature Validation Tests =====

void test_temperature_too_high(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;

  // 40.0 + 0.5 = 40.5, so 40.6 should be too high
  TEST_ASSERT_TRUE(IsTemperatureTooHigh(params, 40.6f));
  TEST_ASSERT_TRUE(IsTemperatureTooHigh(params, 45.0f));
}

void test_temperature_not_too_high_at_boundary(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;

  // Exactly at upper boundary should not be too high
  TEST_ASSERT_FALSE(IsTemperatureTooHigh(params, 40.5f));
  TEST_ASSERT_FALSE(IsTemperatureTooHigh(params, 40.0f));
}

void test_temperature_too_low(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;

  // 40.0 - 0.5 = 39.5, so 39.4 should be too low
  TEST_ASSERT_TRUE(IsTemperatureTooLow(params, 39.4f));
  TEST_ASSERT_TRUE(IsTemperatureTooLow(params, 35.0f));
}

void test_temperature_not_too_low_at_boundary(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;

  // Exactly at lower boundary should not be too low
  TEST_ASSERT_FALSE(IsTemperatureTooLow(params, 39.5f));
  TEST_ASSERT_FALSE(IsTemperatureTooLow(params, 40.0f));
}

void test_temperature_in_range(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;

  // Within deadband: 39.5 to 40.5
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 39.5f));
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 40.0f));
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 40.5f));
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 39.8f));
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 40.3f));
}

void test_temperature_out_of_range(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;

  // Outside deadband
  TEST_ASSERT_FALSE(IsTemperatureInRange(params, 39.4f));
  TEST_ASSERT_FALSE(IsTemperatureInRange(params, 40.6f));
  TEST_ASSERT_FALSE(IsTemperatureInRange(params, 35.0f));
  TEST_ASSERT_FALSE(IsTemperatureInRange(params, 45.0f));
}

// ===== CalculateStep Tests =====

void test_calculate_step_within_deadband(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;

  // Within deadband should return 0
  TEST_ASSERT_EQUAL_FLOAT(0.0f, CalculateStep(params, 40.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, CalculateStep(params, 39.5f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, CalculateStep(params, 40.5f));
}

void test_calculate_step_at_minimum(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;
  params.heater_full_scale_delta = 30.0f;

  // Just outside deadband should give minimum step
  // diff_abs = 0.51, diff_abs - deadband = 0.01
  // k = 0.01 / 30.0 = 0.000333...
  // step = 1.0 + 9.0 * 0.000333 ≈ 1.003
  float step = CalculateStep(params, 40.51f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, step);
}

void test_calculate_step_at_half_scale(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;
  params.heater_full_scale_delta = 30.0f;

  // Half full scale delta away
  // Temperature: 40 + 0.5 (deadband) + 15 (half of 30) = 55.5°C
  // diff_abs = 15.5, diff_abs - deadband = 15.0
  // k = 15.0 / 30.0 = 0.5
  // step = 1.0 + 9.0 * 0.5 = 5.5
  float step = CalculateStep(params, 55.5f);
  TEST_ASSERT_EQUAL_FLOAT(5.5f, step);
}

void test_calculate_step_at_full_scale(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;
  params.heater_full_scale_delta = 30.0f;

  // Full scale delta away
  // Temperature: 40 + 0.5 (deadband) + 30 (full scale) = 70.5°C
  // diff_abs = 30.5, diff_abs - deadband = 30.0
  // k = 30.0 / 30.0 = 1.0
  // step = 1.0 + 9.0 * 1.0 = 10.0
  float step = CalculateStep(params, 70.5f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, step);
}

void test_calculate_step_beyond_full_scale_clamped(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;
  params.heater_full_scale_delta = 30.0f;

  // Beyond full scale should clamp to max step
  // Temperature: 100°C (way beyond)
  // k should be clamped to 1.0
  // step = 10.0
  float step = CalculateStep(params, 100.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, step);
}

void test_calculate_step_below_target(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;
  params.heater_full_scale_delta = 30.0f;

  // Below target should work the same (absolute value)
  // Temperature: 40 - 0.5 (deadband) - 15 (half scale) = 24.5°C
  // diff_abs = 15.5, diff_abs - deadband = 15.0
  // k = 0.5, step = 5.5
  float step = CalculateStep(params, 24.5f);
  TEST_ASSERT_EQUAL_FLOAT(5.5f, step);
}

void test_calculate_step_with_different_min_max(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 1.0f;
  params.heater_step_min = 2.0f;
  params.heater_step_max = 8.0f;
  params.heater_full_scale_delta = 20.0f;

  // Half scale: 40 + 1.0 + 10.0 = 51.0°C
  // k = 0.5
  // step = 2.0 + 6.0 * 0.5 = 5.0
  float step = CalculateStep(params, 51.0f);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, step);
}

void test_calculate_step_with_zero_deadband(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.0f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;
  params.heater_full_scale_delta = 30.0f;

  // No deadband, any difference should give step
  // At exactly target: diff_abs = 0, should return 0
  TEST_ASSERT_EQUAL_FLOAT(0.0f, CalculateStep(params, 40.0f));

  // Tiny difference: 40.1°C
  // diff_abs = 0.1, k = 0.1 / 30.0 = 0.00333
  // step = 1.0 + 9.0 * 0.00333 ≈ 1.03
  float step = CalculateStep(params, 40.1f);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.03f, step);
}

void test_calculate_step_quarter_scale(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;
  params.heater_full_scale_delta = 30.0f;

  // Quarter scale: 40 + 0.5 + 7.5 = 48.0°C
  // diff_abs = 8.0, diff_abs - deadband = 7.5
  // k = 7.5 / 30.0 = 0.25
  // step = 1.0 + 9.0 * 0.25 = 3.25
  float step = CalculateStep(params, 48.0f);
  TEST_ASSERT_EQUAL_FLOAT(3.25f, step);
}

// ===== Edge Case Tests =====

void test_temperature_validation_with_zero_deadband(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.0f;

  // With zero deadband, only exactly at target is in range
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 40.0f));
  TEST_ASSERT_FALSE(IsTemperatureInRange(params, 40.01f));
  TEST_ASSERT_FALSE(IsTemperatureInRange(params, 39.99f));
}

void test_temperature_validation_with_large_deadband(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 5.0f;

  // Large deadband: 35.0 to 45.0 is in range
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 35.0f));
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 45.0f));
  TEST_ASSERT_TRUE(IsTemperatureInRange(params, 40.0f));
  TEST_ASSERT_FALSE(IsTemperatureInRange(params, 34.9f));
  TEST_ASSERT_FALSE(IsTemperatureInRange(params, 45.1f));
}

void test_calculate_step_symmetry(void) {
  TestHeatingParams params;
  params.temperature_target = 40.0f;
  params.temperature_deadband = 0.5f;
  params.heater_step_min = 1.0f;
  params.heater_step_max = 10.0f;
  params.heater_full_scale_delta = 30.0f;

  // Step should be the same for equal distance above and below target
  float step_above = CalculateStep(params, 50.0f);
  float step_below = CalculateStep(params, 30.0f);
  TEST_ASSERT_EQUAL_FLOAT(step_above, step_below);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Temperature validation tests
  RUN_TEST(test_temperature_too_high);
  RUN_TEST(test_temperature_not_too_high_at_boundary);
  RUN_TEST(test_temperature_too_low);
  RUN_TEST(test_temperature_not_too_low_at_boundary);
  RUN_TEST(test_temperature_in_range);
  RUN_TEST(test_temperature_out_of_range);

  // CalculateStep tests
  RUN_TEST(test_calculate_step_within_deadband);
  RUN_TEST(test_calculate_step_at_minimum);
  RUN_TEST(test_calculate_step_at_half_scale);
  RUN_TEST(test_calculate_step_at_full_scale);
  RUN_TEST(test_calculate_step_beyond_full_scale_clamped);
  RUN_TEST(test_calculate_step_below_target);
  RUN_TEST(test_calculate_step_with_different_min_max);
  RUN_TEST(test_calculate_step_with_zero_deadband);
  RUN_TEST(test_calculate_step_quarter_scale);

  // Edge case tests
  RUN_TEST(test_temperature_validation_with_zero_deadband);
  RUN_TEST(test_temperature_validation_with_large_deadband);
  RUN_TEST(test_calculate_step_symmetry);

  return UNITY_END();
}
