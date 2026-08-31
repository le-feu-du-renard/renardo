// Unit tests for AirDamper and DamperFeedback — one command, one or two
// registers.
//
// The command is binary and drives every register at once; each register keeps
// its own calibration because they are asymmetric. These cover the two ordered
// calibration marks and the signal direction that gives them meaning, the
// detection of a channel carrying no signal at all, the travel detection behind
// the openings on the main screen, and two things that would each be a silent
// disaster: the per-register direction switch, where getting it backwards shows
// a plausible screen describing a dryer that is recycling when it believes it is
// extracting, and the airflow interlock, which is the only reading in this
// firmware allowed to stop the dryer.

#include <unity.h>
#include "AirDamper.h"

void setUp(void) { TestSetMillis(0); }
void tearDown(void) {}

namespace
{

// The complementary pair the dryer has always run, with the actuator model's
// signal the intuitive way round unless a test says otherwise.
//
// Note what that means for the recycling register, and it is the whole point of
// the pair: its switch is inverted, so its feedback is mirrored too, and raw
// 4000 — wide open on the extraction register — is *shut* on this one. Both
// channels carrying the same voltage is the normal state of a complementary
// pair, not a fault.
DamperConfig TwoRegisters(bool low_is_open = false)
{
  DamperConfig config{};
  config.count                = 2;
  config.feedback_low_is_open = low_is_open;
  config.extraction_inverted  = false;
  config.recycling_inverted   = true;
  config.extraction_raw_min   = 800;
  config.extraction_raw_max   = 4000;
  config.recycling_raw_min    = 800;
  config.recycling_raw_max    = 4000;
  return config;
}

// Park both registers at a given opening, in raw counts of the calibration
// above, and run the interlock long enough for it to make up its mind.
void SettleBothAt(AirDamper &damper, uint16_t extraction_raw, uint16_t recycling_raw)
{
  damper.Extraction().SetRawPosition(extraction_raw);
  damper.Recycling().SetRawPosition(recycling_raw);
  damper.UpdateInterlock();
  TestAdvanceMillis(DAMPER_BLOCKED_CONFIRM_MS + 1);
  damper.Extraction().SetRawPosition(extraction_raw);
  damper.Recycling().SetRawPosition(recycling_raw);
  damper.UpdateInterlock();
}

} // namespace

void test_command_is_binary(void)
{
  AirDamper damper;
  TEST_ASSERT_FALSE(damper.IsOpen());

  damper.Open();
  TEST_ASSERT_TRUE(damper.IsOpen());

  damper.Close();
  TEST_ASSERT_FALSE(damper.IsOpen());
}

void test_command_polarity_only_moves_the_relay(void)
{
  // The one setting on the register page that changes where the air goes. It
  // belongs to the wire, so it stops at the relay: the air path, the targets and
  // everything above them read the same on either polarity.
  AirDamper damper;
  DamperConfig config = TwoRegisters();

  config.command_inverted = false;
  damper.ApplyConfig(config);
  damper.Open();
  TEST_ASSERT_TRUE(damper.GetRelayOutput());
  damper.Close();
  TEST_ASSERT_FALSE(damper.GetRelayOutput());

  config.command_inverted = true;
  damper.ApplyConfig(config);
  TEST_ASSERT_TRUE(damper.GetRelayOutput());  // at rest, and extracting
  TEST_ASSERT_FALSE(damper.IsOpen());         // which the dryer still calls shut
  TEST_ASSERT_FALSE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_TRUE(damper.Recycling().GetTargetOpen());

  damper.Open();
  TEST_ASSERT_FALSE(damper.GetRelayOutput());
  TEST_ASSERT_TRUE(damper.IsOpen());
}

void test_registers_are_complementary(void)
{
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters());

  // Recirculation: the recycling register is the one that should be open.
  damper.Close();
  TEST_ASSERT_FALSE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_TRUE(damper.Recycling().GetTargetOpen());

  damper.Open();
  TEST_ASSERT_TRUE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_FALSE(damper.Recycling().GetTargetOpen());
}

