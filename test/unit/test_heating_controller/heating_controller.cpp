// Unit tests for TemperatureManager — the real class, compiled natively.
//
// v4 replaced the PID on the circulator with two independent on/off sources:
// the hydraulic as base heat (wide band, slow cycling) and the electric as fine
// trim (narrow band, predictive shutoff). These tests drive the production code
// directly through the shim clock, so a change in config.h or in the control
// logic shows up here instead of silently diverging.

#include <unity.h>

#include "TemperatureManager.h"
#include "ElectricHeater.h"

// --- Harness ----------------------------------------------------------------

namespace
{

struct Harness
{
  ElectricHeater     electric;
  TemperatureManager manager;

  Harness() : electric(), manager(&electric) {}

  // Bring the manager into a state where both sources are allowed to run.
  void Arm(bool hydraulic_online = true)
  {
    TestSetMillis(0);
    manager.Begin();
    manager.SetFanActive(true);
    manager.SetHeatingPermitted(true);
    manager.SetElectricEnabled(true);
    manager.SetHydraulicEnabled(true);
    manager.SetHydraulicOnline(hydraulic_online);
  }

  // Advance the clock by `seconds` and run one control tick at `temperature`.
  void Tick(float temperature, uint32_t seconds = 1)
  {
    TestAdvanceMillis(seconds * 1000);
    manager.Update(temperature);
  }

  // Hold a steady temperature for `seconds`, one tick per second.
  void Hold(float temperature, uint32_t seconds)
  {
    for (uint32_t i = 0; i < seconds; i++)
    {
      Tick(temperature, 1);
    }
  }
};

} // namespace

void setUp(void) {}
void tearDown(void) {}

// --- Interlocks -------------------------------------------------------------

void test_no_heating_without_fan(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);
  h.manager.SetFanActive(false);

  h.Hold(20.0f, 5);

  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
  TEST_ASSERT_EQUAL(static_cast<int>(ControlState::OFF),
                    static_cast<int>(h.manager.GetControlState()));
}

void test_no_heating_when_reading_is_stale(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  // Warm up normally, then lose the probe.
  h.Hold(20.0f, 5);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());

  h.manager.SetHeatingPermitted(false);
  h.Hold(20.0f, 5);

  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
}

void test_safety_cutoff_stops_both_sources(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(45.0f);

  h.Hold(20.0f, 5);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());

  // Above TEMPERATURE_SAFETY_MAX everything must drop out at once.
  h.Tick(TEMPERATURE_SAFETY_MAX + 1.0f);

  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
}

void test_sensor_fault_stops_both_sources(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);
  h.Hold(20.0f, 5);

  h.Tick(NAN);

  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
}

// --- Electric trim ----------------------------------------------------------

void test_electric_turns_on_beyond_its_band(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  h.Tick(39.0f); // error 1.0 > CTRL_BANDE_ELEC

  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
}

void test_electric_stays_off_inside_its_band(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  h.Hold(39.8f, 5); // error 0.2 < CTRL_BANDE_ELEC

  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
}

void test_electric_respects_minimum_on_time(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  h.Tick(39.0f);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());

  // Setpoint reached immediately, but the minimum ON time is not elapsed.
  h.Tick(41.0f);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());

  h.Hold(41.0f, static_cast<uint32_t>(CTRL_T_ON_MIN) + 2);
  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
}

void test_electric_respects_minimum_off_time(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  // Drive one full ON/OFF cycle.
  h.Tick(39.0f);
  h.Hold(41.0f, static_cast<uint32_t>(CTRL_T_ON_MIN) + 2);
  TEST_ASSERT_FALSE(h.manager.GetElectricOn());

  // Demand heat again straight away: the OFF timer must hold it back.
  h.Tick(39.0f);
  TEST_ASSERT_FALSE(h.manager.GetElectricOn());

  h.Hold(39.0f, static_cast<uint32_t>(CTRL_T_OFF_MIN) + 2);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
}

void test_predictive_shutoff_ignores_sensor_noise(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  // Settle just under the setpoint with a flat derivative.
  h.Tick(39.0f);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
  h.Hold(39.0f, static_cast<uint32_t>(CTRL_T_ON_MIN) + 2);

  // One isolated 0.1°C quantization step, then flat again. Filtered, that peaks
  // at ~0.03°C/s — below CTRL_DT_PREDICT_MIN. The naive prediction
  // 39.1 + 0.03 * 60 = 40.9 does cross the setpoint, so without the noise gate
  // this spike alone would cut the electric out.
  h.Tick(39.1f);
  h.Tick(39.1f);

  TEST_ASSERT_TRUE(h.manager.GetTemperatureDerivative() < CTRL_DT_PREDICT_MIN);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
}

