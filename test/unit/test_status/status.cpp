// Unit tests for StatusIndicator.
//
// The panel LEDs are the only thing about this machine an operator can read
// from across the room, so a state that resolves the wrong way is a lie told at
// a distance. Two things are worth pinning down: the order of priority between
// the four states, and the fact that a freshly booted dryer whose probe has not
// answered yet is not a dryer in fault.

#include <unity.h>
#include "StatusIndicator.h"

namespace
{

// Comfortably past the grace window, so a test that is not about the boot does
// not accidentally exercise it.
constexpr uint32_t kSettled = STATUS_FAULT_GRACE_MS + 1000;

// The two halves of a blink, at a known instant each.
constexpr uint32_t kBeatOn  = 0;                      // (0 / 500) % 2 == 0
constexpr uint32_t kBeatOff = STATUS_BLINK_INTERVAL;  // (500 / 500) % 2 == 1

} // namespace

void test_running_is_steady_green(void)
{
  DryerStatus status = ResolveStatus(true, true, false, kSettled);
  TEST_ASSERT_EQUAL(DryerStatus::kRunning, status);

  // Steady means steady: the same at both ends of a blink period.
  LedPattern on  = PatternFor(status, kSettled + kBeatOn);
  LedPattern off = PatternFor(status, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.green);
  TEST_ASSERT_TRUE(off.green);
  TEST_ASSERT_FALSE(on.red);
  TEST_ASSERT_FALSE(off.red);
}

void test_stopped_is_steady_red(void)
{
  DryerStatus status = ResolveStatus(false, false, false, kSettled);
  TEST_ASSERT_EQUAL(DryerStatus::kStopped, status);

  LedPattern on  = PatternFor(status, kSettled + kBeatOn);
  LedPattern off = PatternFor(status, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.red);
  TEST_ASSERT_TRUE(off.red);
  TEST_ASSERT_FALSE(on.green);
  TEST_ASSERT_FALSE(off.green);
}

// The fan outliving a stopped session is the cooldown, and it has to read
// differently from a dryer at rest: the fan is still turning in there.
void test_fan_running_after_stop_is_cooling(void)
{
  DryerStatus status = ResolveStatus(false, true, false, kSettled);
  TEST_ASSERT_EQUAL(DryerStatus::kCooling, status);

  LedPattern on  = PatternFor(status, kSettled + kBeatOn);
  LedPattern off = PatternFor(status, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.green);
  TEST_ASSERT_FALSE(off.green);
  TEST_ASSERT_FALSE(on.red);
  TEST_ASSERT_FALSE(off.red);
}

void test_fault_blinks_red_with_the_green_out(void)
{
  DryerStatus status = ResolveStatus(false, false, true, kSettled);
  TEST_ASSERT_EQUAL(DryerStatus::kFault, status);

  LedPattern on  = PatternFor(status, kSettled + kBeatOn);
  LedPattern off = PatternFor(status, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.red);
  TEST_ASSERT_FALSE(off.red);
  TEST_ASSERT_FALSE(on.green);
  TEST_ASSERT_FALSE(off.green);
}

// A stale probe blocks the heat sources but not the fan, so the dryer can be
// turning while something is wrong. That is exactly when the panel must say so
// rather than show a reassuring steady green.
void test_fault_wins_over_a_running_session(void)
{
  TEST_ASSERT_EQUAL(DryerStatus::kFault,
                    ResolveStatus(true, true, true, kSettled));
}

void test_fault_wins_over_the_cooldown(void)
{
  TEST_ASSERT_EQUAL(DryerStatus::kCooling,
                    ResolveStatus(false, true, false, kSettled));
  TEST_ASSERT_EQUAL(DryerStatus::kFault,
                    ResolveStatus(false, true, true, kSettled));
}

// The inlet probe and the hydraulic module are both silent until Core 1 has
// completed its first RS485 cycle. Every freshly booted dryer is therefore in
// "fault" by the letter of the test, and must not blink for it.
void test_a_fault_during_the_grace_window_is_not_reported(void)
{
  TEST_ASSERT_EQUAL(DryerStatus::kStopped,
                    ResolveStatus(false, false, true, 0));
  TEST_ASSERT_EQUAL(DryerStatus::kStopped,
                    ResolveStatus(false, false, true, STATUS_FAULT_GRACE_MS - 1000));
  TEST_ASSERT_EQUAL(DryerStatus::kRunning,
                    ResolveStatus(true, true, true, STATUS_FAULT_GRACE_MS - 1000));
}

void test_the_same_fault_is_reported_once_the_window_has_passed(void)
{
  TEST_ASSERT_EQUAL(DryerStatus::kFault,
                    ResolveStatus(false, false, true, STATUS_FAULT_GRACE_MS));
  TEST_ASSERT_EQUAL(DryerStatus::kFault,
                    ResolveStatus(false, false, true, STATUS_FAULT_GRACE_MS + 1000));
}

// No state leaves both LEDs dark: a dead LED or an unpowered board must not
// look like a dryer sitting quietly at rest. The blinking states are exempt
// half the time by definition — what matters is that the dark half is a beat,
// not a state.
void test_no_state_is_ever_fully_dark_on_the_lit_half_of_a_beat(void)
{
  const DryerStatus kAll[] = {DryerStatus::kStopped, DryerStatus::kRunning,
                              DryerStatus::kCooling, DryerStatus::kFault};

  for (DryerStatus status : kAll)
  {
    LedPattern pattern = PatternFor(status, kSettled + kBeatOn);
    TEST_ASSERT_TRUE(pattern.green || pattern.red);
  }
}

// The blink is a pure function of the clock, so it must repeat period after
// period rather than drift with the caller's cadence.
void test_the_blink_repeats_every_two_intervals(void)
{
  for (uint32_t period = 0; period < 5; period++)
  {
    uint32_t base = period * 2 * STATUS_BLINK_INTERVAL;
    TEST_ASSERT_TRUE(PatternFor(DryerStatus::kFault, base).red);
    TEST_ASSERT_TRUE(PatternFor(DryerStatus::kFault, base + STATUS_BLINK_INTERVAL - 1).red);
    TEST_ASSERT_FALSE(PatternFor(DryerStatus::kFault, base + STATUS_BLINK_INTERVAL).red);
    TEST_ASSERT_FALSE(PatternFor(DryerStatus::kFault,
                                 base + 2 * STATUS_BLINK_INTERVAL - 1).red);
  }
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_running_is_steady_green);
  RUN_TEST(test_stopped_is_steady_red);
  RUN_TEST(test_fan_running_after_stop_is_cooling);
  RUN_TEST(test_fault_blinks_red_with_the_green_out);
  RUN_TEST(test_fault_wins_over_a_running_session);
  RUN_TEST(test_fault_wins_over_the_cooldown);
  RUN_TEST(test_a_fault_during_the_grace_window_is_not_reported);
  RUN_TEST(test_the_same_fault_is_reported_once_the_window_has_passed);
  RUN_TEST(test_no_state_is_ever_fully_dark_on_the_lit_half_of_a_beat);
  RUN_TEST(test_the_blink_repeats_every_two_intervals);
  return UNITY_END();
}