void test_direction_switch_decides_where_a_register_goes(void)
{
  // Each Belimo has a mechanical direction switch and the firmware cannot read
  // it, so where it is set is a setting. All four combinations, because the pair
  // set the same way round is a real misconfiguration — and the one the airflow
  // interlock exists to catch.
  AirDamper damper;
  DamperConfig config = TwoRegisters();

  config.extraction_inverted = false;
  config.recycling_inverted  = false;
  damper.ApplyConfig(config);
  damper.Open();
  TEST_ASSERT_TRUE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_TRUE(damper.Recycling().GetTargetOpen());

  config.extraction_inverted = true;
  config.recycling_inverted  = true;
  damper.ApplyConfig(config);
  TEST_ASSERT_FALSE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_FALSE(damper.Recycling().GetTargetOpen());

  config.extraction_inverted = true;
  config.recycling_inverted  = false;
  damper.ApplyConfig(config);
  TEST_ASSERT_FALSE(damper.Extraction().GetTargetOpen());
  TEST_ASSERT_TRUE(damper.Recycling().GetTargetOpen());
}

void test_a_direction_change_reaches_the_registers_at_once(void)
{
  // Without the re-push in ApplyConfig, a direction changed from the menu would
  // only take effect at the next command — which, mid-phase, can be an hour off.
  AirDamper damper;
  DamperConfig config = TwoRegisters();
  damper.ApplyConfig(config);
  damper.Open();
  TEST_ASSERT_TRUE(damper.Extraction().GetTargetOpen());

  config.extraction_inverted = true;
  damper.ApplyConfig(config);
  TEST_ASSERT_FALSE(damper.Extraction().GetTargetOpen());
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

void test_position_maps_the_calibration_marks(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000, false);

  damper.Extraction().SetRawPosition(800);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(4000);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(2400);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, damper.Extraction().GetPositionPercent());
}

void test_signal_direction_flips_the_same_marks(void)
{
  // The heart of the new calibration model: the two marks are the same numbers
  // in the same order either way, and only the direction flag says which end is
  // open. This dryer's actuator puts out 10.10 V shut and 2.00 V open, which is
  // the low_is_open case.
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000, true);

  damper.Extraction().SetRawPosition(800);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(4000);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(2400);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, damper.Extraction().GetPositionPercent());
}

void test_marks_entered_in_descending_order_are_rejected(void)
{
  // What the ordered marks buy over the old named ends: a pair the wrong way
  // round used to be a perfectly valid, perfectly inside-out calibration. It is
  // now simply not a calibration.
  AirDamper damper;
  damper.Extraction().SetCalibration(4000, 800, false);
  damper.Extraction().SetRawPosition(2400);

  TEST_ASSERT_TRUE(isnan(damper.Extraction().GetPositionPercent()));
}

void test_each_register_keeps_its_own_calibration(void)
{
  // The whole reason for two DamperFeedback instances: the registers are
  // asymmetric, so the same raw reading means different openings on each.
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000, false);
  damper.Recycling().SetCalibration(600, 2000, false);

  damper.Extraction().SetRawPosition(2400);
  damper.Recycling().SetRawPosition(2400);

  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, damper.Extraction().GetPositionPercent());
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Recycling().GetPositionPercent());
}

void test_a_degenerate_span_is_caught(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(1000, 1010, false);
  damper.Extraction().SetRawPosition(1005);

  TEST_ASSERT_TRUE(isnan(damper.Extraction().GetPositionPercent()));
  TEST_ASSERT_FALSE(damper.Extraction().IsMoving());
}

void test_position_is_clamped_outside_the_marks(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000, false);

  damper.Extraction().SetRawPosition(600);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Extraction().GetPositionPercent());

  damper.Extraction().SetRawPosition(4095);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Extraction().GetPositionPercent());
}

