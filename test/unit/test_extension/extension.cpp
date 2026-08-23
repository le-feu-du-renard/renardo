// Unit tests for the extension port wire format.
//
// ExtensionPort itself needs the bus, so what is tested here is everything that
// decides what the module sees and what the dryer will act on: the sentinels
// that keep a missing reading distinct from a real zero, the flag bits a v3
// decoder still expects to find where they were, the validation that refuses a
// command, and the filter that stops a resident mailbox being replayed forever.

#include <unity.h>

#include "ExtensionProtocol.h"

void setUp(void) {}
void tearDown(void) {}

// --- Values -----------------------------------------------------------------

void test_a_missing_reading_encodes_to_the_sentinel(void)
{
  TEST_ASSERT_EQUAL_INT16(kExtInvalidValue, ExtEncodeValue(NAN));
  TEST_ASSERT_TRUE(isnan(ExtDecodeValue(kExtInvalidValue)));
}

void test_a_real_zero_is_not_the_sentinel(void)
{
  // The whole reason for a sentinel: a probe reading 0.0 C must not be reported
  // as an absent probe.
  TEST_ASSERT_EQUAL_INT16(0, ExtEncodeValue(0.0f));
  TEST_ASSERT_FALSE(isnan(ExtDecodeValue(0)));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, ExtDecodeValue(0));
}

void test_values_round_trip_through_tenths(void)
{
  TEST_ASSERT_EQUAL_FLOAT(42.3f, ExtDecodeValue(ExtEncodeValue(42.3f)));
  TEST_ASSERT_EQUAL_FLOAT(0.1f, ExtDecodeValue(ExtEncodeValue(0.1f)));
  TEST_ASSERT_EQUAL_FLOAT(100.0f, ExtDecodeValue(ExtEncodeValue(100.0f)));
}

void test_negative_values_survive(void)
{
  // The water loop can legitimately sit below zero.
  TEST_ASSERT_EQUAL_INT16(-45, ExtEncodeValue(-4.5f));
  TEST_ASSERT_EQUAL_FLOAT(-4.5f, ExtDecodeValue(ExtEncodeValue(-4.5f)));
}

void test_a_wild_reading_cannot_impersonate_the_sentinel(void)
{
  // A reading far enough below zero would encode onto kExtInvalidValue itself
  // and read back as a missing probe. It is clamped one step short instead.
  int16_t raw = ExtEncodeValue(-100000.0f);

  TEST_ASSERT_NOT_EQUAL(kExtInvalidValue, raw);
  TEST_ASSERT_FALSE(isnan(ExtDecodeValue(raw)));
}

// --- Positions --------------------------------------------------------------

void test_an_unknown_opening_encodes_to_its_own_sentinel(void)
{
  TEST_ASSERT_EQUAL_UINT16(kExtNoPosition, ExtEncodePosition(NAN));
  TEST_ASSERT_TRUE(isnan(ExtDecodePosition(kExtNoPosition)));
}

