// Unit tests for AirDamper and DamperFeedback — one command, two registers.
//
// The command is binary and drives both registers at once; each register keeps
// its own calibration because the two are asymmetric. These cover the two-point
// calibration, the travel detection behind the openings on the main screen, and
// above all the complementarity: extraction opens while recycling closes, and
// getting that backwards would show a plausible screen describing a dryer that
// is recycling when it believes it is extracting.

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

void test_registers_are_complementary(void)
{
  AirDamper damper;

  // Recirculation: the recycling register is the one that should be open.
  damper.Close();
  TEST_ASSERT_FALSE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_TRUE(damper.Recycling().GetTargetOpen());

  damper.Open();
  TEST_ASSERT_TRUE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_FALSE(damper.Recycling().GetTargetOpen());
}

void test_targets_are_consistent_before_any_command(void)
{
  // A fresh AirDamper reports recirculation, so the registers must already
  // agree with that — otherwise the first samples would show phantom travel.
  AirDamper damper;
  TEST_ASSERT_FALSE(damper.IsOpen());
  TEST_ASSERT_FALSE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_TRUE(damper.Recycling().GetTargetOpen());
}

void test_position_is_unknown_before_any_sample(void)
{
  AirDamper damper;
  TEST_ASSERT_TRUE(isnan(damper.Extraction().GetPositionPercent()));
  TEST_ASSERT_TRUE(isnan(damper.Recycling().GetPositionPercent()));
}

void test_position_maps_calibrated_end_stops(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000);

  damper.Extraction().SetRawPosition(800);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(4000);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(2400);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, damper.Extraction().GetPositionPercent());
}

void test_each_register_keeps_its_own_calibration(void)
{
  // The whole reason for two DamperFeedback instances: the registers are
  // asymmetric, so the same raw reading means different openings on each.
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000);
  damper.Recycling().SetCalibration(600, 2000);

  damper.Extraction().SetRawPosition(2400);
  damper.Recycling().SetRawPosition(2400);

  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, damper.Extraction().GetPositionPercent());
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Recycling().GetPositionPercent());
}

void test_feedback_running_backwards_still_reads_right(void)
{
  // The bench actuator puts out 10.10 V shut and 2.00 V open, so its closed
  // reading is the *higher* one and the calibrated span is negative. Nothing
  // inverts it explicitly: GetPositionPercent() takes the span as open minus
  // closed and the arithmetic carries the sign. Worth pinning down, because the
  // alternative — a silent flip somewhere — would show every opening inside out
  // while looking entirely plausible on screen.
  AirDamper damper;
  damper.Extraction().SetCalibration(3845, 1392);

  damper.Extraction().SetRawPosition(3845);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(1392);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(2618); // midpoint of the descending span
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, damper.Extraction().GetPositionPercent());
}

void test_backwards_feedback_clamps_at_both_ends(void)
{
  // Past either stop the ratio leaves 0..1 in the other direction than usual,
  // so the clamp has to hold on a negative span too.
  AirDamper damper;
  damper.Extraction().SetCalibration(3845, 1392);

  damper.Extraction().SetRawPosition(4000); // beyond shut
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(1000); // beyond open
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Extraction().GetPositionPercent());
}

void test_backwards_feedback_detects_travel(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(3845, 1392);

  damper.Open();
  damper.Extraction().SetRawPosition(3845); // still shut
  TEST_ASSERT_TRUE(damper.Extraction().IsMoving());

  damper.Extraction().SetRawPosition(1392); // arrived open
  TEST_ASSERT_FALSE(damper.Extraction().IsMoving());
}

void test_a_degenerate_span_is_caught_in_either_direction(void)
{
  // The guard is written signed so a backwards feedback with a dead wire is
  // rejected like any other, rather than sneaking past a magnitude test.
  AirDamper damper;
  damper.Extraction().SetCalibration(1010, 1000); // descending, but degenerate
  damper.Extraction().SetRawPosition(1005);

  TEST_ASSERT_TRUE(isnan(damper.Extraction().GetPositionPercent()));
}

void test_position_is_clamped_outside_the_end_stops(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000);

  // The Belimo feedback starts at 2V, so a reading below the calibrated closed
  // point means drift or noise, not a negative opening.
  damper.Extraction().SetRawPosition(400);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(4095);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Extraction().GetPositionPercent());
}

