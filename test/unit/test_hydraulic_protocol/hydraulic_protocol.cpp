// Unit tests for the hydraulic wire format.
//
// This header exists in two repositories at once and nothing on the wire can
// detect a disagreement between the copies: a Modbus frame carries no schema,
// so a register read with the wrong meaning arrives looking perfectly healthy.
// The round trips below are what stands in for that missing check — if encode
// and decode agree here, and both repositories hold the same file, then both
// ends agree.
//
// Three things are worth pinning down beyond the round trip: that the "no
// reading" sentinel survives, that no real value can ever collide with it, and
// that the status bits keep the positions the dryer reads them at.

#include <string.h>
#include <unity.h>
#include "HydraulicProtocol.h"

namespace
{

// A reading and its expected raw encoding, for the tenths format.
struct ValueCase
{
  float   celsius;
  int16_t raw;
};

} // namespace

// ---------------------------------------------------------------------------
// Tenths encoding

void test_value_round_trip(void)
{
  const ValueCase cases[] = {
      {0.0f, 0},      // a real zero, which must not look like a missing probe
      {40.0f, 400},
      {55.5f, 555},
      {-5.0f, -50},   // the water loop can legitimately read below freezing
      {-0.1f, -1},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    int16_t raw = HydroEncodeValue(cases[i].celsius);
    TEST_ASSERT_EQUAL_INT16(cases[i].raw, raw);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, cases[i].celsius, HydroDecodeValue(raw));
  }
}

void test_nan_encodes_to_the_sentinel(void)
{
  TEST_ASSERT_EQUAL_INT16(kHydroInvalidValue, HydroEncodeValue(NAN));
  TEST_ASSERT_TRUE(isnan(HydroDecodeValue(kHydroInvalidValue)));
}

// Zero is a temperature, not an absence. This is the confusion the sentinel
// exists to prevent and the one the register map lacked before this revision.
void test_zero_is_not_the_sentinel(void)
{
  TEST_ASSERT_EQUAL_INT16(0, HydroEncodeValue(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, HydroDecodeValue(0));
}

// A probe reading nonsense must not be able to announce itself as absent.
void test_extreme_values_clamp_clear_of_the_sentinel(void)
{
  int16_t very_cold = HydroEncodeValue(-40000.0f);
  TEST_ASSERT_NOT_EQUAL(kHydroInvalidValue, very_cold);
  TEST_ASSERT_EQUAL_INT16(kHydroInvalidValue + 1, very_cold);

  TEST_ASSERT_EQUAL_INT16(INT16_MAX, HydroEncodeValue(40000.0f));
}

// ---------------------------------------------------------------------------
// Pump speed

void test_speed_round_trip(void)
{
  TEST_ASSERT_EQUAL_UINT16(0, HydroEncodeSpeed(0.0f));
  TEST_ASSERT_EQUAL_UINT16(75, HydroEncodeSpeed(75.0f));
  TEST_ASSERT_EQUAL_UINT16(100, HydroEncodeSpeed(100.0f));

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 75.0f, HydroDecodeSpeed(75));
}

void test_unknown_speed_uses_its_own_sentinel(void)
{
  TEST_ASSERT_EQUAL_UINT16(kHydroNoSpeed, HydroEncodeSpeed(NAN));
  TEST_ASSERT_TRUE(isnan(HydroDecodeSpeed(kHydroNoSpeed)));
}

// 0 % is a stopped pump and 100 % a running one; both are legal readings, so
// the sentinel has to sit outside the range rather than at either end of it.
void test_speed_clamps_inside_the_range(void)
{
  TEST_ASSERT_EQUAL_UINT16(0, HydroEncodeSpeed(-20.0f));
  TEST_ASSERT_EQUAL_UINT16(100, HydroEncodeSpeed(180.0f));
  TEST_ASSERT_NOT_EQUAL(kHydroNoSpeed, HydroEncodeSpeed(180.0f));
}

// ---------------------------------------------------------------------------
// Status bits

// The bit positions are the contract. Asserting the packed word rather than
// re-deriving it from the enum is deliberate: a test that computes the expected
// value the same way the code does would pass through a renumbering that broke
// the dryer.
void test_status_bit_positions(void)
{
  HydraulicTelemetry telemetry;

  telemetry.circulating = true;
  TEST_ASSERT_EQUAL_UINT16(0x0001, HydroEncodeStatus(telemetry));

  telemetry.circulating = false;
  telemetry.tank_too_cold = true;
  TEST_ASSERT_EQUAL_UINT16(0x0004, HydroEncodeStatus(telemetry));

  telemetry.tank_too_cold = false;
  telemetry.watchdog_tripped = true;
  TEST_ASSERT_EQUAL_UINT16(0x0040, HydroEncodeStatus(telemetry));
}

