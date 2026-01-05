#include <unity.h>
#include "DisplayHelpers.h"
#include <cmath>
#include <cstring>

/**
 * Unit tests for DisplayHelpers.h
 * Pure formatting and calculation functions
 */

void setUp(void)
{
  // Called before each test
}

void tearDown(void)
{
  // Called after each test
}

// ===== FormatDuration Tests =====

void test_format_duration_zero_seconds(void)
{
  const char *result = FormatDuration(0);
  TEST_ASSERT_EQUAL_STRING("00:00:00", result);
}

void test_format_duration_59_seconds(void)
{
  const char *result = FormatDuration(59);
  TEST_ASSERT_EQUAL_STRING("00:00:59", result);
}

void test_format_duration_60_seconds(void)
{
  const char *result = FormatDuration(60);
  TEST_ASSERT_EQUAL_STRING("00:01:00", result);
}

void test_format_duration_3599_seconds(void)
{
  const char *result = FormatDuration(3599);
  TEST_ASSERT_EQUAL_STRING("00:59:59", result);
}

void test_format_duration_3600_seconds(void)
{
  const char *result = FormatDuration(3600);
  TEST_ASSERT_EQUAL_STRING("01:00:00", result);
}

void test_format_duration_7265_seconds(void)
{
  // 2 hours, 1 minute, 5 seconds
  const char *result = FormatDuration(7265);
  TEST_ASSERT_EQUAL_STRING("02:01:05", result);
}

void test_format_duration_large_value(void)
{
  // 99 hours, 59 minutes, 59 seconds
  const char *result = FormatDuration(359999);
  TEST_ASSERT_EQUAL_STRING("99:59:59", result);
}

// ===== GetProgressBarPosition Tests =====

void test_progress_bar_position_0_percent(void)
{
  int16_t result = GetProgressBarPosition(10, 100, 0.0f);
  TEST_ASSERT_EQUAL_INT16(10, result);
}

void test_progress_bar_position_50_percent(void)
{
  int16_t result = GetProgressBarPosition(10, 100, 0.5f);
  TEST_ASSERT_EQUAL_INT16(60, result);
}

void test_progress_bar_position_100_percent(void)
{
  int16_t result = GetProgressBarPosition(10, 100, 1.0f);
  TEST_ASSERT_EQUAL_INT16(110, result);
}

void test_progress_bar_position_with_offset(void)
{
  int16_t result = GetProgressBarPosition(5, 50, 0.25f);
  TEST_ASSERT_EQUAL_INT16(17, result);
}

// ===== GetProgress Tests =====

void test_get_progress_zero_current(void)
{
  float result = GetProgress(0, 100);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_get_progress_half_complete(void)
{
  float result = GetProgress(50, 100);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, result);
}

void test_get_progress_100_percent(void)
{
  float result = GetProgress(100, 100);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, result);
}

void test_get_progress_over_100_percent_clamped(void)
{
  float result = GetProgress(150, 100);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, result);
}

