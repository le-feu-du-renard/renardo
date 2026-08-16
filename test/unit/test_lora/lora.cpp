// Unit tests for the LoRa wire format.
//
// Over the air there is no transport to lean on. A frame can arrive corrupted,
// duplicated, addressed to another dryer, or long after it stopped being
// relevant — and every one of those has to be rejected rather than obeyed,
// because obeying means starting or stopping a machine.

#include <unity.h>
#include <math.h>
#include <string.h>

#include "LoraProtocol.h"

void setUp(void) {}
void tearDown(void) {}

// --- Value encoding ---------------------------------------------------------

void test_values_round_trip_through_tenths(void)
{
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 38.2f, LoraDecodeValue(LoraEncodeValue(38.2f)));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, LoraDecodeValue(LoraEncodeValue(0.0f)));
}

void test_negative_values_survive(void)
{
  // The water loop can legitimately be below zero; a sign lost here would be
  // reported to the server as a plausible warm reading.
  TEST_ASSERT_FLOAT_WITHIN(0.05f, -7.3f, LoraDecodeValue(LoraEncodeValue(-7.3f)));
}

void test_missing_reading_is_distinguishable_from_zero(void)
{
  int16_t encoded = LoraEncodeValue(NAN);
  TEST_ASSERT_EQUAL_INT16(kLoraInvalidValue, encoded);
  TEST_ASSERT_TRUE(isnan(LoraDecodeValue(encoded)));

  // A real zero must not collide with the sentinel.
  TEST_ASSERT_NOT_EQUAL(kLoraInvalidValue, LoraEncodeValue(0.0f));
}

void test_out_of_range_values_are_clamped_not_wrapped(void)
{
  // A wrapped value would arrive as a believable reading of the wrong sign.
  TEST_ASSERT_TRUE(LoraEncodeValue(100000.0f) > 0);
  TEST_ASSERT_TRUE(LoraEncodeValue(-100000.0f) < 0);
  TEST_ASSERT_NOT_EQUAL(kLoraInvalidValue, LoraEncodeValue(-100000.0f));
}

// --- Framing ----------------------------------------------------------------

void test_sealed_telemetry_validates(void)
{
  TelemetryPacket packet;
  memset(&packet, 0, sizeof(packet));
  packet.device_id = 1;
  packet.inlet_temperature = LoraEncodeValue(38.2f);
  SealTelemetry(packet);

  TEST_ASSERT_TRUE(IsTelemetryValid(packet));
}

void test_corrupt_telemetry_is_rejected(void)
{
  TelemetryPacket packet;
  memset(&packet, 0, sizeof(packet));
  SealTelemetry(packet);

  uint8_t *bytes = reinterpret_cast<uint8_t *>(&packet);
  bytes[6] ^= 0xFF;

  TEST_ASSERT_FALSE(IsTelemetryValid(packet));
}

void test_sealed_command_validates(void)
{
  CommandPacket packet;
  memset(&packet, 0, sizeof(packet));
  packet.device_id = 7;
  packet.sequence  = 3;
  packet.command   = kLoraCommandStart;
  SealCommand(packet);

  TEST_ASSERT_TRUE(IsCommandValid(packet, 7));
}

void test_command_for_another_dryer_is_ignored(void)
{
  // Several dryers can share the band; obeying a neighbour's START would run a
  // machine nobody asked for.
  CommandPacket packet;
  memset(&packet, 0, sizeof(packet));
  packet.device_id = 2;
  packet.command   = kLoraCommandStop;
  SealCommand(packet);

  TEST_ASSERT_TRUE(IsCommandValid(packet, 2));
  TEST_ASSERT_FALSE(IsCommandValid(packet, 1));
}

void test_command_with_wrong_magic_is_ignored(void)
{
  CommandPacket packet;
  memset(&packet, 0, sizeof(packet));
  packet.device_id = 1;
  SealCommand(packet);
  packet.magic = 0x00;

  TEST_ASSERT_FALSE(IsCommandValid(packet, 1));
}

