#include <unity.h>
#include <cmath>

/**
 * Unit tests for TemperatureManager control logic.
 *
 * Replicates pure logic from TemperatureManager (no Arduino/hardware dependencies):
 *   - ECO mode effective target calculation
 *   - Night-window hour detection
 *   - Temperature in-range check
 *   - Target clamping
 *   - SetTargetTemperature dead-band (< 1°C change ignored)
 *
 * All constants mirrored from config.h.
 */

// ===== Constants (mirrored from config.h) =====
static constexpr float    kEcoNightTargetPct = 85.0f;
static constexpr uint8_t  kEcoStartHour      = 18;
static constexpr uint8_t  kEcoEndHour        = 9;
static constexpr float    kTempTargetMin     = 20.0f;
static constexpr float    kTempTargetMax     = 45.0f;
static constexpr float    kInRangeTolerance  = 2.0f;

// ===== Replicas of TemperatureManager logic =====

// Mirrors IsEcoWindowActive() — ECO window: hour >= 18 OR hour < 9
bool IsEcoWindowActive(bool eco_mode, uint8_t hour) {
  if (!eco_mode) return false;
  return (hour >= kEcoStartHour) || (hour < kEcoEndHour);
}

// Mirrors GetEffectiveTargetTemperature()
float GetEffectiveTarget(float target, bool eco_mode, uint8_t hour) {
  if (IsEcoWindowActive(eco_mode, hour))
    return target * (kEcoNightTargetPct / 100.0f);
  return target;
}

// Mirrors SetTargetTemperature() clamping + dead-band
float ClampTarget(float requested, float current) {
  float clamped = requested;
  if (clamped < kTempTargetMin) clamped = kTempTargetMin;
  if (clamped > kTempTargetMax) clamped = kTempTargetMax;
  if (fabsf(clamped - current) < 1.0f) return current;  // dead-band: no change
  return clamped;
}

// Mirrors IsTemperatureInRange()
bool IsTemperatureInRange(float measured, float target) {
  return fabsf(measured - target) <= kInRangeTolerance;
}

void setUp(void) {}
void tearDown(void) {}

// ===== ECO Mode Target Calculation =====

void test_eco_mode_off_returns_full_target(void) {
  // Performance mode: effective target = raw target at all hours.
  TEST_ASSERT_EQUAL_FLOAT(40.0f, GetEffectiveTarget(40.0f, false, 20));
  TEST_ASSERT_EQUAL_FLOAT(40.0f, GetEffectiveTarget(40.0f, false, 3));
  TEST_ASSERT_EQUAL_FLOAT(40.0f, GetEffectiveTarget(40.0f, false, 12));
}

void test_eco_mode_night_window_reduces_target(void) {
  // ECO mode, night window active: target × 85%.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, GetEffectiveTarget(40.0f, true, 20));  // 40 * 0.85 = 34
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, GetEffectiveTarget(40.0f, true, 23));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, GetEffectiveTarget(40.0f, true, 0));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 34.0f, GetEffectiveTarget(40.0f, true, 8));
}

void test_eco_mode_day_window_returns_full_target(void) {
  // ECO mode but outside night window (9h–17h): full target.
  TEST_ASSERT_EQUAL_FLOAT(40.0f, GetEffectiveTarget(40.0f, true, 9));
  TEST_ASSERT_EQUAL_FLOAT(40.0f, GetEffectiveTarget(40.0f, true, 12));
  TEST_ASSERT_EQUAL_FLOAT(40.0f, GetEffectiveTarget(40.0f, true, 17));
}

void test_eco_target_reduction_at_various_setpoints(void) {
  // Verify the 85% factor applies correctly at several setpoints.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f * 0.85f, GetEffectiveTarget(20.0f, true, 22));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 35.0f * 0.85f, GetEffectiveTarget(35.0f, true, 0));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f * 0.85f, GetEffectiveTarget(45.0f, true, 5));
}

// ===== ECO Night-Window Hour Boundary =====

void test_eco_window_starts_at_18h(void) {
  TEST_ASSERT_TRUE(IsEcoWindowActive(true, 18));
  TEST_ASSERT_FALSE(IsEcoWindowActive(true, 17));  // Just before start
}

void test_eco_window_ends_at_9h(void) {
  // Hour 8: inside window (< 9). Hour 9: outside window.
  TEST_ASSERT_TRUE(IsEcoWindowActive(true, 8));
  TEST_ASSERT_FALSE(IsEcoWindowActive(true, 9));
}

void test_eco_window_spans_midnight(void) {
  // Window covers 18h–23h and 0h–8h.
  for (uint8_t h = 18; h <= 23; h++)
    TEST_ASSERT_TRUE(IsEcoWindowActive(true, h));
  for (uint8_t h = 0; h < 9; h++)
    TEST_ASSERT_TRUE(IsEcoWindowActive(true, h));
}

void test_eco_window_inactive_during_day(void) {
  for (uint8_t h = 9; h <= 17; h++)
    TEST_ASSERT_FALSE(IsEcoWindowActive(true, h));
}

// ===== Temperature In-Range Check =====

