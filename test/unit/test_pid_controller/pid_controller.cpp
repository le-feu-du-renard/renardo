#include <unity.h>
#include "PIDController.h"
#include <cmath>

/**
 * Unit tests for PIDController
 * Tests the PID calculation logic, anti-windup, and filtering
 */

void setUp(void) {
  // Called before each test
}

void tearDown(void) {
  // Called after each test
}

// ===== Basic PID Computation Tests =====

void test_pid_proportional_only(void) {
  // Test P-only controller (Ki=0, Kd=0)
  PIDController pid(1.0f, 0.0f, 0.0f, 0.0f, 100.0f);

  // Error = 10°C, Kp = 1.0 => output = 10.0
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, output);

  // Error = -5°C, Kp = 1.0 => output = -5.0, clamped to 0.0
  output = pid.Compute(40.0f, 45.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, output);
}

void test_pid_integral_accumulation(void) {
  // Test I-only controller (Kp=0, Kd=0)
  PIDController pid(0.0f, 1.0f, 0.0f, 0.0f, 100.0f);

  // First computation: error = 10°C, dt = 1s
  // Integral = 10 * 1 = 10, output = 1.0 * 10 = 10.0
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, output);

  // Second computation: error = 10°C, dt = 1s
  // Integral = 10 + (10 * 1) = 20, output = 1.0 * 20 = 20.0
  output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, output);

  // Third computation: error = 10°C, dt = 1s
  // Integral = 20 + (10 * 1) = 30, output = 1.0 * 30 = 30.0
  output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, output);
}

void test_pid_derivative_on_change(void) {
  // Test D-only controller (Kp=0, Ki=0)
  PIDController pid(0.0f, 0.0f, 1.0f, 0.0f, 100.0f, 50.0f, 1.0f); // No filtering

  // First computation: error = 10°C, derivative = 10/1 = 10
  // But first call has no previous error, so derivative is based on (10-0)/1 = 10
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, output);

  // Second computation: error = 5°C, derivative = (5-10)/1 = -5
  // Output = 1.0 * (-5) = -5.0, clamped to 0.0
  output = pid.Compute(50.0f, 45.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, output);
}

void test_pid_full_computation(void) {
  // Test full PID controller
  PIDController pid(1.0f, 0.5f, 0.2f, 0.0f, 100.0f);

  // First computation: setpoint=50, measured=40, dt=1s
  // Error = 10
  // P = 1.0 * 10 = 10.0
  // I = 0.5 * 10 = 5.0
  // D = 0.2 * 10 (with some filtering) ≈ 0.2
  // Total ≈ 15.2 (approximately, due to filtering)
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 15.0f, output);
}

// ===== Anti-Windup Tests =====

void test_pid_integral_windup_prevention(void) {
  // Test anti-windup: integral should be clamped
  PIDController pid(0.0f, 1.0f, 0.0f, 0.0f, 100.0f, 20.0f); // integral_max = 20

  // Accumulate integral beyond limit
  for (int i = 0; i < 30; i++) {
    pid.Compute(50.0f, 40.0f, 1.0f); // Error = 10, integral += 10
  }

  // Integral should be clamped to 20.0
  float integral = pid.GetIntegral();
  TEST_ASSERT_EQUAL_FLOAT(20.0f, integral);

  // Output should be 1.0 * 20.0 = 20.0
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, output);
}

void test_pid_integral_negative_windup(void) {
  // Test anti-windup in negative direction
  PIDController pid(0.0f, 1.0f, 0.0f, -100.0f, 100.0f, 20.0f);

  // Accumulate negative integral beyond limit
  for (int i = 0; i < 30; i++) {
    pid.Compute(40.0f, 50.0f, 1.0f); // Error = -10, integral -= 10
  }

  // Integral should be clamped to -20.0
  float integral = pid.GetIntegral();
  TEST_ASSERT_EQUAL_FLOAT(-20.0f, integral);
}

// ===== Output Clamping Tests =====

void test_pid_output_clamping_upper(void) {
  PIDController pid(10.0f, 0.0f, 0.0f, 0.0f, 50.0f);

  // Error = 10, P = 10 * 10 = 100, but max output is 50
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(50.0f, output);
}

void test_pid_output_clamping_lower(void) {
  PIDController pid(10.0f, 0.0f, 0.0f, 10.0f, 100.0f);

  // Error = -10, P = 10 * (-10) = -100, but min output is 10
  float output = pid.Compute(40.0f, 50.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, output);
}

void test_pid_output_within_bounds(void) {
  PIDController pid(1.0f, 0.0f, 0.0f, 0.0f, 100.0f);

  // Error = 10, P = 1.0 * 10 = 10.0 (within bounds)
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, output);
}

// ===== Reset Tests =====

void test_pid_reset_clears_state(void) {
  PIDController pid(1.0f, 1.0f, 1.0f, 0.0f, 100.0f);

  // Accumulate some state
  pid.Compute(50.0f, 40.0f, 1.0f);
  pid.Compute(50.0f, 40.0f, 1.0f);

  // Verify state exists
  TEST_ASSERT_NOT_EQUAL(0.0f, pid.GetIntegral());
  TEST_ASSERT_NOT_EQUAL(0.0f, pid.GetLastError());

  // Reset
  pid.Reset();

  // Verify state is cleared
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.GetIntegral());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.GetLastError());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.GetDerivative());
}

