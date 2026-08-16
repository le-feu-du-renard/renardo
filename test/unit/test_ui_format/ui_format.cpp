// Unit tests for the on-screen formatters.
//
// These are the two places where a display bug is both easy to write and
// immediately visible: a duration field that changes width makes the status bar
// jitter, and a missing probe rendered as 0.0 would read as a real measurement.

#include <unity.h>
#include <math.h>
#include <string.h>

#include "UiTheme.h"

void setUp(void) {}
void tearDown(void) {}

void test_duration_is_zero_padded(void)
{
  char out[16];
  UiTheme::FormatDuration(0, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("00:00:00", out);

  UiTheme::FormatDuration(3661, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("01:01:01", out);
}

void test_duration_width_never_changes(void)
{
  // The status bar puts the duration on the left precisely because it does not
  // reflow; if this width ever varies, the whole bar shifts.
  const uint32_t samples[] = {0, 9, 59, 60, 3599, 3600, 86399, 86400, 359999};
  char out[16];

  for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
  {
    UiTheme::FormatDuration(samples[i], out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(8, strlen(out));
  }
}

void test_duration_clamps_past_ninety_nine_hours(void)
{
  char out[16];
  // A session can legitimately run for days; the field saturates rather than
  // growing a third hour digit.
  UiTheme::FormatDuration(100UL * 3600UL, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("99:59:59", out);

  UiTheme::FormatDuration(1000UL * 3600UL, out, sizeof(out));
  TEST_ASSERT_EQUAL_UINT(8, strlen(out));
}

void test_value_keeps_one_decimal(void)
{
  char out[16];
  UiTheme::FormatValue(38.24f, "", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("38.2", out);

  UiTheme::FormatValue(62.0f, " %HR", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("62.0 %HR", out);
}

void test_missing_reading_is_not_shown_as_zero(void)
{
  char out[16];
  UiTheme::FormatValue(NAN, " C", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--.- C", out);
}

void test_negative_values_are_kept(void)
{
  // Water temperature comes back signed from the hydraulic module; a frozen
  // loop must not read as a positive one.
  char out[16];
  UiTheme::FormatValue(-3.5f, " C", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("-3.5 C", out);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_duration_is_zero_padded);
  RUN_TEST(test_duration_width_never_changes);
  RUN_TEST(test_duration_clamps_past_ninety_nine_hours);
  RUN_TEST(test_value_keeps_one_decimal);
  RUN_TEST(test_missing_reading_is_not_shown_as_zero);
  RUN_TEST(test_negative_values_are_kept);
  return UNITY_END();
}
