// Unit tests for the backoff gate that fronts every remote RS485 module.
//
// RemoteModule itself needs Rs485Bus and ModbusMaster, so what is tested here is
// the part that decides whether a transaction is worth spending 2 s of poll loop
// on: the failure count, the retry interval, and the fact that a skipped attempt
// is not itself a failure.

#include <unity.h>

#include "BackoffGate.h"

static constexpr uint32_t kRetryMs = 30000;

void setUp(void) {}
void tearDown(void) {}

void test_a_fresh_gate_lets_everything_through(void)
{
  BackoffGate gate(kRetryMs);

  TEST_ASSERT_TRUE(gate.ShouldAttempt(0));
  TEST_ASSERT_FALSE(gate.IsBackedOff());
}

void test_a_single_failure_does_not_back_off(void)
{
  // A lone CRC error on a shared bus must not cost a healthy module its next
  // several cycles.
  BackoffGate gate(kRetryMs);

  gate.RecordFailure(1000);

  TEST_ASSERT_FALSE(gate.IsBackedOff());
  TEST_ASSERT_TRUE(gate.ShouldAttempt(1000));
}

void test_two_failures_back_off_until_the_interval_elapses(void)
{
  BackoffGate gate(kRetryMs);

  gate.RecordFailure(1000);
  gate.RecordFailure(3000);

  TEST_ASSERT_TRUE(gate.IsBackedOff());
  TEST_ASSERT_FALSE(gate.ShouldAttempt(3000));
  TEST_ASSERT_FALSE(gate.ShouldAttempt(3000 + kRetryMs - 1));
  TEST_ASSERT_TRUE(gate.ShouldAttempt(3000 + kRetryMs));
}

void test_a_skipped_attempt_is_not_a_failure(void)
{
  // The gate is asked on every cycle while backed off. Those queries must not
  // push the retry deadline further out, or the module would never be retried.
  BackoffGate gate(kRetryMs);

  gate.RecordFailure(1000);
  gate.RecordFailure(3000);

  for (uint32_t now = 3000; now < 3000 + kRetryMs; now += 2000)
  {
    gate.ShouldAttempt(now);
  }

  TEST_ASSERT_TRUE(gate.ShouldAttempt(3000 + kRetryMs));
}

void test_a_success_clears_the_backoff(void)
{
  BackoffGate gate(kRetryMs);

  gate.RecordFailure(1000);
  gate.RecordFailure(3000);
  TEST_ASSERT_TRUE(gate.IsBackedOff());

  gate.RecordSuccess();

  TEST_ASSERT_FALSE(gate.IsBackedOff());
  TEST_ASSERT_TRUE(gate.ShouldAttempt(3000));
}

void test_a_retry_that_fails_again_waits_another_interval(void)
{
  BackoffGate gate(kRetryMs);

  gate.RecordFailure(1000);
  gate.RecordFailure(3000);

  uint32_t retry_at = 3000 + kRetryMs;
  TEST_ASSERT_TRUE(gate.ShouldAttempt(retry_at));
  gate.RecordFailure(retry_at);

  TEST_ASSERT_FALSE(gate.ShouldAttempt(retry_at + kRetryMs - 1));
  TEST_ASSERT_TRUE(gate.ShouldAttempt(retry_at + kRetryMs));
}

void test_a_module_plugged_in_later_is_picked_up(void)
{
  // The whole point of retrying at all: an absent module answers one day, and
  // from then on it is polled every cycle again.
  BackoffGate gate(kRetryMs);

  gate.RecordFailure(1000);
  gate.RecordFailure(3000);

  uint32_t retry_at = 3000 + kRetryMs;
  TEST_ASSERT_TRUE(gate.ShouldAttempt(retry_at));
  gate.RecordSuccess();

  TEST_ASSERT_TRUE(gate.ShouldAttempt(retry_at));
  TEST_ASSERT_TRUE(gate.ShouldAttempt(retry_at + 2000));
}

void test_the_failure_count_does_not_wrap_after_a_long_absence(void)
{
  // Hours of failures must leave the gate backed off, not roll a counter over
  // into "healthy again".
  BackoffGate gate(kRetryMs);

  for (uint32_t i = 0; i < 1000; i++)
  {
    gate.RecordFailure(i * kRetryMs);
  }

  TEST_ASSERT_TRUE(gate.IsBackedOff());
}

void test_the_retry_deadline_survives_a_millis_wrap(void)
{
  // millis() wraps every 49 days. Unsigned subtraction makes the comparison
  // valid across the wrap, and this pins that down.
  BackoffGate gate(kRetryMs);

  uint32_t before_wrap = 0xFFFFFFFFu - 5000;
  gate.RecordFailure(before_wrap - 2000);
  gate.RecordFailure(before_wrap);

  uint32_t after_wrap = before_wrap + kRetryMs; // wraps past zero

  TEST_ASSERT_FALSE(gate.ShouldAttempt(before_wrap + kRetryMs - 1));
  TEST_ASSERT_TRUE(gate.ShouldAttempt(after_wrap));
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_a_fresh_gate_lets_everything_through);
  RUN_TEST(test_a_single_failure_does_not_back_off);
  RUN_TEST(test_two_failures_back_off_until_the_interval_elapses);
  RUN_TEST(test_a_skipped_attempt_is_not_a_failure);
  RUN_TEST(test_a_success_clears_the_backoff);
  RUN_TEST(test_a_retry_that_fails_again_waits_another_interval);
  RUN_TEST(test_a_module_plugged_in_later_is_picked_up);
  RUN_TEST(test_the_failure_count_does_not_wrap_after_a_long_absence);
  RUN_TEST(test_the_retry_deadline_survives_a_millis_wrap);
  return UNITY_END();
}