void test_status_round_trip(void)
{
  HydraulicTelemetry sent;
  sent.circulating       = true;
  sent.permission        = true;
  sent.tank_too_cold     = false;
  sent.water_probe_fault = false;
  sent.tank_probe_fault  = true;
  sent.watchdog_tripped  = false;
  sent.setpoint_missed   = true;

  HydraulicTelemetry received;
  HydroDecodeStatus(HydroEncodeStatus(sent), received);

  TEST_ASSERT_TRUE(received.circulating);
  TEST_ASSERT_TRUE(received.permission);
  TEST_ASSERT_FALSE(received.tank_too_cold);
  TEST_ASSERT_FALSE(received.water_probe_fault);
  TEST_ASSERT_TRUE(received.tank_probe_fault);
  TEST_ASSERT_FALSE(received.watchdog_tripped);
  TEST_ASSERT_TRUE(received.setpoint_missed);
}

// ---------------------------------------------------------------------------
// Whole blocks

void test_command_block_round_trip(void)
{
  HydraulicCommand sent;
  sent.run_permitted         = true;
  sent.dryer_air_temperature = 38.5f;

  uint16_t registers[HYDRO_COMMAND_COUNT];
  HydroEncodeCommand(sent, registers);

  // Two registers, not three: the water setpoint left the block when the module
  // took ownership of the loop it belongs to, and the air temperature moved
  // down into the hole rather than a reserved word being left behind.
  TEST_ASSERT_EQUAL_UINT16(2, HYDRO_COMMAND_COUNT);
  TEST_ASSERT_EQUAL_UINT16(1, registers[kHydroCmdRegState]);
  TEST_ASSERT_EQUAL_UINT16(385, registers[kHydroCmdRegDryerAirTemp]);

  HydraulicCommand received;
  HydroDecodeCommand(registers, received);

  TEST_ASSERT_TRUE(received.run_permitted);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 38.5f, received.dryer_air_temperature);
}

// A dryer that has no fresh air reading says so, and the module has to be able
// to tell that from a dryer reporting 0 C — the interlock treats the two
// completely differently.
void test_command_carries_an_absent_air_temperature(void)
{
  HydraulicCommand sent;
  sent.run_permitted         = true;
  sent.dryer_air_temperature = NAN;

  uint16_t registers[HYDRO_COMMAND_COUNT];
  HydroEncodeCommand(sent, registers);

  HydraulicCommand received;
  HydroDecodeCommand(registers, received);

  TEST_ASSERT_TRUE(received.run_permitted);
  TEST_ASSERT_TRUE(isnan(received.dryer_air_temperature));
}

void test_telemetry_block_round_trip(void)
{
  HydraulicTelemetry sent;
  sent.water_temperature      = 48.2f;
  sent.tank_temperature       = 62.4f;
  sent.pump_speed_percent     = 75.0f;
  sent.circulating            = true;
  sent.permission             = true;

  uint16_t registers[HYDRO_TELEMETRY_COUNT];
  HydroEncodeTelemetry(sent, registers);

  HydraulicTelemetry received;
  HydroDecodeTelemetry(registers, received);

  TEST_ASSERT_FLOAT_WITHIN(0.05f, 48.2f, received.water_temperature);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 62.4f, received.tank_temperature);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 75.0f, received.pump_speed_percent);
  TEST_ASSERT_TRUE(received.circulating);
  TEST_ASSERT_TRUE(received.permission);
}

// A module that has not read its probes yet reports dashes, not zeroes. The
// dryer's screen shows those dashes; showing 0.0 C instead would be a fault
// report nobody could distinguish from a very cold morning.
void test_telemetry_carries_absent_readings(void)
{
  HydraulicTelemetry sent; // default-constructed: every reading NAN

  uint16_t registers[HYDRO_TELEMETRY_COUNT];
  HydroEncodeTelemetry(sent, registers);

  HydraulicTelemetry received;
  HydroDecodeTelemetry(registers, received);

  TEST_ASSERT_TRUE(isnan(received.water_temperature));
  TEST_ASSERT_TRUE(isnan(received.tank_temperature));
  TEST_ASSERT_TRUE(isnan(received.pump_speed_percent));
}