void test_predictive_shutoff_fires_on_genuine_rise(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  h.Tick(35.0f);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
  h.Hold(35.0f, static_cast<uint32_t>(CTRL_T_ON_MIN) + 2);

  // A sustained 0.5°C/s climb predicts far past the setpoint within
  // CTRL_HORIZON, so the electric must cut out before overshooting.
  for (int i = 1; i <= 6 && h.manager.GetElectricOn(); i++)
  {
    h.Tick(35.0f + 0.5f * i);
  }

  TEST_ASSERT_TRUE(h.manager.GetTemperatureDerivative() > CTRL_DT_PREDICT_MIN);
  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
}

// --- Hydraulic base heat ----------------------------------------------------

void test_hydraulic_turns_on_beyond_its_wider_band(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  h.Tick(35.0f); // error 5.0 > CTRL_BANDE_HYDRO

  TEST_ASSERT_TRUE(h.manager.GetHydraulicOn());
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
}

void test_small_error_is_trimmed_by_electric_alone(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  // Error 1.0: above the electric band, below the hydraulic band.
  h.Hold(39.0f, 5);

  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
}

void test_hydraulic_respects_its_long_minimum_on_time(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  h.Tick(35.0f);
  TEST_ASSERT_TRUE(h.manager.GetHydraulicOn());

  // Setpoint reached, but the valve must not be cycled before its minimum.
  h.Hold(41.0f, static_cast<uint32_t>(CTRL_HYDRO_T_ON_MIN) - 10);
  TEST_ASSERT_TRUE(h.manager.GetHydraulicOn());

  h.Hold(41.0f, 20);
  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
}

void test_hydraulic_stays_off_when_module_is_offline(void)
{
  Harness h;
  h.Arm(/*hydraulic_online=*/false);
  h.manager.SetTargetTemperature(40.0f);

  h.Hold(30.0f, 5);

  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
  TEST_ASSERT_TRUE(h.manager.GetElectricOn()); // electric still trims
  TEST_ASSERT_EQUAL(static_cast<int>(ControlState::ELECTRIC_ONLY),
                    static_cast<int>(h.manager.GetControlState()));
}

void test_hydraulic_stays_off_when_disabled_in_menu(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);
  h.manager.SetHydraulicEnabled(false);

  h.Hold(30.0f, 5);

  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
}

void test_electric_stays_off_when_disabled_in_menu(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);
  h.manager.SetElectricEnabled(false);

  h.Hold(30.0f, 5);

  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
  TEST_ASSERT_TRUE(h.manager.GetHydraulicOn());
  TEST_ASSERT_EQUAL(static_cast<int>(ControlState::HYDRAULIC_ONLY),
                    static_cast<int>(h.manager.GetControlState()));
}

void test_both_disabled_reports_off(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);
  h.manager.SetElectricEnabled(false);
  h.manager.SetHydraulicEnabled(false);

  h.Hold(30.0f, 5);

  TEST_ASSERT_EQUAL(static_cast<int>(ControlState::OFF),
                    static_cast<int>(h.manager.GetControlState()));
}

// --- Setpoint and ECO -------------------------------------------------------

void test_target_is_clamped_to_menu_range(void)
{
  Harness h;
  h.Arm();

  h.manager.SetTargetTemperature(99.0f);
  TEST_ASSERT_EQUAL_FLOAT(TARGET_TEMP_MAX, h.manager.GetTargetTemperature());

  h.manager.SetTargetTemperature(-10.0f);
  TEST_ASSERT_EQUAL_FLOAT(TARGET_TEMP_MIN, h.manager.GetTargetTemperature());
}

void test_setpoint_change_preserves_anti_short_cycle(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  // Run the electric, then stop it so its OFF timer starts from zero.
  h.Tick(39.0f);
  h.Hold(41.0f, static_cast<uint32_t>(CTRL_T_ON_MIN) + 2);
  TEST_ASSERT_FALSE(h.manager.GetElectricOn());

  // v3 reset the timers on every setpoint change, which turned the menu into a
  // way to re-energise the contactor immediately. It must not.
  h.manager.SetTargetTemperature(44.0f);
  h.Tick(39.0f);

  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
}