void test_position_is_unknown_without_a_feedback_wire(void)
{
  AirDamper damper;
  // A disconnected feedback input calibrates to a degenerate span; reporting
  // 0 % or 100 % there would be a lie.
  damper.Extraction().SetCalibration(1000, 1010);
  damper.Extraction().SetRawPosition(1005);

  TEST_ASSERT_TRUE(isnan(damper.Extraction().GetPositionPercent()));
  TEST_ASSERT_FALSE(damper.Extraction().IsMoving());
}

void test_one_dead_feedback_does_not_hide_the_other(void)
{
  // A single unwired register must not make the working one look idle.
  AirDamper damper;
  damper.Extraction().SetCalibration(1000, 1010); // degenerate: wire absent
  damper.Recycling().SetCalibration(800, 4000);

  damper.Open(); // recycling should close
  damper.Extraction().SetRawPosition(1005);
  damper.Recycling().SetRawPosition(4000); // still fully open

  TEST_ASSERT_FALSE(damper.Extraction().IsMoving());
  TEST_ASSERT_TRUE(damper.Recycling().IsMoving());
  TEST_ASSERT_TRUE(damper.IsMoving());
}

void test_moving_while_travelling_to_the_commanded_end(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000);

  damper.Open();
  damper.Extraction().SetRawPosition(800);   // still fully closed
  TEST_ASSERT_TRUE(damper.Extraction().IsMoving());

  damper.Extraction().SetRawPosition(2400);  // mid travel
  TEST_ASSERT_TRUE(damper.Extraction().IsMoving());

  damper.Extraction().SetRawPosition(4000);  // arrived
  TEST_ASSERT_FALSE(damper.Extraction().IsMoving());
}

void test_recycling_travels_the_other_way(void)
{
  AirDamper damper;
  damper.Recycling().SetCalibration(800, 4000);

  // Commanding extraction sends the recycling register to *closed*, so a
  // recycling register sitting wide open is the one that is still travelling.
  damper.Open();
  damper.Recycling().SetRawPosition(4000);
  TEST_ASSERT_TRUE(damper.Recycling().IsMoving());

  damper.Recycling().SetRawPosition(800);
  TEST_ASSERT_FALSE(damper.Recycling().IsMoving());
}

void test_damper_is_moving_while_either_register_travels(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000);
  damper.Recycling().SetCalibration(800, 4000);

  damper.Open();
  damper.Extraction().SetRawPosition(4000); // arrived
  damper.Recycling().SetRawPosition(2400);  // still on its way to closed
  TEST_ASSERT_TRUE(damper.IsMoving());

  damper.Recycling().SetRawPosition(800);   // arrived too
  TEST_ASSERT_FALSE(damper.IsMoving());
}

void test_arrival_tolerance_absorbs_actuator_slop(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(0, 4000);

  damper.Open();
  // 97 % — within DAMPER_POSITION_TOLERANCE of the end stop, so travel is done.
  damper.Extraction().SetRawPosition(3880);
  TEST_ASSERT_FALSE(damper.Extraction().IsMoving());

  // 90 % — still moving.
  damper.Extraction().SetRawPosition(3600);
  TEST_ASSERT_TRUE(damper.Extraction().IsMoving());
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_command_is_binary);
  RUN_TEST(test_registers_are_complementary);
  RUN_TEST(test_targets_are_consistent_before_any_command);
  RUN_TEST(test_position_is_unknown_before_any_sample);
  RUN_TEST(test_position_maps_calibrated_end_stops);
  RUN_TEST(test_each_register_keeps_its_own_calibration);
  RUN_TEST(test_feedback_running_backwards_still_reads_right);
  RUN_TEST(test_backwards_feedback_clamps_at_both_ends);
  RUN_TEST(test_backwards_feedback_detects_travel);
  RUN_TEST(test_a_degenerate_span_is_caught_in_either_direction);
  RUN_TEST(test_position_is_clamped_outside_the_end_stops);
  RUN_TEST(test_position_is_unknown_without_a_feedback_wire);
  RUN_TEST(test_one_dead_feedback_does_not_hide_the_other);
  RUN_TEST(test_moving_while_travelling_to_the_commanded_end);
  RUN_TEST(test_recycling_travels_the_other_way);
  RUN_TEST(test_damper_is_moving_while_either_register_travels);
  RUN_TEST(test_arrival_tolerance_absorbs_actuator_slop);
  return UNITY_END();
}