void test_command_from_a_future_protocol_is_ignored(void)
{
  CommandPacket packet;
  memset(&packet, 0, sizeof(packet));
  packet.device_id = 1;
  SealCommand(packet);
  packet.version = LORA_PROTOCOL_VERSION + 1;
  packet.checksum = LoraChecksum(&packet, offsetof(CommandPacket, checksum));

  TEST_ASSERT_FALSE(IsCommandValid(packet, 1));
}

// --- Replay filtering -------------------------------------------------------

void test_first_command_is_executed(void)
{
  LoraCommandFilter filter;
  TEST_ASSERT_TRUE(filter.ShouldExecute(42));
  TEST_ASSERT_EQUAL_UINT8(42, filter.GetLastSequence());
}

void test_repeated_command_is_executed_once(void)
{
  // The Commander repeats until it sees the acknowledgement, so the same frame
  // arrives several times as a matter of course.
  LoraCommandFilter filter;
  TEST_ASSERT_TRUE(filter.ShouldExecute(10));
  TEST_ASSERT_FALSE(filter.ShouldExecute(10));
  TEST_ASSERT_FALSE(filter.ShouldExecute(10));
}

void test_newer_commands_are_executed(void)
{
  LoraCommandFilter filter;
  TEST_ASSERT_TRUE(filter.ShouldExecute(10));
  TEST_ASSERT_TRUE(filter.ShouldExecute(11));
  TEST_ASSERT_TRUE(filter.ShouldExecute(15));
}

void test_late_arriving_older_command_is_dropped(void)
{
  // A superseded "set temperature" that turns up late must not undo the newer
  // one that already took effect.
  LoraCommandFilter filter;
  TEST_ASSERT_TRUE(filter.ShouldExecute(20));
  TEST_ASSERT_FALSE(filter.ShouldExecute(19));
  TEST_ASSERT_FALSE(filter.ShouldExecute(5));
  TEST_ASSERT_EQUAL_UINT8(20, filter.GetLastSequence());
}

void test_sequence_wrap_does_not_freeze_the_link(void)
{
  // Sequence numbers wrap at 256. Comparing them as plain integers would make
  // everything after the rollover look older, and the dryer would stop obeying
  // the Commander until the counter caught up again.
  LoraCommandFilter filter;
  TEST_ASSERT_TRUE(filter.ShouldExecute(254));
  TEST_ASSERT_TRUE(filter.ShouldExecute(255));
  TEST_ASSERT_TRUE(filter.ShouldExecute(0));
  TEST_ASSERT_TRUE(filter.ShouldExecute(1));
  TEST_ASSERT_FALSE(filter.ShouldExecute(0));
}

void test_reset_reprimes_the_filter(void)
{
  LoraCommandFilter filter;
  TEST_ASSERT_TRUE(filter.ShouldExecute(100));
  TEST_ASSERT_FALSE(filter.ShouldExecute(100));

  filter.Reset();
  TEST_ASSERT_TRUE(filter.ShouldExecute(100));
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  RUN_TEST(test_values_round_trip_through_tenths);
  RUN_TEST(test_negative_values_survive);
  RUN_TEST(test_missing_reading_is_distinguishable_from_zero);
  RUN_TEST(test_out_of_range_values_are_clamped_not_wrapped);

  RUN_TEST(test_sealed_telemetry_validates);
  RUN_TEST(test_corrupt_telemetry_is_rejected);
  RUN_TEST(test_sealed_command_validates);
  RUN_TEST(test_command_for_another_dryer_is_ignored);
  RUN_TEST(test_command_with_wrong_magic_is_ignored);
  RUN_TEST(test_command_from_a_future_protocol_is_ignored);

  RUN_TEST(test_first_command_is_executed);
  RUN_TEST(test_repeated_command_is_executed_once);
  RUN_TEST(test_newer_commands_are_executed);
  RUN_TEST(test_late_arriving_older_command_is_dropped);
  RUN_TEST(test_sequence_wrap_does_not_freeze_the_link);
  RUN_TEST(test_reset_reprimes_the_filter);

  return UNITY_END();
}