void test_a_channel_with_no_signal_reports_nothing(void)
{
  // The divider's lower resistor pulls the tap to ground, so an absent or dead
  // feedback wire reads near zero rather than floating. Reporting that as 0 %
  // would be a lie, and on the low_is_open dryer it would read as 100 % — a
  // register the screen shows wide open while its wire is cut.
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000, true);

  for (uint8_t i = 0; i < DAMPER_SIGNAL_CONFIRM_SAMPLES; i++)
  {
    damper.Extraction().SetRawPosition(3);
  }

  TEST_ASSERT_FALSE(damper.Extraction().HasSignal());
  TEST_ASSERT_TRUE(isnan(damper.Extraction().GetPositionPercent()));
  TEST_ASSERT_FALSE(damper.Extraction().IsClosed());
}

void test_one_quiet_sample_does_not_declare_a_feedback_dead(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000, false);

  damper.Extraction().SetRawPosition(2400);
  damper.Extraction().SetRawPosition(0); // a single bad conversion
  TEST_ASSERT_TRUE(damper.Extraction().HasSignal());

  damper.Extraction().SetRawPosition(2400);
  TEST_ASSERT_TRUE(damper.Extraction().HasSignal());
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, damper.Extraction().GetPositionPercent());
}

void test_a_signal_that_comes_back_is_believed_again(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000, false);

  for (uint8_t i = 0; i < DAMPER_SIGNAL_CONFIRM_SAMPLES + 5; i++)
  {
    damper.Extraction().SetRawPosition(0);
  }
  TEST_ASSERT_FALSE(damper.Extraction().HasSignal());

  damper.Extraction().SetRawPosition(2400);
  TEST_ASSERT_TRUE(damper.Extraction().HasSignal());
}

void test_moving_while_travelling_to_the_commanded_end(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(800, 4000, false);

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
  damper.ApplyConfig(TwoRegisters());

  // Commanding extraction sends the recycling register to *closed*, so a
  // recycling register sitting wide open is the one that is still travelling.
  // Wide open, on this mirrored channel, is raw 800.
  damper.Open();
  damper.Recycling().SetRawPosition(800);
  TEST_ASSERT_TRUE(damper.Recycling().IsMoving());

  damper.Recycling().SetRawPosition(4000);
  TEST_ASSERT_FALSE(damper.Recycling().IsMoving());
}

void test_the_direction_switch_turns_the_feedback_round_too(void)
{
  // The bug this pair of assertions exists for: the Belimo's U output reports
  // position in the actuator's own frame, which the direction switch mirrors, so
  // a complementary pair reads the *same* voltage on both channels. Resolving
  // that with one dryer-wide signal sense gave both registers the same opening —
  // a screen showing two registers shut, an airflow interlock tripping through
  // half of every session, and no way at all to configure the pair from the
  // menu, since the calibration marks are ordered and cannot be entered
  // backwards to compensate.
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters());

  damper.Extraction().SetRawPosition(4000);
  damper.Recycling().SetRawPosition(4000);

  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Extraction().GetPositionPercent());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Recycling().GetPositionPercent());

  damper.Extraction().SetRawPosition(800);
  damper.Recycling().SetRawPosition(800);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, damper.Extraction().GetPositionPercent());
  TEST_ASSERT_EQUAL_FLOAT(100.0f, damper.Recycling().GetPositionPercent());
}

void test_the_signal_sense_setting_survives_a_round_trip(void)
{
  // The per-register sense is derived, so the setting it is derived from has to
  // be kept: reading it back off the extraction register would fold that
  // register's switch into it and flip the stored value on every save.
  AirDamper damper;
  DamperConfig config = TwoRegisters(true);
  config.extraction_inverted = true;
  damper.ApplyConfig(config);

  TEST_ASSERT_TRUE(damper.GetFeedbackLowIsOpen());
  TEST_ASSERT_FALSE(damper.Extraction().GetLowIsOpen());
}

void test_damper_is_moving_while_either_register_travels(void)
{
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters());

  damper.Open();
  damper.Extraction().SetRawPosition(4000); // arrived
  damper.Recycling().SetRawPosition(2400);  // still on its way to closed
  TEST_ASSERT_TRUE(damper.IsMoving());

  damper.Recycling().SetRawPosition(4000);  // arrived too — mirrored channel
  TEST_ASSERT_FALSE(damper.IsMoving());
}