void test_both_ends_of_the_opening_range_stay_legal(void)
{
  // 0 % is a shut register and 100 % a wide open one; neither may be mistaken
  // for an absent feedback.
  TEST_ASSERT_EQUAL_UINT16(0, ExtEncodePosition(0.0f));
  TEST_ASSERT_EQUAL_UINT16(100, ExtEncodePosition(100.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, ExtDecodePosition(0));
  TEST_ASSERT_EQUAL_FLOAT(100.0f, ExtDecodePosition(100));
}

void test_a_drifting_feedback_is_clamped_not_wrapped(void)
{
  TEST_ASSERT_EQUAL_UINT16(0, ExtEncodePosition(-8.0f));
  TEST_ASSERT_EQUAL_UINT16(100, ExtEncodePosition(137.0f));
}

// --- Flags ------------------------------------------------------------------

void test_each_flag_sits_on_its_own_bit(void)
{
  ExtensionTelemetry telemetry;
  telemetry.hydraulic_online = true; // so the "off" bit stays clear

  TEST_ASSERT_EQUAL_UINT16(0, ExtEncodeFlags(telemetry));

  telemetry.running = true;
  TEST_ASSERT_EQUAL_UINT16(kExtFlagRunning, ExtEncodeFlags(telemetry));

  telemetry.fan_on = true;
  telemetry.electric_on = true;
  telemetry.hydraulic_demand = true;
  telemetry.damper_open = true;
  telemetry.sensor_fault = true;
  telemetry.airflow_fault = true;
  telemetry.feedback_fault = true;

  uint16_t expected = kExtFlagRunning | kExtFlagFan | kExtFlagElectric |
                      kExtFlagHydraulic | kExtFlagDamperOpen |
                      kExtFlagSensorFault | kExtFlagAirflowFault |
                      kExtFlagFeedbackFault;
  TEST_ASSERT_EQUAL_UINT16(expected, ExtEncodeFlags(telemetry));
}

void test_the_hydraulic_bit_reports_absence_not_presence(void)
{
  ExtensionTelemetry telemetry;

  telemetry.hydraulic_online = false;
  TEST_ASSERT_TRUE(ExtEncodeFlags(telemetry) & kExtFlagHydraulicOff);

  telemetry.hydraulic_online = true;
  TEST_ASSERT_FALSE(ExtEncodeFlags(telemetry) & kExtFlagHydraulicOff);
}

void test_the_v3_flag_bits_kept_their_places(void)
{
  // A decoder written for the radio frame reads bits 0..7 unchanged; only bit 8
  // is new. Pinning the values down stops a reordering going unnoticed.
  TEST_ASSERT_EQUAL_UINT16(1 << 0, kExtFlagRunning);
  TEST_ASSERT_EQUAL_UINT16(1 << 1, kExtFlagFan);
  TEST_ASSERT_EQUAL_UINT16(1 << 2, kExtFlagElectric);
  TEST_ASSERT_EQUAL_UINT16(1 << 3, kExtFlagHydraulic);
  TEST_ASSERT_EQUAL_UINT16(1 << 4, kExtFlagDamperOpen);
  TEST_ASSERT_EQUAL_UINT16(1 << 5, kExtFlagSensorFault);
  TEST_ASSERT_EQUAL_UINT16(1 << 6, kExtFlagHydraulicOff);
  TEST_ASSERT_EQUAL_UINT16(1 << 7, kExtFlagAirflowFault);
  TEST_ASSERT_EQUAL_UINT16(1 << 8, kExtFlagFeedbackFault);
}

// --- Telemetry block --------------------------------------------------------

void test_the_block_carries_its_protocol_version(void)
{
  ExtensionTelemetry telemetry;
  uint16_t block[EXT_TELEMETRY_COUNT];

  ExtEncodeTelemetry(telemetry, block);

  TEST_ASSERT_EQUAL_UINT16(EXT_PROTOCOL_VERSION, block[kExtRegVersion]);
}

void test_elapsed_seconds_split_across_two_registers(void)
{
  ExtensionTelemetry telemetry;
  telemetry.session_elapsed_s = 0x0001E240; // 123456 s, well past 16 bits
  telemetry.uptime_s = 0xDEADBEEF;

  uint16_t block[EXT_TELEMETRY_COUNT];
  ExtEncodeTelemetry(telemetry, block);

  uint32_t elapsed = (static_cast<uint32_t>(block[kExtRegElapsedHigh]) << 16) |
                     block[kExtRegElapsedLow];
  uint32_t uptime = (static_cast<uint32_t>(block[kExtRegUptimeHigh]) << 16) |
                    block[kExtRegUptimeLow];

  TEST_ASSERT_EQUAL_UINT32(123456u, elapsed);
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, uptime);
}