// The block sizes are what the dryer's FC16 and FC03 counts are written
// against. A register added without the count moving would be written by one
// side and never read by the other.
void test_block_sizes_match_the_register_map(void)
{
  TEST_ASSERT_EQUAL_UINT8(2, HYDRO_COMMAND_COUNT);
  TEST_ASSERT_EQUAL_UINT8(4, HYDRO_TELEMETRY_COUNT);

  // The command block is contiguous too, which is why removing the water
  // setpoint from the middle of it moved the air temperature down rather than
  // leaving 0x0001 reserved: the dryer writes the whole block with one FC16,
  // and a hole would be a register written every cycle and read by nobody.
  TEST_ASSERT_EQUAL_UINT8(0, kHydroCmdRegState);
  TEST_ASSERT_EQUAL_UINT8(1, kHydroCmdRegDryerAirTemp);
  TEST_ASSERT_EQUAL_UINT16(HYDRO_REG_STATE + 1, HYDRO_REG_DRYER_AIR_TEMP);

  TEST_ASSERT_EQUAL_UINT8(HYDRO_COMMAND_COUNT - 1, kHydroCmdRegDryerAirTemp);
  TEST_ASSERT_EQUAL_UINT8(HYDRO_TELEMETRY_COUNT - 1, kHydroRegPumpSpeed);

  // And the telemetry block starts where the dryer reads it from.
  TEST_ASSERT_EQUAL_UINT16(0x0010, HYDRO_REG_WATER_TEMP);
  TEST_ASSERT_EQUAL_UINT16(0x0000, HYDRO_REG_STATE);

  // The block is contiguous: every address between the base and the count is a
  // register somebody encodes. A hole here would be read by the dryer's one
  // FC03 and decoded as a reading.
  TEST_ASSERT_EQUAL_UINT16(HYDRO_REG_WATER_TEMP + kHydroRegPumpSpeed,
                           HYDRO_REG_PUMP_SPEED);
}

// Bit 5 was the fake-probe fault, from when the module synthesised a probe for
// the three-way valve. It is retired, and the bits above it deliberately did
// not move down to close the gap — the dryer's HARDWARE.md tabulates them, and
// somebody reads that table off a bench with a status word in front of them.
//
// This is the test that keeps the paper and the firmware agreeing: renumbering
// would pass every other test in this file.
void test_retired_bit_five_stays_vacant(void)
{
  HydraulicTelemetry telemetry;
  telemetry.circulating       = true;
  telemetry.permission        = true;
  telemetry.tank_too_cold     = true;
  telemetry.water_probe_fault = true;
  telemetry.tank_probe_fault  = true;
  telemetry.watchdog_tripped  = true;
  telemetry.setpoint_missed   = true;

  // Every flag there is, and bit 5 still reads back as zero.
  const uint16_t bits = HydroEncodeStatus(telemetry);
  TEST_ASSERT_EQUAL_UINT16(0x00DF, bits);
  TEST_ASSERT_EQUAL_UINT16(0, bits & (1 << 5));

  TEST_ASSERT_EQUAL_UINT16(0x0040, kHydroFlagWatchdogTripped);
  TEST_ASSERT_EQUAL_UINT16(0x0080, kHydroFlagSetpointMissed);
}

int main(int, char **)
{
  UNITY_BEGIN();

  RUN_TEST(test_value_round_trip);
  RUN_TEST(test_nan_encodes_to_the_sentinel);
  RUN_TEST(test_zero_is_not_the_sentinel);
  RUN_TEST(test_extreme_values_clamp_clear_of_the_sentinel);

  RUN_TEST(test_speed_round_trip);
  RUN_TEST(test_unknown_speed_uses_its_own_sentinel);
  RUN_TEST(test_speed_clamps_inside_the_range);

  RUN_TEST(test_status_bit_positions);
  RUN_TEST(test_status_round_trip);
  RUN_TEST(test_retired_bit_five_stays_vacant);

  RUN_TEST(test_command_block_round_trip);
  RUN_TEST(test_command_carries_an_absent_air_temperature);
  RUN_TEST(test_telemetry_block_round_trip);
  RUN_TEST(test_telemetry_carries_absent_readings);
  RUN_TEST(test_block_sizes_match_the_register_map);

  return UNITY_END();
}