void test_a_register_the_dryer_does_not_have_is_not_consulted(void)
{
  // With one register declared, GP27 is never sampled, so Recycling() holds
  // whatever it last had. It must not be able to report travel or shut the air
  // path on a dryer that does not own it.
  AirDamper damper;
  DamperConfig config = TwoRegisters();
  config.count = 1;
  damper.ApplyConfig(config);

  damper.Open();
  damper.Extraction().SetRawPosition(4000); // arrived
  damper.Recycling().SetRawPosition(800);   // stale, and pointing the wrong way

  TEST_ASSERT_FALSE(damper.IsMoving());
  TEST_ASSERT_TRUE(damper.IsFeedbackUsable());
}

void test_one_dead_feedback_does_not_hide_the_other(void)
{
  AirDamper damper;
  DamperConfig config = TwoRegisters();
  config.extraction_raw_min = 1000; // degenerate: wire absent
  config.extraction_raw_max = 1010;
  damper.ApplyConfig(config);

  damper.Open(); // recycling should close
  damper.Extraction().SetRawPosition(1005);
  damper.Recycling().SetRawPosition(800); // still fully open

  TEST_ASSERT_FALSE(damper.Extraction().IsMoving());
  TEST_ASSERT_TRUE(damper.Recycling().IsMoving());
  TEST_ASSERT_TRUE(damper.IsMoving());
}

void test_arrival_tolerance_absorbs_actuator_slop(void)
{
  AirDamper damper;
  damper.Extraction().SetCalibration(0, 4000, false);

  damper.Open();
  // 97 % — within DAMPER_POSITION_TOLERANCE of the end stop, so travel is done.
  damper.Extraction().SetRawPosition(3880);
  TEST_ASSERT_FALSE(damper.Extraction().IsMoving());

  // 90 % — still moving.
  damper.Extraction().SetRawPosition(3600);
  TEST_ASSERT_TRUE(damper.Extraction().IsMoving());
}

// --- Airflow interlock ------------------------------------------------------

void test_both_registers_shut_blocks_the_airflow(void)
{
  // Both shut is not both channels at the same voltage — that is the normal
  // complementary state. It is each channel at the shut end of *its own*
  // mirrored signal: 800 on the extraction register, 4000 on the recycling one.
  // What puts a pair there is one of them failing to travel.
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters());

  SettleBothAt(damper, 800, 4000);
  TEST_ASSERT_TRUE(damper.IsAirflowBlocked());
}

void test_the_interlock_waits_for_confirmation(void)
{
  // Two registers can read shut for a moment while the readings are sampled a
  // beat apart, and a fault that trips on one sampling round would stop the
  // dryer on noise.
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters());

  damper.Extraction().SetRawPosition(800);
  damper.Recycling().SetRawPosition(4000);
  damper.UpdateInterlock();
  TEST_ASSERT_FALSE(damper.IsAirflowBlocked());

  TestAdvanceMillis(DAMPER_BLOCKED_CONFIRM_MS - 1);
  damper.UpdateInterlock();
  TEST_ASSERT_FALSE(damper.IsAirflowBlocked());

  TestAdvanceMillis(2);
  damper.UpdateInterlock();
  TEST_ASSERT_TRUE(damper.IsAirflowBlocked());
}

void test_one_open_register_is_enough_for_air_to_move(void)
{
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters());

  // Both channels at the same voltage: the healthy complementary pair, one
  // register shut and the other wide open.
  SettleBothAt(damper, 800, 800);
  TEST_ASSERT_FALSE(damper.IsAirflowBlocked());
}

void test_a_single_register_can_never_block_the_airflow(void)
{
  // One register shut is a normal recirculation, not a fault — the interlock
  // only exists on a dryer that has two.
  AirDamper damper;
  DamperConfig config = TwoRegisters();
  config.count = 1;
  damper.ApplyConfig(config);

  SettleBothAt(damper, 800, 4000);
  TEST_ASSERT_FALSE(damper.IsAirflowBlocked());
}