void test_a_signed_temperature_survives_the_unsigned_register(void)
{
  ExtensionTelemetry telemetry;
  telemetry.water_temperature = -3.2f;

  uint16_t block[EXT_TELEMETRY_COUNT];
  ExtEncodeTelemetry(telemetry, block);

  float decoded = ExtDecodeValue(static_cast<int16_t>(block[kExtRegWaterTemp]));
  TEST_ASSERT_EQUAL_FLOAT(-3.2f, decoded);
}

void test_an_untouched_telemetry_reports_every_reading_as_missing(void)
{
  // Default-constructed means "the dryer has nothing to say yet", not "zero
  // everywhere".
  ExtensionTelemetry telemetry;
  uint16_t block[EXT_TELEMETRY_COUNT];

  ExtEncodeTelemetry(telemetry, block);

  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(kExtInvalidValue), block[kExtRegInletTemp]);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(kExtInvalidValue), block[kExtRegTankTemp]);
  TEST_ASSERT_EQUAL_UINT16(kExtNoPosition, block[kExtRegExtractionPos]);
  TEST_ASSERT_EQUAL_UINT16(kExtNoPosition, block[kExtRegRecyclingPos]);
}

void test_the_acknowledgement_rides_in_the_telemetry_block(void)
{
  // No separate write answers a command; this is how the module learns.
  ExtensionTelemetry telemetry;
  telemetry.ack_sequence = 77;
  telemetry.ack_result = kExtResultRefused;

  uint16_t block[EXT_TELEMETRY_COUNT];
  ExtEncodeTelemetry(telemetry, block);

  TEST_ASSERT_EQUAL_UINT16(77, block[kExtRegAckSequence]);
  TEST_ASSERT_EQUAL_UINT16(kExtResultRefused, block[kExtRegAckResult]);
}

// --- Command mailbox --------------------------------------------------------

static void BuildMailbox(uint16_t *mailbox, uint16_t sequence, uint16_t opcode,
                         int16_t argument, uint16_t version = EXT_PROTOCOL_VERSION)
{
  mailbox[kExtCmdRegSequence] = sequence;
  mailbox[kExtCmdRegOpcode]   = opcode;
  mailbox[kExtCmdRegArgument] = static_cast<uint16_t>(argument);
  mailbox[kExtCmdRegVersion]  = version;
}

void test_an_empty_mailbox_yields_nothing_to_do(void)
{
  uint16_t mailbox[EXT_COMMAND_COUNT] = {0, 0, 0, 0};
  ExtensionCommand command;

  // Every register zero is what a module that has written nothing looks like:
  // no command, and no version complaint either.
  TEST_ASSERT_EQUAL_UINT16(kExtResultOk, ExtDecodeCommand(mailbox, command));
  TEST_ASSERT_EQUAL_UINT16(kExtCmdNone, command.opcode);
  TEST_ASSERT_EQUAL_UINT16(0, command.sequence);
}

void test_a_sequence_without_an_opcode_is_no_command(void)
{
  uint16_t mailbox[EXT_COMMAND_COUNT];
  BuildMailbox(mailbox, 5, kExtCmdNone, 0);
  ExtensionCommand command;

  TEST_ASSERT_EQUAL_UINT16(kExtResultOk, ExtDecodeCommand(mailbox, command));
  TEST_ASSERT_EQUAL_UINT16(kExtCmdNone, command.opcode);
}

void test_a_stop_is_accepted(void)
{
  uint16_t mailbox[EXT_COMMAND_COUNT];
  BuildMailbox(mailbox, 12, kExtCmdStop, 0);
  ExtensionCommand command;

  TEST_ASSERT_EQUAL_UINT16(kExtResultOk, ExtDecodeCommand(mailbox, command));
  TEST_ASSERT_EQUAL_UINT16(kExtCmdStop, command.opcode);
  TEST_ASSERT_EQUAL_UINT16(12, command.sequence);
}

