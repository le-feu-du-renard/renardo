#include <unity.h>
#include <cmath>

/**
 * Unit tests for TemperatureManager PID logic
 * Tests water temperature constraint and PID behavior
 */

// Mock heater classes for testing
class MockElectricHeater {
 public:
  MockElectricHeater() : power_(0.0f) {}
  void Begin() {}
  void Update() {}
  void SetPower(float power) { power_ = power; }
  float GetPower() const { return power_; }
  float GetOutput() const { return power_ > 0.5f ? 1.0f : 0.0f; }
 private:
  float power_;
};

class MockHydraulicHeater {
 public:
  MockHydraulicHeater() : power_(0) {}
  void Begin() {}
  void Update() {}
  void SetPower(uint8_t power) { power_ = power; }
  uint8_t GetPower() const { return power_; }
  float GetOutput() const { return (float)power_ / 100.0f; }
 private:
  uint8_t power_;
};

// Test structure for temperature manager logic
struct TestTempManager {
  float setpoint;
  float water_temp;
  float air_temp;
  float water_margin;
  uint8_t hydraulic_power;  // Current power (for checking if circulator is running)

  TestTempManager()
    : setpoint(40.0f),
      water_temp(45.0f),
      air_temp(38.0f),
      water_margin(2.0f),
      hydraulic_power(50) {}
};

// Replicate water temperature validation logic
bool IsWaterTemperatureValid(const TestTempManager& tm) {
  // Water temperature is valid if:
  // 1. It's within reasonable range (5-95°C)
  // 2. Hydraulic heater is running (circulator active, power > 0)

  if (tm.water_temp < 5.0f || tm.water_temp > 95.0f) {
    return false;
  }

  if (tm.hydraulic_power == 0) {
    return false;  // Circulator off, water is stagnant
  }

  return true;
}

// Check if hydraulic should be constrained by water temperature
bool ShouldConstrainHydraulic(const TestTempManager& tm) {
  if (!IsWaterTemperatureValid(tm)) {
    return false;  // Water temp not valid, don't constrain
  }

  float min_water_temp = tm.setpoint + tm.water_margin;
  return tm.water_temp < min_water_temp;
}

void setUp(void) {
  // Called before each test
}

void tearDown(void) {
  // Called after each test
}

// ===== Water Temperature Validation Tests =====

void test_water_temp_valid_when_in_range_and_circulating(void) {
  TestTempManager tm;
  tm.water_temp = 45.0f;
  tm.hydraulic_power = 50;

  TEST_ASSERT_TRUE(IsWaterTemperatureValid(tm));
}

void test_water_temp_invalid_when_too_cold(void) {
  TestTempManager tm;
  tm.water_temp = 4.0f;
  tm.hydraulic_power = 50;

  TEST_ASSERT_FALSE(IsWaterTemperatureValid(tm));
}

void test_water_temp_invalid_when_too_hot(void) {
  TestTempManager tm;
  tm.water_temp = 96.0f;
  tm.hydraulic_power = 50;

  TEST_ASSERT_FALSE(IsWaterTemperatureValid(tm));
}

void test_water_temp_invalid_when_circulator_off(void) {
  TestTempManager tm;
  tm.water_temp = 45.0f;
  tm.hydraulic_power = 0;  // Circulator off

  TEST_ASSERT_FALSE(IsWaterTemperatureValid(tm));
}

void test_water_temp_valid_at_boundaries(void) {
  TestTempManager tm;
  tm.hydraulic_power = 50;

  // At lower boundary
  tm.water_temp = 5.0f;
  TEST_ASSERT_TRUE(IsWaterTemperatureValid(tm));

  // At upper boundary
  tm.water_temp = 95.0f;
  TEST_ASSERT_TRUE(IsWaterTemperatureValid(tm));
}

// ===== Hydraulic Constraint Tests =====