// ===== Parameter Update Tests =====

void test_pid_set_parameters(void) {
  PIDController pid(1.0f, 1.0f, 1.0f, 0.0f, 100.0f);

  // Update parameters
  pid.SetParameters(5.0f, 0.5f, 2.0f);

  // Test with new parameters: error = 10
  // P = 5.0 * 10 = 50.0
  // I = 0.5 * 10 = 5.0
  // D ≈ 2.0 * 10 = 20.0 (with filtering)
  // Total ≈ 75.0
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 75.0f, output);
}

void test_pid_set_output_limits(void) {
  PIDController pid(10.0f, 0.0f, 0.0f, 0.0f, 100.0f);

  // Change output limits
  pid.SetOutputLimits(0.0f, 20.0f);

  // Error = 10, P = 100, but max is now 20
  float output = pid.Compute(50.0f, 40.0f, 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, output);
}

void test_pid_set_integral_limit(void) {
  PIDController pid(0.0f, 1.0f, 0.0f, 0.0f, 100.0f, 50.0f);

  // Accumulate integral
  for (int i = 0; i < 10; i++) {
    pid.Compute(50.0f, 40.0f, 1.0f);
  }

  // Change integral limit to a lower value
  pid.SetIntegralLimit(5.0f);

  // Integral should be clamped to new limit
  TEST_ASSERT_EQUAL_FLOAT(5.0f, pid.GetIntegral());
}

// ===== Edge Cases =====

void test_pid_zero_dt_skips_computation(void) {
  PIDController pid(1.0f, 1.0f, 1.0f, 0.0f, 100.0f);

  // First valid computation
  float output1 = pid.Compute(50.0f, 40.0f, 1.0f);

  // Zero dt should skip computation and return last output
  float output2 = pid.Compute(50.0f, 40.0f, 0.0f);
  TEST_ASSERT_EQUAL_FLOAT(output1, output2);
}

void test_pid_negative_dt_skips_computation(void) {
  PIDController pid(1.0f, 1.0f, 1.0f, 0.0f, 100.0f);

  // First valid computation
  float output1 = pid.Compute(50.0f, 40.0f, 1.0f);

  // Negative dt should skip computation
  float output2 = pid.Compute(50.0f, 40.0f, -1.0f);
  TEST_ASSERT_EQUAL_FLOAT(output1, output2);
}

void test_pid_large_dt_handled(void) {
  PIDController pid(1.0f, 1.0f, 1.0f, 0.0f, 100.0f);

  // Large dt (>10s) should be rejected
  float output1 = pid.Compute(50.0f, 40.0f, 1.0f);
  float output2 = pid.Compute(50.0f, 40.0f, 15.0f);

  // Should return last output
  TEST_ASSERT_EQUAL_FLOAT(output1, output2);
}

void test_pid_zero_error(void) {
  PIDController pid(1.0f, 1.0f, 1.0f, 0.0f, 100.0f);

  // No error: setpoint = measured
  float output = pid.Compute(50.0f, 50.0f, 1.0f);

  // P = 0, I = 0, D ≈ 0 => output ≈ 0
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, output);
}

// ===== Derivative Filter Tests =====

void test_pid_derivative_filtering(void) {
  // Low filter coefficient (heavy filtering)
  PIDController pid1(0.0f, 0.0f, 1.0f, 0.0f, 100.0f, 50.0f, 0.1f);

  // No filter (coefficient = 1.0)
  PIDController pid2(0.0f, 0.0f, 1.0f, 0.0f, 100.0f, 50.0f, 1.0f);

  // Apply same error to both
  float output1 = pid1.Compute(50.0f, 40.0f, 1.0f);
  float output2 = pid2.Compute(50.0f, 40.0f, 1.0f);

  // Filtered derivative should be smaller (more smoothed)
  TEST_ASSERT_LESS_THAN(output2, output1 + 0.1f);
}

// ===== Test Runner =====

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Basic computation tests
  RUN_TEST(test_pid_proportional_only);
  RUN_TEST(test_pid_integral_accumulation);
  RUN_TEST(test_pid_derivative_on_change);
  RUN_TEST(test_pid_full_computation);

  // Anti-windup tests
  RUN_TEST(test_pid_integral_windup_prevention);
  RUN_TEST(test_pid_integral_negative_windup);

  // Output clamping tests
  RUN_TEST(test_pid_output_clamping_upper);
  RUN_TEST(test_pid_output_clamping_lower);
  RUN_TEST(test_pid_output_within_bounds);

  // Reset tests
  RUN_TEST(test_pid_reset_clears_state);

  // Parameter update tests
  RUN_TEST(test_pid_set_parameters);
  RUN_TEST(test_pid_set_output_limits);
  RUN_TEST(test_pid_set_integral_limit);

  // Edge cases
  RUN_TEST(test_pid_zero_dt_skips_computation);
  RUN_TEST(test_pid_negative_dt_skips_computation);
  RUN_TEST(test_pid_large_dt_handled);
  RUN_TEST(test_pid_zero_error);

  // Derivative filter tests
  RUN_TEST(test_pid_derivative_filtering);

  return UNITY_END();
}