void test_get_progress_zero_total(void)
{
  float result = GetProgress(50, 0);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_get_progress_fractional(void)
{
  float result = GetProgress(25, 200);
  TEST_ASSERT_EQUAL_FLOAT(0.125f, result);
}

// ===== FormatTemperature Tests =====

void test_format_temperature_valid_positive(void)
{
  const char *result = FormatTemperature(25.5f);
  TEST_ASSERT_EQUAL_STRING("25.5°", result);
}

void test_format_temperature_valid_negative(void)
{
  const char *result = FormatTemperature(-10.2f);
  TEST_ASSERT_EQUAL_STRING("-10.2°", result);
}

void test_format_temperature_zero(void)
{
  const char *result = FormatTemperature(0.0f);
  TEST_ASSERT_EQUAL_STRING("0.0°", result);
}

void test_format_temperature_nan(void)
{
  const char *result = FormatTemperature(NAN);
  TEST_ASSERT_EQUAL_STRING("--.-°", result);
}

void test_format_temperature_rounding(void)
{
  const char *result = FormatTemperature(25.47f);
  TEST_ASSERT_EQUAL_STRING("25.5°", result);
}

// ===== FormatPercent Tests =====

void test_format_percent_zero(void)
{
  const char *result = FormatPercent(0.0f);
  TEST_ASSERT_EQUAL_STRING("0%", result);
}

void test_format_percent_50(void)
{
  const char *result = FormatPercent(50.0f);
  TEST_ASSERT_EQUAL_STRING("50%", result);
}

void test_format_percent_100(void)
{
  const char *result = FormatPercent(100.0f);
  TEST_ASSERT_EQUAL_STRING("100%", result);
}

void test_format_percent_nan(void)
{
  const char *result = FormatPercent(NAN);
  TEST_ASSERT_EQUAL_STRING("--%", result);
}

void test_format_percent_rounding(void)
{
  const char *result = FormatPercent(45.7f);
  TEST_ASSERT_EQUAL_STRING("46%", result);
}

void test_format_percent_fractional(void)
{
  const char *result = FormatPercent(33.3f);
  TEST_ASSERT_EQUAL_STRING("33%", result);
}

// ===== FormatTemperaturePercent Tests =====

void test_format_temperature_percent_valid(void)
{
  const char *result = FormatTemperaturePercent(25.5f, 60.0f);
  TEST_ASSERT_EQUAL_STRING("25.5° 60%", result);
}

void test_format_temperature_percent_both_nan(void)
{
  const char *result = FormatTemperaturePercent(NAN, NAN);
  TEST_ASSERT_EQUAL_STRING("--.-° --%", result);
}

void test_format_temperature_percent_temp_nan(void)
{
  const char *result = FormatTemperaturePercent(NAN, 75.0f);
  TEST_ASSERT_EQUAL_STRING("--.-° 75%", result);
}

void test_format_temperature_percent_humidity_nan(void)
{
  const char *result = FormatTemperaturePercent(22.0f, NAN);
  TEST_ASSERT_EQUAL_STRING("22.0° --%", result);
}

void test_format_temperature_percent_negative_temp(void)
{
  const char *result = FormatTemperaturePercent(-5.5f, 85.0f);
  TEST_ASSERT_EQUAL_STRING("-5.5° 85%", result);
}

// ===== GetStateStr Tests =====

void test_get_state_str_true(void)
{
  const char *result = GetStateStr(true);
  TEST_ASSERT_EQUAL_STRING("ON", result);
}

void test_get_state_str_false(void)
{
  const char *result = GetStateStr(false);
  TEST_ASSERT_EQUAL_STRING("OFF", result);
}

// ===== Test Runner =====

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  // FormatDuration tests
  RUN_TEST(test_format_duration_zero_seconds);
  RUN_TEST(test_format_duration_59_seconds);
  RUN_TEST(test_format_duration_60_seconds);
  RUN_TEST(test_format_duration_3599_seconds);
  RUN_TEST(test_format_duration_3600_seconds);
  RUN_TEST(test_format_duration_7265_seconds);
  RUN_TEST(test_format_duration_large_value);

  // GetProgressBarPosition tests
  RUN_TEST(test_progress_bar_position_0_percent);
  RUN_TEST(test_progress_bar_position_50_percent);
  RUN_TEST(test_progress_bar_position_100_percent);
  RUN_TEST(test_progress_bar_position_with_offset);

  // GetProgress tests
  RUN_TEST(test_get_progress_zero_current);
  RUN_TEST(test_get_progress_half_complete);
  RUN_TEST(test_get_progress_100_percent);
  RUN_TEST(test_get_progress_over_100_percent_clamped);
  RUN_TEST(test_get_progress_zero_total);
  RUN_TEST(test_get_progress_fractional);

  // FormatTemperature tests
  RUN_TEST(test_format_temperature_valid_positive);
  RUN_TEST(test_format_temperature_valid_negative);
  RUN_TEST(test_format_temperature_zero);
  RUN_TEST(test_format_temperature_nan);
  RUN_TEST(test_format_temperature_rounding);

  // FormatPercent tests
  RUN_TEST(test_format_percent_zero);
  RUN_TEST(test_format_percent_50);
  RUN_TEST(test_format_percent_100);
  RUN_TEST(test_format_percent_nan);
  RUN_TEST(test_format_percent_rounding);
  RUN_TEST(test_format_percent_fractional);

  // FormatTemperaturePercent tests
  RUN_TEST(test_format_temperature_percent_valid);
  RUN_TEST(test_format_temperature_percent_both_nan);
  RUN_TEST(test_format_temperature_percent_temp_nan);
  RUN_TEST(test_format_temperature_percent_humidity_nan);
  RUN_TEST(test_format_temperature_percent_negative_temp);

  // GetStateStr tests
  RUN_TEST(test_get_state_str_true);
  RUN_TEST(test_get_state_str_false);

  return UNITY_END();
}
