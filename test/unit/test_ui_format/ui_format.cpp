// Unit tests for the on-screen formatters.
//
// These are the places where a display bug is both easy to write and
// immediately visible: a duration field that changes width makes the header
// jitter, a missing probe rendered as 0.0 would read as a real measurement, and
// a register called FERME when it is in fact wide open would send an operator
// looking for a fault that is not there.
//
// They are also the only part of the interface that can be tested at all
// without a panel, which is why the palette and the formatters live in a header
// that knows nothing about TFT_eSPI.

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

void test_temperature_carries_its_unit(void)
{
  // The degree sign and nothing else: everything this machine measures is in
  // Celsius, and at the size the figures are set a trailing "C" costs a glyph
  // cell the cards cannot spare.
  char out[16];
  UiTheme::FormatTemperature(51.24f, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("51.2" UI_DEGREE, out);

  UiTheme::FormatTemperature(NAN, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--.-" UI_DEGREE, out);
}

void test_temperature_fits_its_reserved_width(void)
{
  // A temperature is not fixed width — "9.5~" is a cell narrower than "42.3~"
  // — which is exactly why the cards right-align it inside a five-cell slot
  // rather than trusting it to fill one. Five cells is what has to hold.
  const float samples[] = {0.0f, 9.5f, 42.3f, 99.9f, -3.5f, NAN};
  char out[16];

  for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
  {
    UiTheme::FormatTemperature(samples[i], out, sizeof(out));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(5, strlen(out));
  }
}

void test_percent_fits_its_reserved_width(void)
{
  // Right-aligned inside four cells, so "100%" is the widest case the cards
  // have to leave room for.
  const float samples[] = {0.0f, 38.0f, 99.6f, 100.0f, NAN};
  char out[16];

  for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
  {
    UiTheme::FormatPercent(samples[i], out, sizeof(out));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(4, strlen(out));
  }
}

void test_rounded_values_go_to_the_nearest(void)
{
  // Truncating would put every reading up to a whole point below the same
  // figure shown with a decimal elsewhere on the screen.
  char out[16];
  UiTheme::FormatRounded(39.8f, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("40", out);

  UiTheme::FormatRounded(39.2f, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("39", out);

  UiTheme::FormatRounded(NAN, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--", out);
}

void test_percent_adds_the_sign(void)
{
  char out[16];
  UiTheme::FormatPercent(38.4f, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("38%", out);

  UiTheme::FormatPercent(NAN, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--%", out);
}

void test_damper_end_stops_are_words(void)
{
  // The percentage is only worth screen space part way along; at the end stops
  // it says the same thing twice in a cell 74 px wide.
  char out[16];
  UiTheme::FormatDamperState(0.0f, false, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("FERME", out);

  UiTheme::FormatDamperState(100.0f, true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("OUVERT", out);

  UiTheme::FormatDamperState(65.0f, true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("OUV. 65%", out);
}

void test_damper_verb_follows_the_command_not_the_number(void)
{
  // A register caught mid-travel is equally "40% open" and "60% closed". The
  // verb says where it is bound; the number stays the measured opening, so the
  // same 40 % reads two ways depending on which way it is going.
  char out[16];
  UiTheme::FormatDamperState(40.0f, true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("OUV. 40%", out);

  UiTheme::FormatDamperState(40.0f, false, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("FER. 40%", out);

  // The end stops are past arguing about, and say the same word either way.
  UiTheme::FormatDamperState(100.0f, false, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("OUVERT", out);

  UiTheme::FormatDamperState(0.0f, true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("FERME", out);
}

void test_damper_rounds_before_testing_the_end_stops(void)
{
  // 99.6 % must not come out as "OUV. 100%", which would claim to be part way
  // and print a full opening in the same breath.
  char out[16];
  UiTheme::FormatDamperState(99.6f, true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("OUVERT", out);

  UiTheme::FormatDamperState(0.4f, false, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("FERME", out);
}

void test_damper_without_feedback_is_not_closed(void)
{
  // A register with a dead feedback wire reading "FERME" would be taken as a
  // measurement, and a shut extraction register is a real fault condition.
  char out[16];
  UiTheme::FormatDamperState(NAN, true, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--", out);

  UiTheme::FormatDamperState(NAN, false, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("--", out);
}

void test_damper_state_fits_the_cell(void)
{
  // Eight glyphs is what a 74 px cell holds in the 7 px font the row uses.
  const float samples[] = {0.0f, 1.0f, 50.0f, 99.0f, 100.0f, NAN};
  char out[16];

  // Both verbs, since "OUV." and "FER." are the same width but the assertion
  // should not be taking that on trust.
  for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
  {
    UiTheme::FormatDamperState(samples[i], true, out, sizeof(out));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(8, strlen(out));

    UiTheme::FormatDamperState(samples[i], false, out, sizeof(out));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(8, strlen(out));
  }
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
  RUN_TEST(test_temperature_carries_its_unit);
  RUN_TEST(test_temperature_fits_its_reserved_width);
  RUN_TEST(test_rounded_values_go_to_the_nearest);
  RUN_TEST(test_percent_adds_the_sign);
  RUN_TEST(test_percent_fits_its_reserved_width);
  RUN_TEST(test_damper_end_stops_are_words);
  RUN_TEST(test_damper_verb_follows_the_command_not_the_number);
  RUN_TEST(test_damper_rounds_before_testing_the_end_stops);
  RUN_TEST(test_damper_without_feedback_is_not_closed);
  RUN_TEST(test_damper_state_fits_the_cell);
  return UNITY_END();
}