void test_hydraulic_constrained_when_water_too_cold(void) {
  TestTempManager tm;
  tm.setpoint = 40.0f;
  tm.water_margin = 2.0f;
  tm.water_temp = 41.0f;  // 40 + 2 = 42, so 41 < 42 => constrained
  tm.hydraulic_power = 50;

  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));
}

void test_hydraulic_not_constrained_when_water_hot_enough(void) {
  TestTempManager tm;
  tm.setpoint = 40.0f;
  tm.water_margin = 2.0f;
  tm.water_temp = 43.0f;  // 43 > 42 => not constrained
  tm.hydraulic_power = 50;

  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

void test_hydraulic_not_constrained_at_exact_margin(void) {
  TestTempManager tm;
  tm.setpoint = 40.0f;
  tm.water_margin = 2.0f;
  tm.water_temp = 42.0f;  // Exactly at margin
  tm.hydraulic_power = 50;

  // At the boundary, should not be constrained (>=)
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

void test_hydraulic_not_constrained_when_circulator_off(void) {
  TestTempManager tm;
  tm.setpoint = 40.0f;
  tm.water_margin = 2.0f;
  tm.water_temp = 35.0f;  // Way below margin
  tm.hydraulic_power = 0;  // Circulator off

  // Water temp not valid, so no constraint
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

void test_hydraulic_constrained_with_different_margins(void) {
  TestTempManager tm;
  tm.setpoint = 50.0f;
  tm.hydraulic_power = 50;

  // Margin = 5°C, min water = 55°C, water = 54°C => constrained (54 < 55)
  tm.water_margin = 5.0f;
  tm.water_temp = 54.0f;
  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));

  // Margin = 5°C, water = 55°C => not constrained (55 >= 55)
  tm.water_temp = 55.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));

  // Margin = 5°C, water = 56°C => not constrained (56 >= 55)
  tm.water_temp = 56.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

// ===== Mock Heater Tests =====

void test_electric_heater_binary_behavior(void) {
  MockElectricHeater heater;

  // Set to 0 => output 0
  heater.SetPower(0.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, heater.GetOutput());

  // Set to 0.4 => output 0 (below threshold)
  heater.SetPower(0.4f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, heater.GetOutput());

  // Set to 0.6 => output 1 (above threshold)
  heater.SetPower(0.6f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, heater.GetOutput());

  // Set to 1.0 => output 1
  heater.SetPower(1.0f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, heater.GetOutput());
}

void test_hydraulic_heater_proportional_behavior(void) {
  MockHydraulicHeater heater;

  // Set to 0% => output 0.0
  heater.SetPower(0);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, heater.GetOutput());

  // Set to 50% => output 0.5
  heater.SetPower(50);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, heater.GetOutput());

  // Set to 100% => output 1.0
  heater.SetPower(100);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, heater.GetOutput());

  // Set to 25% => output 0.25
  heater.SetPower(25);
  TEST_ASSERT_EQUAL_FLOAT(0.25f, heater.GetOutput());
}

// ===== Edge Cases =====

void test_water_temp_constraint_with_zero_margin(void) {
  TestTempManager tm;
  tm.setpoint = 40.0f;
  tm.water_margin = 0.0f;
  tm.hydraulic_power = 50;

  // Water = 39.9°C => constrained (39.9 < 40.0)
  tm.water_temp = 39.9f;
  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));

  // Water = 40.0°C => not constrained (40.0 >= 40.0)
  tm.water_temp = 40.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));

  // Water = 40.1°C => not constrained
  tm.water_temp = 40.1f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

void test_water_temp_constraint_with_large_margin(void) {
  TestTempManager tm;
  tm.setpoint = 40.0f;
  tm.water_margin = 10.0f;
  tm.hydraulic_power = 50;

  // Min water temp = 40 + 10 = 50°C
  // Water = 49°C => constrained
  tm.water_temp = 49.0f;
  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));

  // Water = 50°C => not constrained
  tm.water_temp = 50.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));

  // Water = 51°C => not constrained
  tm.water_temp = 51.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