void test_in_range_when_exactly_on_target(void) {
  TEST_ASSERT_TRUE(IsTemperatureInRange(40.0f, 40.0f));
}

void test_in_range_within_tolerance(void) {
  TEST_ASSERT_TRUE(IsTemperatureInRange(40.0f + kInRangeTolerance, 40.0f));
  TEST_ASSERT_TRUE(IsTemperatureInRange(40.0f - kInRangeTolerance, 40.0f));
}

void test_out_of_range_beyond_tolerance(void) {
  TEST_ASSERT_FALSE(IsTemperatureInRange(40.0f + kInRangeTolerance + 0.1f, 40.0f));
  TEST_ASSERT_FALSE(IsTemperatureInRange(40.0f - kInRangeTolerance - 0.1f, 40.0f));
}

void test_in_range_at_various_setpoints(void) {
  TEST_ASSERT_TRUE(IsTemperatureInRange(21.5f, 20.0f));   // target=20, T=21.5 (within 2)
  TEST_ASSERT_FALSE(IsTemperatureInRange(17.9f, 20.0f));  // target=20, T=17.9 (outside 2)
  TEST_ASSERT_TRUE(IsTemperatureInRange(44.0f, 45.0f));   // target=45, T=44 (within 2)
}

// ===== SetTargetTemperature Clamping =====

void test_target_clamped_to_minimum(void) {
  // Request below min (20°C) → clamped to 20°C.
  float result = ClampTarget(10.0f, 30.0f);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, result);
}

void test_target_clamped_to_maximum(void) {
  // Request above max (45°C) → clamped to 45°C.
  float result = ClampTarget(60.0f, 30.0f);
  TEST_ASSERT_EQUAL_FLOAT(45.0f, result);
}

void test_target_within_bounds_accepted(void) {
  TEST_ASSERT_EQUAL_FLOAT(35.0f, ClampTarget(35.0f, 30.0f));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, ClampTarget(20.0f, 25.0f));  // at minimum, 5°C change
  TEST_ASSERT_EQUAL_FLOAT(45.0f, ClampTarget(45.0f, 40.0f));  // at maximum, 5°C change
}

void test_target_deadband_ignores_small_change(void) {
  // Change < 1°C: current value returned unchanged.
  TEST_ASSERT_EQUAL_FLOAT(30.0f, ClampTarget(30.5f, 30.0f));  // 0.5°C delta
  TEST_ASSERT_EQUAL_FLOAT(30.0f, ClampTarget(30.9f, 30.0f));  // 0.9°C delta
  TEST_ASSERT_EQUAL_FLOAT(30.0f, ClampTarget(29.5f, 30.0f));  // 0.5°C below
}

void test_target_change_exactly_1_degree_triggers_update(void) {
  // Exactly 1°C change: not strictly less than 1 → update accepted.
  TEST_ASSERT_EQUAL_FLOAT(31.0f, ClampTarget(31.0f, 30.0f));
}

// ===== Realistic ECO Scenario =====

void test_eco_scenario_full_day_cycle(void) {
  // Simulate a 24-hour day for a 40°C target in ECO mode.
  // Night hours: 18h–8h → effective = 34°C. Day hours: 9h–17h → effective = 40°C.
  const float target = 40.0f;
  const float night_effective = target * (kEcoNightTargetPct / 100.0f);

  for (uint8_t h = 0; h <= 23; h++) {
    float effective = GetEffectiveTarget(target, true, h);
    if (h >= kEcoStartHour || h < kEcoEndHour) {
      TEST_ASSERT_FLOAT_WITHIN(0.01f, night_effective, effective);
    } else {
      TEST_ASSERT_EQUAL_FLOAT(target, effective);
    }
  }
}

// ===== Test Runner =====

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // ECO target calculation
  RUN_TEST(test_eco_mode_off_returns_full_target);
  RUN_TEST(test_eco_mode_night_window_reduces_target);
  RUN_TEST(test_eco_mode_day_window_returns_full_target);
  RUN_TEST(test_eco_target_reduction_at_various_setpoints);

  // ECO window hour boundaries
  RUN_TEST(test_eco_window_starts_at_18h);
  RUN_TEST(test_eco_window_ends_at_9h);
  RUN_TEST(test_eco_window_spans_midnight);
  RUN_TEST(test_eco_window_inactive_during_day);

  // Temperature in-range check
  RUN_TEST(test_in_range_when_exactly_on_target);
  RUN_TEST(test_in_range_within_tolerance);
  RUN_TEST(test_out_of_range_beyond_tolerance);
  RUN_TEST(test_in_range_at_various_setpoints);

  // Target clamping + dead-band
  RUN_TEST(test_target_clamped_to_minimum);
  RUN_TEST(test_target_clamped_to_maximum);
  RUN_TEST(test_target_within_bounds_accepted);
  RUN_TEST(test_target_deadband_ignores_small_change);
  RUN_TEST(test_target_change_exactly_1_degree_triggers_update);

  // Realistic scenarios
  RUN_TEST(test_eco_scenario_full_day_cycle);

  return UNITY_END();
}