void test_the_fault_clears_itself_when_a_register_opens(void)
{
  // Nothing latches: the operator fixes the direction switch and the dryer will
  // start again, without an acknowledgement step that does not exist anywhere
  // else in this firmware.
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters());

  SettleBothAt(damper, 800, 4000);
  TEST_ASSERT_TRUE(damper.IsAirflowBlocked());

  damper.Recycling().SetRawPosition(800);
  damper.UpdateInterlock();
  TEST_ASSERT_FALSE(damper.IsAirflowBlocked());
}

void test_the_interlock_never_trips_on_an_absent_reading(void)
{
  // A dead feedback reads near zero, which through a low_is_open calibration
  // would otherwise look exactly like a register at its shut stop. It must not:
  // a missing reading refuses the next start, it does not stop the dryer.
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters(true));

  for (uint8_t i = 0; i < DAMPER_SIGNAL_CONFIRM_SAMPLES; i++)
  {
    damper.Extraction().SetRawPosition(2);
    damper.Recycling().SetRawPosition(2);
  }
  TestAdvanceMillis(DAMPER_BLOCKED_CONFIRM_MS + 1);
  damper.UpdateInterlock();

  TEST_ASSERT_FALSE(damper.IsAirflowBlocked());
  TEST_ASSERT_FALSE(damper.IsFeedbackUsable());
}

void test_feedback_is_usable_only_for_the_declared_registers(void)
{
  AirDamper damper;
  damper.ApplyConfig(TwoRegisters());
  TEST_ASSERT_FALSE(damper.IsFeedbackUsable()); // no samples yet

  damper.Extraction().SetRawPosition(2400);
  TEST_ASSERT_FALSE(damper.IsFeedbackUsable()); // the second one is still silent

  damper.Recycling().SetRawPosition(2400);
  TEST_ASSERT_TRUE(damper.IsFeedbackUsable());
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_command_is_binary);
  RUN_TEST(test_command_polarity_only_moves_the_relay);
  RUN_TEST(test_registers_are_complementary);
  RUN_TEST(test_direction_switch_decides_where_a_register_goes);
  RUN_TEST(test_a_direction_change_reaches_the_registers_at_once);
  RUN_TEST(test_targets_are_consistent_before_any_command);
  RUN_TEST(test_position_is_unknown_before_any_sample);
  RUN_TEST(test_position_maps_the_calibration_marks);
  RUN_TEST(test_signal_direction_flips_the_same_marks);
  RUN_TEST(test_marks_entered_in_descending_order_are_rejected);
  RUN_TEST(test_each_register_keeps_its_own_calibration);
  RUN_TEST(test_a_degenerate_span_is_caught);
  RUN_TEST(test_position_is_clamped_outside_the_marks);
  RUN_TEST(test_a_channel_with_no_signal_reports_nothing);
  RUN_TEST(test_one_quiet_sample_does_not_declare_a_feedback_dead);
  RUN_TEST(test_a_signal_that_comes_back_is_believed_again);
  RUN_TEST(test_moving_while_travelling_to_the_commanded_end);
  RUN_TEST(test_recycling_travels_the_other_way);
  RUN_TEST(test_the_direction_switch_turns_the_feedback_round_too);
  RUN_TEST(test_the_signal_sense_setting_survives_a_round_trip);
  RUN_TEST(test_damper_is_moving_while_either_register_travels);
  RUN_TEST(test_a_register_the_dryer_does_not_have_is_not_consulted);
  RUN_TEST(test_one_dead_feedback_does_not_hide_the_other);
  RUN_TEST(test_arrival_tolerance_absorbs_actuator_slop);
  RUN_TEST(test_both_registers_shut_blocks_the_airflow);
  RUN_TEST(test_the_interlock_waits_for_confirmation);
  RUN_TEST(test_one_open_register_is_enough_for_air_to_move);
  RUN_TEST(test_a_single_register_can_never_block_the_airflow);
  RUN_TEST(test_the_fault_clears_itself_when_a_register_opens);
  RUN_TEST(test_the_interlock_never_trips_on_an_absent_reading);
  RUN_TEST(test_feedback_is_usable_only_for_the_declared_registers);
  return UNITY_END();
}
