// Unit tests for AirDamper — the command is binary, the ADC feedback is for
// display only. These cover the two-point calibration and the travel detection
// that drives the progress bar on the main screen.

#include <unity.h>
#include "AirDamper.h"

void setUp(void) {}
void tearDown(void) {}

void test_command_is_binary(void)
{
  AirDamper damper;
  TEST_ASSERT_FALSE(damper.IsOpen());

  damper.Open();
  TEST_ASSERT_TRUE(damper.IsOpen());

  damper.Close();
  TEST_ASSERT_FALSE(damper.IsOpen());
}

void test_position_is_unknown_before_any_sample(void)
{
  AirDamper damper;
  TEST_ASSERT_TRUE(isnan(damper.GetPositionPercent()));
}

void test_position_maps_calibrated_end_stops(void)
{
  AirDamper damper;
  damper.SetCalibration(800, 4000);

  damper.SetRawPosition(800);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.GetPositionPercent());

  damper.SetRawPosition(4000);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.GetPositionPercent());

  damper.SetRawPosition(2400);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, damper.GetPositionPercent());
}

void test_position_is_clamped_outside_the_end_stops(void)
{
  AirDamper damper;
  damper.SetCalibration(800, 4000);

  // The Belimo feedback starts at 2V, so a reading below the calibrated closed
  // point means drift or noise, not a negative opening.
  damper.SetRawPosition(400);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.GetPositionPercent());

  damper.SetRawPosition(4095);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.GetPositionPercent());
}

void test_position_is_unknown_without_a_feedback_wire(void)
{
  AirDamper damper;
  // A disconnected feedback input calibrates to a degenerate span; reporting
  // 0 % or 100 % there would be a lie.
  damper.SetCalibration(1000, 1010);
  damper.SetRawPosition(1005);

  TEST_ASSERT_TRUE(isnan(damper.GetPositionPercent()));
  TEST_ASSERT_FALSE(damper.IsMoving());
}

void test_moving_while_travelling_to_the_commanded_end(void)
{
  AirDamper damper;
  damper.SetCalibration(800, 4000);

  damper.Open();
  damper.SetRawPosition(800);   // still fully closed
  TEST_ASSERT_TRUE(damper.IsMoving());

  damper.SetRawPosition(2400);  // mid travel
  TEST_ASSERT_TRUE(damper.IsMoving());

  damper.SetRawPosition(4000);  // arrived
  TEST_ASSERT_FALSE(damper.IsMoving());
}

void test_arrival_tolerance_absorbs_actuator_slop(void)
{
  AirDamper damper;
  damper.SetCalibration(0, 4000);

  damper.Open();
  // 97 % — within DAMPER_POSITION_TOLERANCE of the end stop, so travel is done.
  damper.SetRawPosition(3880);
  TEST_ASSERT_FALSE(damper.IsMoving());

  // 90 % — still moving.
  damper.SetRawPosition(3600);
  TEST_ASSERT_TRUE(damper.IsMoving());
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_command_is_binary);
  RUN_TEST(test_position_is_unknown_before_any_sample);
  RUN_TEST(test_position_maps_calibrated_end_stops);
  RUN_TEST(test_position_is_clamped_outside_the_end_stops);
  RUN_TEST(test_position_is_unknown_without_a_feedback_wire);
  RUN_TEST(test_moving_while_travelling_to_the_commanded_end);
  RUN_TEST(test_arrival_tolerance_absorbs_actuator_slop);
  return UNITY_END();
}