void test_eco_window_reduces_setpoint_overnight(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);
  h.manager.SetOperatingMode(OperatingMode::ECO);

  h.manager.SetCurrentHour(22); // inside the 18h -> 9h window
  TEST_ASSERT_TRUE(h.manager.IsEcoWindowActive());
  TEST_ASSERT_EQUAL_FLOAT(40.0f * ECO_NIGHT_TARGET_PERCENTAGE / 100.0f,
                          h.manager.GetEffectiveTargetTemperature());

  h.manager.SetCurrentHour(12); // outside
  TEST_ASSERT_FALSE(h.manager.IsEcoWindowActive());
  TEST_ASSERT_EQUAL_FLOAT(40.0f, h.manager.GetEffectiveTargetTemperature());
}

void test_eco_window_ignored_in_performance_mode(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);
  h.manager.SetOperatingMode(OperatingMode::PERFORMANCE);
  h.manager.SetCurrentHour(22);

  TEST_ASSERT_FALSE(h.manager.IsEcoWindowActive());
  TEST_ASSERT_EQUAL_FLOAT(40.0f, h.manager.GetEffectiveTargetTemperature());
}

void test_eco_window_handles_non_wrapping_range(void)
{
  Harness h;
  h.Arm();
  h.manager.SetOperatingMode(OperatingMode::ECO);
  h.manager.GetParams().eco_start_hour = 9;
  h.manager.GetParams().eco_end_hour   = 18;

  h.manager.SetCurrentHour(12);
  TEST_ASSERT_TRUE(h.manager.IsEcoWindowActive());

  h.manager.SetCurrentHour(22);
  TEST_ASSERT_FALSE(h.manager.IsEcoWindowActive());
}

// --- Phase transitions ------------------------------------------------------

void test_reset_control_clears_both_sources(void)
{
  Harness h;
  h.Arm();
  h.manager.SetTargetTemperature(40.0f);

  h.Tick(30.0f);
  TEST_ASSERT_TRUE(h.manager.GetElectricOn());
  TEST_ASSERT_TRUE(h.manager.GetHydraulicOn());

  h.manager.ResetControl();

  TEST_ASSERT_FALSE(h.manager.GetElectricOn());
  TEST_ASSERT_FALSE(h.manager.GetHydraulicOn());
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  // Interlocks
  RUN_TEST(test_no_heating_without_fan);
  RUN_TEST(test_no_heating_when_reading_is_stale);
  RUN_TEST(test_safety_cutoff_stops_both_sources);
  RUN_TEST(test_sensor_fault_stops_both_sources);

  // Electric trim
  RUN_TEST(test_electric_turns_on_beyond_its_band);
  RUN_TEST(test_electric_stays_off_inside_its_band);
  RUN_TEST(test_electric_respects_minimum_on_time);
  RUN_TEST(test_electric_respects_minimum_off_time);
  RUN_TEST(test_predictive_shutoff_ignores_sensor_noise);
  RUN_TEST(test_predictive_shutoff_fires_on_genuine_rise);

  // Hydraulic base heat
  RUN_TEST(test_hydraulic_turns_on_beyond_its_wider_band);
  RUN_TEST(test_small_error_is_trimmed_by_electric_alone);
  RUN_TEST(test_hydraulic_respects_its_long_minimum_on_time);
  RUN_TEST(test_hydraulic_stays_off_when_module_is_offline);
  RUN_TEST(test_hydraulic_stays_off_when_disabled_in_menu);
  RUN_TEST(test_electric_stays_off_when_disabled_in_menu);
  RUN_TEST(test_both_disabled_reports_off);

  // Setpoint and ECO
  RUN_TEST(test_target_is_clamped_to_menu_range);
  RUN_TEST(test_setpoint_change_preserves_anti_short_cycle);
  RUN_TEST(test_eco_window_reduces_setpoint_overnight);
  RUN_TEST(test_eco_window_ignored_in_performance_mode);
  RUN_TEST(test_eco_window_handles_non_wrapping_range);

  // Phase transitions
  RUN_TEST(test_reset_control_clears_both_sources);

  return UNITY_END();
}
