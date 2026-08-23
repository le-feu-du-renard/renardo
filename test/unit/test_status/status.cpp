// Unit tests for StatusIndicator.
//
// The panel LEDs are the only thing about this machine an operator can read
// from across the room, so a state that resolves the wrong way is a lie told at
// a distance. Three things are worth pinning down: that the session axis and
// the fault axis stay independent, that red means one thing steady and another
// blinking, and that a freshly booted dryer whose probe has not answered yet is
// not a dryer in fault.

#include <string.h>
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

PanelState Panel(DryerStatus status, DryerFault fault)
{
  return PanelState{status, fault};
}

} // namespace

void test_running_is_steady_green(void)
{
  PanelState state = ResolvePanel(true, true, DryerFault::kNone);
  TEST_ASSERT_EQUAL(DryerStatus::kRunning, state.status);
  TEST_ASSERT_EQUAL(DryerFault::kNone, state.fault);

  // Steady means steady: the same at both ends of a blink period.
  LedPattern on  = PatternFor(state, kSettled + kBeatOn);
  LedPattern off = PatternFor(state, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.green);
  TEST_ASSERT_TRUE(off.green);
  TEST_ASSERT_FALSE(on.red);
  TEST_ASSERT_FALSE(off.red);
}

void test_stopped_is_steady_red(void)
{
  PanelState state = ResolvePanel(false, false, DryerFault::kNone);
  TEST_ASSERT_EQUAL(DryerStatus::kStopped, state.status);

  LedPattern on  = PatternFor(state, kSettled + kBeatOn);
  LedPattern off = PatternFor(state, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.red);
  TEST_ASSERT_TRUE(off.red);
  TEST_ASSERT_FALSE(on.green);
  TEST_ASSERT_FALSE(off.green);
}

// The fan outliving a stopped session is the cooldown, and it has to read
// differently from a dryer at rest: the fan is still turning in there.
void test_fan_running_after_stop_is_cooling(void)
{
  PanelState state = ResolvePanel(false, true, DryerFault::kNone);
  TEST_ASSERT_EQUAL(DryerStatus::kCooling, state.status);

  LedPattern on  = PatternFor(state, kSettled + kBeatOn);
  LedPattern off = PatternFor(state, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.green);
  TEST_ASSERT_FALSE(off.green);
  TEST_ASSERT_FALSE(on.red);
  TEST_ASSERT_FALSE(off.red);
}

void test_fault_on_a_stopped_dryer_blinks_red(void)
{
  PanelState state = ResolvePanel(false, false, DryerFault::kHydraulicOffline);
  TEST_ASSERT_EQUAL(DryerStatus::kStopped, state.status);

  // Idle red is steady, fault red blinks. That difference is the whole reason
  // one lamp can carry both meanings.
  LedPattern on  = PatternFor(state, kSettled + kBeatOn);
  LedPattern off = PatternFor(state, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.red);
  TEST_ASSERT_FALSE(off.red);
  TEST_ASSERT_FALSE(on.green);
  TEST_ASSERT_FALSE(off.green);
}

// The bug this whole split exists to fix: a stale probe blocks the heat sources
// but not the fan, so the dryer can be turning while something is wrong. The
// panel used to answer that with red alone, leaving the operator no way to tell
// a running dryer in fault from a stopped one.
void test_a_running_dryer_in_fault_shows_both_lamps(void)
{
  PanelState state = ResolvePanel(true, true, DryerFault::kSensorStale);
  TEST_ASSERT_EQUAL(DryerStatus::kRunning, state.status);
  TEST_ASSERT_EQUAL(DryerFault::kSensorStale, state.fault);

  LedPattern on  = PatternFor(state, kSettled + kBeatOn);
  LedPattern off = PatternFor(state, kSettled + kBeatOff);

  // Green steady says the session is running...
  TEST_ASSERT_TRUE(on.green);
  TEST_ASSERT_TRUE(off.green);
  // ...while red blinks over it for the fault.
  TEST_ASSERT_TRUE(on.red);
  TEST_ASSERT_FALSE(off.red);
}

void test_a_cooling_dryer_in_fault_shows_both_lamps(void)
{
  PanelState state = ResolvePanel(false, true, DryerFault::kAirflowBlocked);
  TEST_ASSERT_EQUAL(DryerStatus::kCooling, state.status);

  LedPattern on  = PatternFor(state, kSettled + kBeatOn);
  LedPattern off = PatternFor(state, kSettled + kBeatOff);
  TEST_ASSERT_TRUE(on.green);
  TEST_ASSERT_FALSE(off.green);
  TEST_ASSERT_TRUE(on.red);
  TEST_ASSERT_FALSE(off.red);
}

