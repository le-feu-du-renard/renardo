// Unit tests for the extension port wire format.
//
// ExtensionPort itself needs the bus, so what is tested here is everything
// that decides what the module sees and what the dryer will act on: the
// sentinels that keep a missing reading distinct from a real zero, the
// generic {id, value} envelope, the validation that refuses a command, and
// the filter that stops a resident mailbox being replayed forever.

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

// --- The generic {id, value} table ------------------------------------------

constexpr uint16_t kMetricA = 3;
constexpr uint16_t kMetricB = 5;

void test_a_metric_id_carries_its_kind_in_the_top_bit(void)
{
  ExtensionTelemetryRecord record;
  ExtPutMetricValue(record, kMetricA, 12.3f);

  TEST_ASSERT_EQUAL_UINT8(1, record.metric_count);
  TEST_ASSERT_EQUAL_UINT16(kMetricA, record.metrics[0].metric_id & kExtMetricIdMask);
  TEST_ASSERT_EQUAL_UINT16(kExtMetricKindTenths, record.metrics[0].metric_id & kExtMetricKindMask);
}

void test_a_counter_metric_is_marked_as_such(void)
{
  ExtensionTelemetryRecord record;
  ExtPutMetricCounter(record, kMetricB, 90061);

  TEST_ASSERT_EQUAL_UINT16(kMetricB, record.metrics[0].metric_id & kExtMetricIdMask);
  TEST_ASSERT_EQUAL_UINT16(kExtMetricKindCounter, record.metrics[0].metric_id & kExtMetricKindMask);
  // Whole units, wraps at 65536 — 90061 truncates.
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(90061 & 0xFFFF), record.metrics[0].value);
}

void test_putting_past_the_limit_is_refused(void)
{
  ExtensionTelemetryRecord record;
  for (uint16_t i = 0; i < kExtMaxMetrics; i++)
  {
    TEST_ASSERT_TRUE(ExtPutMetricValue(record, i, 1.0f));
  }
  TEST_ASSERT_FALSE(ExtPutMetricValue(record, 999, 1.0f));
  TEST_ASSERT_EQUAL_UINT8(kExtMaxMetrics, record.metric_count);
}

// --- Telemetry block --------------------------------------------------------

void test_the_block_carries_its_protocol_version(void)
{
  ExtensionTelemetryRecord telemetry;
  uint16_t block[kExtTelemetryMaxCount];

  ExtEncodeTelemetry(telemetry, block);

  TEST_ASSERT_EQUAL_UINT16(EXT_PROTOCOL_VERSION, block[kExtRegVersion]);
}

void test_uptime_is_split_across_two_header_registers(void)
{
  ExtensionTelemetryRecord telemetry;
  telemetry.uptime_s = 0xDEADBEEF;

  uint16_t block[kExtTelemetryMaxCount];
  ExtEncodeTelemetry(telemetry, block);

  uint32_t uptime = (static_cast<uint32_t>(block[kExtRegUptimeHigh]) << 16) |
                    block[kExtRegUptimeLow];
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, uptime);
}

void test_the_block_length_grows_with_the_metric_count(void)
{
  ExtensionTelemetryRecord telemetry;
  ExtPutMetricValue(telemetry, 0, 1.0f);
  ExtPutMetricValue(telemetry, 1, 2.0f);

  uint16_t block[kExtTelemetryMaxCount];
  const size_t count = ExtEncodeTelemetry(telemetry, block);

  TEST_ASSERT_EQUAL_UINT32(kExtTelemetryHeaderCount + 4, count);
  TEST_ASSERT_EQUAL_UINT16(2, block[kExtRegCount]);
}

void test_a_signed_temperature_survives_the_unsigned_register(void)
{
  ExtensionTelemetryRecord telemetry;
  ExtPutMetricValue(telemetry, kMetricA, -3.2f);

  uint16_t block[kExtTelemetryMaxCount];
  ExtEncodeTelemetry(telemetry, block);

  float decoded = ExtDecodeValue(static_cast<int16_t>(block[kExtTelemetryHeaderCount + 1]));
  TEST_ASSERT_EQUAL_FLOAT(-3.2f, decoded);
}

void test_an_untouched_telemetry_carries_no_metrics(void)
{
  // Default-constructed means "nothing put yet", not "zero everywhere".
  ExtensionTelemetryRecord telemetry;
  uint16_t block[kExtTelemetryMaxCount];

  const size_t count = ExtEncodeTelemetry(telemetry, block);

  TEST_ASSERT_EQUAL_UINT32(kExtTelemetryHeaderCount, count);
  TEST_ASSERT_EQUAL_UINT16(0, block[kExtRegCount]);
}

void test_the_acknowledgement_rides_in_the_telemetry_block(void)
{
  // No separate write answers a command; this is how the module learns.
  ExtensionTelemetryRecord telemetry;
  telemetry.ack_sequence = 77;
  telemetry.ack_result = kExtResultRefused;

  uint16_t block[kExtTelemetryMaxCount];
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

  RUN_TEST(test_a_metric_id_carries_its_kind_in_the_top_bit);
  RUN_TEST(test_a_counter_metric_is_marked_as_such);
  RUN_TEST(test_putting_past_the_limit_is_refused);

  RUN_TEST(test_the_block_carries_its_protocol_version);
  RUN_TEST(test_uptime_is_split_across_two_header_registers);
  RUN_TEST(test_the_block_length_grows_with_the_metric_count);
  RUN_TEST(test_a_signed_temperature_survives_the_unsigned_register);
  RUN_TEST(test_an_untouched_telemetry_carries_no_metrics);
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