void test_a_start_is_understood_and_refused(void)
{
  // Refused by policy, not by ignorance: the module gets kExtResultRefused so
  // its author knows the opcode exists and will never be honoured.
  uint16_t mailbox[EXT_COMMAND_COUNT];
  BuildMailbox(mailbox, 13, kExtCmdStart, 0);
  ExtensionCommand command;

  TEST_ASSERT_EQUAL_UINT16(kExtResultRefused, ExtDecodeCommand(mailbox, command));
  TEST_ASSERT_EQUAL_UINT16(13, command.sequence);
}

void test_a_setpoint_inside_the_menu_range_is_accepted(void)
{
  uint16_t mailbox[EXT_COMMAND_COUNT];
  BuildMailbox(mailbox, 14, kExtCmdSetTemp, 385); // 38.5 C
  ExtensionCommand command;

  TEST_ASSERT_EQUAL_UINT16(kExtResultOk, ExtDecodeCommand(mailbox, command));
  TEST_ASSERT_EQUAL_FLOAT(38.5f, command.argument);
}

void test_a_setpoint_outside_the_menu_range_is_refused(void)
{
  // The remote path honours the same limits the menu enforces, rather than
  // clamping silently and reporting success for a value nobody asked for.
  uint16_t mailbox[EXT_COMMAND_COUNT];
  ExtensionCommand command;

  BuildMailbox(mailbox, 15, kExtCmdSetTemp, 900); // 90 C, past TARGET_TEMP_MAX
  TEST_ASSERT_EQUAL_UINT16(kExtResultOutOfRange, ExtDecodeCommand(mailbox, command));

  BuildMailbox(mailbox, 16, kExtCmdSetTemp, 50); // 5 C, below TARGET_TEMP_MIN
  TEST_ASSERT_EQUAL_UINT16(kExtResultOutOfRange, ExtDecodeCommand(mailbox, command));

  BuildMailbox(mailbox, 17, kExtCmdSetHumidity, 1200); // 120 %RH
  TEST_ASSERT_EQUAL_UINT16(kExtResultOutOfRange, ExtDecodeCommand(mailbox, command));
}

void test_a_setpoint_carrying_the_sentinel_is_refused(void)
{
  uint16_t mailbox[EXT_COMMAND_COUNT];
  BuildMailbox(mailbox, 18, kExtCmdSetTemp, kExtInvalidValue);
  ExtensionCommand command;

  TEST_ASSERT_EQUAL_UINT16(kExtResultOutOfRange, ExtDecodeCommand(mailbox, command));
}

void test_an_unknown_opcode_is_reported_as_such(void)
{
  uint16_t mailbox[EXT_COMMAND_COUNT];
  BuildMailbox(mailbox, 19, 99, 0);
  ExtensionCommand command;

  TEST_ASSERT_EQUAL_UINT16(kExtResultUnknownOpcode, ExtDecodeCommand(mailbox, command));
}

void test_a_module_speaking_another_version_is_told_so(void)
{
  uint16_t mailbox[EXT_COMMAND_COUNT];
  BuildMailbox(mailbox, 20, kExtCmdStop, 0, EXT_PROTOCOL_VERSION + 1);
  ExtensionCommand command;

  TEST_ASSERT_EQUAL_UINT16(kExtResultBadVersion, ExtDecodeCommand(mailbox, command));
}

void test_a_refused_command_still_carries_its_sequence(void)
{
  // The acknowledgement has to name the command being refused, or the module
  // repeats it forever waiting for an answer that never matches.
  uint16_t mailbox[EXT_COMMAND_COUNT];
  BuildMailbox(mailbox, 21, kExtCmdSetTemp, 900);
  ExtensionCommand command;

  ExtDecodeCommand(mailbox, command);
  TEST_ASSERT_EQUAL_UINT16(21, command.sequence);
}

// --- Replay filter ----------------------------------------------------------