// A fault must never hide the session, whichever it is.
void test_the_fault_never_changes_the_session_axis(void)
{
  const DryerFault kFaults[] = {
      DryerFault::kNone, DryerFault::kAirflowBlocked, DryerFault::kDamperFeedback,
      DryerFault::kSensorStale, DryerFault::kHydraulicOffline};

  for (DryerFault fault : kFaults)
  {
    TEST_ASSERT_EQUAL(DryerStatus::kRunning, ResolvePanel(true, true, fault).status);
    TEST_ASSERT_EQUAL(DryerStatus::kCooling, ResolvePanel(false, true, fault).status);
    TEST_ASSERT_EQUAL(DryerStatus::kStopped, ResolvePanel(false, false, fault).status);
  }
}

// The inlet probe and the hydraulic module are both silent until Core 1 has
// completed its first RS485 cycle. Every freshly booted dryer is therefore in
// "fault" by the letter of the test, and must not blink for it — nor be refused
// a start for it, which is the other half of what the grace protects.
void test_a_polled_fault_during_the_grace_window_is_not_reported(void)
{
  TEST_ASSERT_EQUAL(DryerFault::kNone, ApplyFaultGrace(DryerFault::kSensorStale, 0));
  TEST_ASSERT_EQUAL(DryerFault::kNone,
                    ApplyFaultGrace(DryerFault::kHydraulicOffline, 0));
  TEST_ASSERT_EQUAL(
      DryerFault::kNone,
      ApplyFaultGrace(DryerFault::kHydraulicOffline, STATUS_FAULT_GRACE_MS - 1));
}

// The air path faults come off the ADC, which reads from the first loop. There
// is nothing to wait for, and waiting would mean fifteen seconds in which a
// dryer with both registers shut would take a start.
void test_the_air_path_faults_are_not_graced(void)
{
  TEST_ASSERT_EQUAL(DryerFault::kAirflowBlocked,
                    ApplyFaultGrace(DryerFault::kAirflowBlocked, 0));
  TEST_ASSERT_EQUAL(DryerFault::kDamperFeedback,
                    ApplyFaultGrace(DryerFault::kDamperFeedback, 0));
}

void test_the_same_fault_is_reported_once_the_window_has_passed(void)
{
  TEST_ASSERT_EQUAL(
      DryerFault::kHydraulicOffline,
      ApplyFaultGrace(DryerFault::kHydraulicOffline, STATUS_FAULT_GRACE_MS));
  TEST_ASSERT_EQUAL(
      DryerFault::kSensorStale,
      ApplyFaultGrace(DryerFault::kSensorStale, STATUS_FAULT_GRACE_MS + 1000));
}

// No state leaves both LEDs dark: a dead LED or an unpowered board must not
// look like a dryer sitting quietly at rest. The blinking states are exempt
// half the time by definition — what matters is that the dark half is a beat,
// not a state.
void test_no_state_is_ever_fully_dark_on_the_lit_half_of_a_beat(void)
{
  const DryerStatus kStates[] = {DryerStatus::kStopped, DryerStatus::kRunning,
                                 DryerStatus::kCooling};
  const DryerFault kFaults[]  = {DryerFault::kNone, DryerFault::kAirflowBlocked,
                                 DryerFault::kDamperFeedback, DryerFault::kSensorStale,
                                 DryerFault::kHydraulicOffline};

  for (DryerStatus status : kStates)
  {
    for (DryerFault fault : kFaults)
    {
      LedPattern pattern = PatternFor(Panel(status, fault), kSettled + kBeatOn);
      TEST_ASSERT_TRUE(pattern.green || pattern.red);
    }
  }
}

// The blink is a pure function of the clock, so it must repeat period after
// period rather than drift with the caller's cadence.
void test_the_blink_repeats_every_two_intervals(void)
{
  PanelState state = Panel(DryerStatus::kStopped, DryerFault::kSensorStale);

  for (uint32_t period = 0; period < 5; period++)
  {
    uint32_t base = period * 2 * STATUS_BLINK_INTERVAL;
    TEST_ASSERT_TRUE(PatternFor(state, base).red);
    TEST_ASSERT_TRUE(PatternFor(state, base + STATUS_BLINK_INTERVAL - 1).red);
    TEST_ASSERT_FALSE(PatternFor(state, base + STATUS_BLINK_INTERVAL).red);
    TEST_ASSERT_FALSE(PatternFor(state, base + 2 * STATUS_BLINK_INTERVAL - 1).red);
  }
}

