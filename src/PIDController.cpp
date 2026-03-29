#include "PIDController.h"

#ifdef ARDUINO
#include "Logger.h"
#else
// Mock logger for unit tests
#define Log_warning(fmt, ...)
#define Log_trace(fmt, ...)
#define Log_notice(fmt, ...)
namespace {
  struct MockLog {
    template<typename... Args>
    void warning(const char*, Args...) {}
    template<typename... Args>
    void trace(const char*, Args...) {}
    template<typename... Args>
    void notice(const char*, Args...) {}
  };
  MockLog Log;
}
#endif

PIDController::PIDController(float kp, float ki, float kd,
                             float output_min, float output_max,
                             float integral_max, float derivative_filter)
  : kp_(kp),
    ki_(ki),
    kd_(kd),
    output_min_(output_min),
    output_max_(output_max),
    integral_max_(integral_max),
    derivative_filter_(derivative_filter),
    last_error_(0.0f),
    integral_(0.0f),
    derivative_filtered_(0.0f),
    last_output_(0.0f),
    p_term_(0.0f),
    i_term_(0.0f),
    d_term_(0.0f) {
}

float PIDController::Compute(float setpoint, float measured_value, float dt) {
  // Avoid division by zero
  if (dt <= 0.0f) {
    Log.warning("PID: Invalid dt (%F), skipping computation", dt);
    return last_output_;
  }

  // Calculate error
  float error = setpoint - measured_value;

  // Proportional term
  p_term_ = kp_ * error;

  // Integral term with anti-windup
  integral_ += error * dt;
  integral_ = Clamp(integral_, -integral_max_, integral_max_);
  i_term_ = ki_ * integral_;

  // Derivative term with filtering
  float derivative_raw = (error - last_error_) / dt;

  // Low-pass filter on derivative to reduce noise
  // filtered = alpha * new + (1 - alpha) * old
  derivative_filtered_ = derivative_filter_ * derivative_raw +
                         (1.0f - derivative_filter_) * derivative_filtered_;
  d_term_ = kd_ * derivative_filtered_;

  // Calculate total output
  float output = p_term_ + i_term_ + d_term_;

  // Clamp output
  output = Clamp(output, output_min_, output_max_);

  // Save state for next iteration
  last_error_ = error;
  last_output_ = output;

  return output;
}

void PIDController::Reset() {
  last_error_ = 0.0f;
  integral_ = 0.0f;
  derivative_filtered_ = 0.0f;
  last_output_ = 0.0f;
  p_term_ = 0.0f;
  i_term_ = 0.0f;
  d_term_ = 0.0f;

  Log.trace("PID: Reset internal state");
}

void PIDController::SetParameters(float kp, float ki, float kd) {
  kp_ = kp;
  ki_ = ki;
  kd_ = kd;

  Log.notice("PID: Parameters updated - Kp=%F, Ki=%F, Kd=%F", kp_, ki_, kd_);
}

void PIDController::SetOutputLimits(float output_min, float output_max) {
  output_min_ = output_min;
  output_max_ = output_max;

  Log.trace("PID: Output limits updated - min=%F, max=%F", output_min_, output_max_);
}

void PIDController::SetIntegralLimit(float integral_max) {
  integral_max_ = integral_max;

  // Clamp current integral if it exceeds new limit
  integral_ = Clamp(integral_, -integral_max_, integral_max_);

  Log.trace("PID: Integral limit updated - max=%F", integral_max_);
}

void PIDController::SetDerivativeFilter(float filter_coef) {
  // Clamp filter coefficient to valid range
  derivative_filter_ = Clamp(filter_coef, 0.0f, 1.0f);

  Log.trace("PID: Derivative filter updated - coef=%F", derivative_filter_);
}

float PIDController::Clamp(float value, float min, float max) const {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}