void test_water_temp_constraint_at_low_setpoint(void) {
  TestTempManager tm;
  tm.setpoint = 20.0f;
  tm.water_margin = 2.0f;
  tm.hydraulic_power = 50;

  // Min water temp = 20 + 2 = 22°C
  tm.water_temp = 21.0f;
  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));

  tm.water_temp = 22.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

void test_water_temp_constraint_at_high_setpoint(void) {
  TestTempManager tm;
  tm.setpoint = 60.0f;
  tm.water_margin = 2.0f;
  tm.hydraulic_power = 50;

  // Min water temp = 60 + 2 = 62°C
  tm.water_temp = 61.0f;
  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));

  tm.water_temp = 62.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

// ===== Circulator State Tests =====

void test_circulator_on_allows_constraint(void) {
  TestTempManager tm;
  tm.setpoint = 40.0f;
  tm.water_margin = 2.0f;
  tm.water_temp = 41.0f;  // Below minimum
  tm.hydraulic_power = 1;  // Minimal power, but still on

  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));
}

void test_circulator_at_full_power_allows_constraint(void) {
  TestTempManager tm;
  tm.setpoint = 40.0f;
  tm.water_margin = 2.0f;
  tm.water_temp = 41.0f;  // Below minimum
  tm.hydraulic_power = 100;  // Full power

  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));
}

void test_multiple_constraints_scenarios(void) {
  TestTempManager tm;
  tm.setpoint = 35.0f;
  tm.water_margin = 2.0f;  // Min water = 37°C
  tm.hydraulic_power = 50;

  // Scenario 1: Water well above minimum
  tm.water_temp = 45.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));

  // Scenario 2: Water slightly above minimum
  tm.water_temp = 37.5f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));

  // Scenario 3: Water at minimum
  tm.water_temp = 37.0f;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));

  // Scenario 4: Water slightly below minimum
  tm.water_temp = 36.9f;
  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));

  // Scenario 5: Water well below minimum
  tm.water_temp = 30.0f;
  TEST_ASSERT_TRUE(ShouldConstrainHydraulic(tm));

  // Scenario 6: Water below minimum but circulator off
  tm.water_temp = 30.0f;
  tm.hydraulic_power = 0;
  TEST_ASSERT_FALSE(ShouldConstrainHydraulic(tm));
}

// ===== Test Runner =====

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Water temperature validation tests
  RUN_TEST(test_water_temp_valid_when_in_range_and_circulating);
  RUN_TEST(test_water_temp_invalid_when_too_cold);
  RUN_TEST(test_water_temp_invalid_when_too_hot);
  RUN_TEST(test_water_temp_invalid_when_circulator_off);
  RUN_TEST(test_water_temp_valid_at_boundaries);

  // Hydraulic constraint tests
  RUN_TEST(test_hydraulic_constrained_when_water_too_cold);
  RUN_TEST(test_hydraulic_not_constrained_when_water_hot_enough);
  RUN_TEST(test_hydraulic_not_constrained_at_exact_margin);
  RUN_TEST(test_hydraulic_not_constrained_when_circulator_off);
  RUN_TEST(test_hydraulic_constrained_with_different_margins);

  // Mock heater tests
  RUN_TEST(test_electric_heater_binary_behavior);
  RUN_TEST(test_hydraulic_heater_proportional_behavior);

  // Edge cases
  RUN_TEST(test_water_temp_constraint_with_zero_margin);
  RUN_TEST(test_water_temp_constraint_with_large_margin);
  RUN_TEST(test_water_temp_constraint_at_low_setpoint);
  RUN_TEST(test_water_temp_constraint_at_high_setpoint);

  // Circulator state tests
  RUN_TEST(test_circulator_on_allows_constraint);
  RUN_TEST(test_circulator_at_full_power_allows_constraint);
  RUN_TEST(test_multiple_constraints_scenarios);

  return UNITY_END();
}