// Which faults end a batch. The hydraulic module is the only one that does not:
// it is optional at runtime and losing it degrades to electric-only.
void test_the_faults_that_stop_a_session(void)
{
  TEST_ASSERT_NOT_EQUAL(kFaultNeverStops,
                        FaultStopHoldoffMs(DryerFault::kAirflowBlocked));
  TEST_ASSERT_NOT_EQUAL(kFaultNeverStops, FaultStopHoldoffMs(DryerFault::kSensorStale));

  // The one most easily argued out of the list, and the one that most needs to
  // be in it: a dead recopy does not cost one indicator, it disarms the airflow
  // interlock, because IsClosed() reads no signal as not-shut. A dryer that
  // cannot tell whether air is moving must not keep heating on the assumption.
  TEST_ASSERT_NOT_EQUAL(kFaultNeverStops,
                        FaultStopHoldoffMs(DryerFault::kDamperFeedback));

  TEST_ASSERT_EQUAL(kFaultNeverStops,
                    FaultStopHoldoffMs(DryerFault::kHydraulicOffline));
  TEST_ASSERT_EQUAL(kFaultNeverStops, FaultStopHoldoffMs(DryerFault::kNone));
}

// Only the probe waits, because only the probe's threshold is a timeout. The
// other two arrive already confirmed — 30 s of both registers reading shut,
// three samples below the signal floor — so a hold-off here would be the same
// debounce served twice on a dryer that is provably unfit.
void test_only_the_probe_gets_a_holdoff(void)
{
  TEST_ASSERT_EQUAL_UINT32(0, FaultStopHoldoffMs(DryerFault::kAirflowBlocked));
  TEST_ASSERT_EQUAL_UINT32(0, FaultStopHoldoffMs(DryerFault::kDamperFeedback));
  TEST_ASSERT_TRUE(FaultStopHoldoffMs(DryerFault::kSensorStale) > 0);
}

// The two sensor thresholds are one silence measured twice: the fault appears
// at SENSOR_TIMEOUT_MS and the batch ends at SENSOR_SESSION_TIMEOUT_MS, so the
// hold-off has to be exactly the gap. Getting this wrong is invisible on the
// bench — the dryer would simply stop at the wrong moment.
void test_the_probe_holdoff_lands_the_stop_at_the_session_timeout(void)
{
  uint32_t holdoff = FaultStopHoldoffMs(DryerFault::kSensorStale);
  TEST_ASSERT_EQUAL_UINT32(SENSOR_SESSION_TIMEOUT_MS, SENSOR_TIMEOUT_MS + holdoff);

  // And the batch must outlive the heating interlock, or the two thresholds
  // would fire in the wrong order and the purge window would not exist at all.
  TEST_ASSERT_TRUE(SENSOR_SESSION_TIMEOUT_MS > SENSOR_TIMEOUT_MS);
}

// Every fault has a name, including the ones added last: a log line reading
// "fault: unknown" is a fault report that has to be reproduced to be read.
void test_every_fault_has_a_name(void)
{
  const DryerFault kFaults[] = {DryerFault::kAirflowBlocked,
                                DryerFault::kDamperFeedback, DryerFault::kSensorStale,
                                DryerFault::kHydraulicOffline};

  for (DryerFault fault : kFaults)
  {
    TEST_ASSERT_TRUE(strcmp(FaultName(fault), "unknown") != 0);
    TEST_ASSERT_TRUE(strcmp(FaultName(fault), "none") != 0);
  }
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_running_is_steady_green);
  RUN_TEST(test_stopped_is_steady_red);
  RUN_TEST(test_fan_running_after_stop_is_cooling);
  RUN_TEST(test_fault_on_a_stopped_dryer_blinks_red);
  RUN_TEST(test_a_running_dryer_in_fault_shows_both_lamps);
  RUN_TEST(test_a_cooling_dryer_in_fault_shows_both_lamps);
  RUN_TEST(test_the_fault_never_changes_the_session_axis);
  RUN_TEST(test_a_polled_fault_during_the_grace_window_is_not_reported);
  RUN_TEST(test_the_air_path_faults_are_not_graced);
  RUN_TEST(test_the_same_fault_is_reported_once_the_window_has_passed);
  RUN_TEST(test_no_state_is_ever_fully_dark_on_the_lit_half_of_a_beat);
  RUN_TEST(test_the_blink_repeats_every_two_intervals);
  RUN_TEST(test_the_faults_that_stop_a_session);
  RUN_TEST(test_only_the_probe_gets_a_holdoff);
  RUN_TEST(test_the_probe_holdoff_lands_the_stop_at_the_session_timeout);
  RUN_TEST(test_every_fault_has_a_name);
  return UNITY_END();
}