void test_a_resident_command_executes_once(void)
{
  // The mailbox is re-read every cycle and keeps the same command in it. Without
  // the filter, a stop would fire every two seconds forever.
  ExtensionCommandFilter filter;

  TEST_ASSERT_TRUE(filter.ShouldExecute(7));
  filter.MarkExecuted(7);
  TEST_ASSERT_FALSE(filter.ShouldExecute(7));
  TEST_ASSERT_FALSE(filter.ShouldExecute(7));
}

void test_a_new_sequence_executes_again(void)
{
  ExtensionCommandFilter filter;

  filter.MarkExecuted(7);
  TEST_ASSERT_TRUE(filter.ShouldExecute(8));
}

void test_an_empty_mailbox_never_executes(void)
{
  ExtensionCommandFilter filter;

  TEST_ASSERT_FALSE(filter.ShouldExecute(0));
  filter.MarkExecuted(7);
  TEST_ASSERT_FALSE(filter.ShouldExecute(0));
}

void test_a_sequence_wrapping_past_zero_is_accepted(void)
{
  // A module counting up in a 16-bit register eventually rolls over. 65535 then
  // 1 is a new command, not a replay — and 0 in between is simply skipped.
  ExtensionCommandFilter filter;

  filter.MarkExecuted(65535);
  TEST_ASSERT_FALSE(filter.ShouldExecute(0));
  TEST_ASSERT_TRUE(filter.ShouldExecute(1));
}

void test_a_reset_forgets_what_ran(void)
{
  ExtensionCommandFilter filter;

  filter.MarkExecuted(7);
  filter.Reset();

  TEST_ASSERT_TRUE(filter.ShouldExecute(7));
  TEST_ASSERT_EQUAL_UINT16(0, filter.GetLastSequence());
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_a_missing_reading_encodes_to_the_sentinel);
  RUN_TEST(test_a_real_zero_is_not_the_sentinel);
  RUN_TEST(test_values_round_trip_through_tenths);
  RUN_TEST(test_negative_values_survive);
  RUN_TEST(test_a_wild_reading_cannot_impersonate_the_sentinel);

  RUN_TEST(test_an_unknown_opening_encodes_to_its_own_sentinel);
  RUN_TEST(test_both_ends_of_the_opening_range_stay_legal);
  RUN_TEST(test_a_drifting_feedback_is_clamped_not_wrapped);

  RUN_TEST(test_each_flag_sits_on_its_own_bit);
  RUN_TEST(test_the_hydraulic_bit_reports_absence_not_presence);
  RUN_TEST(test_the_v3_flag_bits_kept_their_places);

  RUN_TEST(test_the_block_carries_its_protocol_version);
  RUN_TEST(test_elapsed_seconds_split_across_two_registers);
  RUN_TEST(test_a_signed_temperature_survives_the_unsigned_register);
  RUN_TEST(test_an_untouched_telemetry_reports_every_reading_as_missing);
  RUN_TEST(test_the_acknowledgement_rides_in_the_telemetry_block);

  RUN_TEST(test_an_empty_mailbox_yields_nothing_to_do);
  RUN_TEST(test_a_sequence_without_an_opcode_is_no_command);
  RUN_TEST(test_a_stop_is_accepted);
  RUN_TEST(test_a_start_is_understood_and_refused);
  RUN_TEST(test_a_setpoint_inside_the_menu_range_is_accepted);
  RUN_TEST(test_a_setpoint_outside_the_menu_range_is_refused);
  RUN_TEST(test_a_setpoint_carrying_the_sentinel_is_refused);
  RUN_TEST(test_an_unknown_opcode_is_reported_as_such);
  RUN_TEST(test_a_module_speaking_another_version_is_told_so);
  RUN_TEST(test_a_refused_command_still_carries_its_sequence);

  RUN_TEST(test_a_resident_command_executes_once);
  RUN_TEST(test_a_new_sequence_executes_again);
  RUN_TEST(test_an_empty_mailbox_never_executes);
  RUN_TEST(test_a_sequence_wrapping_past_zero_is_accepted);
  RUN_TEST(test_a_reset_forgets_what_ran);

  return UNITY_END();
}
