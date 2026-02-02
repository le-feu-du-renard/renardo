#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdint>
#include <cmath>
#endif

/**
 * Generic PID Controller
 * Implements Proportional-Integral-Derivative control with:
 * - Anti-windup (integral clamping)
 * - Derivative filtering (to reduce noise)
 * - Output clamping
 */
class PIDController {
 public:
  /**
   * Constructor
   * @param kp Proportional gain
   * @param ki Integral gain
   * @param kd Derivative gain
   * @param output_min Minimum output value
   * @param output_max Maximum output value
   * @param integral_max Maximum integral value (anti-windup)
   * @param derivative_filter Filter coefficient for derivative (0-1, default 0.1)
   */
  PIDController(float kp = 0.0f, float ki = 0.0f, float kd = 0.0f,
                float output_min = 0.0f, float output_max = 100.0f,
                float integral_max = 50.0f, float derivative_filter = 0.1f);

  /**
   * Compute PID output
   * @param setpoint Target value
   * @param measured_value Current measured value
   * @param dt Time delta since last computation (seconds)
   * @return PID output (clamped to output_min/output_max)
   */
  float Compute(float setpoint, float measured_value, float dt);

  /**
   * Reset internal state (integral, derivative)
   * Call this when starting/stopping the controller
   */
  void Reset();

  /**
   * Update PID parameters
   */
  void SetParameters(float kp, float ki, float kd);
  void SetOutputLimits(float output_min, float output_max);
  void SetIntegralLimit(float integral_max);
  void SetDerivativeFilter(float filter_coef);

  /**
   * Get current state (for debugging/monitoring)
   */
  float GetLastError() const { return last_error_; }
  float GetIntegral() const { return integral_; }
  float GetDerivative() const { return derivative_filtered_; }
  float GetLastOutput() const { return last_output_; }

  // Get individual PID terms (for tuning/debugging)
  float GetProportionalTerm() const { return p_term_; }
  float GetIntegralTerm() const { return i_term_; }
  float GetDerivativeTerm() const { return d_term_; }

 private:
  // PID gains
  float kp_;
  float ki_;
  float kd_;

  // Output limits
  float output_min_;
  float output_max_;

  // Anti-windup limit
  float integral_max_;

  // Derivative filter coefficient (0-1)
  // Higher value = more filtering (slower response but less noise)
  float derivative_filter_;

  // Internal state
  float last_error_;
  float integral_;
  float derivative_filtered_;
  float last_output_;

  // Individual terms (for debugging)
  float p_term_;
  float i_term_;
  float d_term_;

  // Helper to clamp values
  float Clamp(float value, float min, float max) const;
};

#endif  // PID_CONTROLLER_H
