// Unit tests for SessionManager — the real class, compiled natively.
//
// This suite used to test a hand-copied replica of the phase enum and of a
// GetNextPhase() that lived nowhere but here, under comments asking that it
// "must match SessionManager.h". It matched right up until the day it did not,
// and a test that asserts a stale model of the system is worse than no test:
// it is a green tick over a wrong answer. The real class links natively, so
// there was never a reason for the copy.
//
// Two programmes are covered. Drying cycles Init -> [Brassage -> Extraction]
// for ever; climate holds one phase and never leaves it. Under a dehumidifier
// the drying cycle loses Extraction, which would be emptying a circuit the
// machine is condensing out of.

#include <unity.h>

#include "SessionManager.h"
#include "ElectricHeater.h"
#include "AirDamper.h"

namespace
{

struct Harness
{
  ElectricHeater     electric;
  AirDamper          damper;
  TemperatureManager temperature;
  HumidityManager    humidity;
  SessionManager     session;

  Harness()
      : electric(), damper(), temperature(&electric), humidity(&damper),
        session(&temperature, &humidity)
  {
    // Not zero. GetPhaseElapsedTime() uses phase_start_ms_ == 0 as a "not
    // started" sentinel, so a session started at millis() == 0 reports no
    // elapsed time for ever and no phase would ever time out. Harmless on the
    // real board, where Start() is a button press seconds into the boot, but
    // the tests have to step off it.
    TestSetMillis(1000);
    temperature.Begin();
    humidity.Begin();
    session.Begin();
    temperature.SetFanActive(true);
    temperature.SetHeatingPermitted(true);
    temperature.SetElectricEnabled(true);
    temperature.SetTargetTemperature(40.0f);
  }

  // Advance by `seconds` and run one transition check, well under any setpoint
  // so a temperature-driven Init exit does not fire unasked.
  void Run(uint32_t seconds, float temperature_c = 20.0f, float humidity_rh = 30.0f)
  {
    for (uint32_t i = 0; i < seconds; i++)
    {
      TestAdvanceMillis(1000);
      session.Update(temperature_c, humidity_rh);
    }
  }

  DryerPhase Phase() const { return session.GetCurrentPhase(); }
};

} // namespace

void setUp(void) {}
void tearDown(void) {}

// --- Names and enum values ----------------------------------------------------

void test_phase_names(void)
{
  Harness h;
  TEST_ASSERT_EQUAL_STRING("Stop", h.session.GetCurrentPhaseName());

  h.session.Start();
  TEST_ASSERT_EQUAL_STRING("Init", h.session.GetCurrentPhaseName());

  h.session.SetProgram(DryerProgram::kClimate);
  h.session.Stop();
  h.session.Start();
  TEST_ASSERT_EQUAL_STRING("Climat", h.session.GetCurrentPhaseName());
}

void test_phase_enum_values(void)
{
  // The screen indexes its style table by this value and asserts the order in
  // main.cpp, and SessionSnapshot stores it in flash. Neither can absorb a
  // renumbering, so the numbers are part of the contract.
  TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(DryerPhase::kStop));
  TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(DryerPhase::kInit));
  TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(DryerPhase::kBrassage));
  TEST_ASSERT_EQUAL_UINT8(3, static_cast<uint8_t>(DryerPhase::kExtraction));
  TEST_ASSERT_EQUAL_UINT8(4, static_cast<uint8_t>(DryerPhase::kClimat));
}

void test_factory_durations(void)
{
  Harness h;
  const PhaseDurations &d = h.session.GetDurations();
  TEST_ASSERT_EQUAL_UINT32(INIT_PHASE_DURATION, d.init);
  TEST_ASSERT_EQUAL_UINT32(BRASSAGE_PHASE_DURATION, d.brassage);
  TEST_ASSERT_EQUAL_UINT32(EXTRACTION_PHASE_DURATION, d.extraction);
}

// --- The drying cycle ---------------------------------------------------------

void test_drying_cycles_init_brassage_extraction_for_ever(void)
{
  Harness h;
  h.session.Start();
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kInit), static_cast<int>(h.Phase()));

  // Init ends on its timeout, since the temperature is held well below target.
  h.Run(INIT_PHASE_DURATION + 2);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kBrassage), static_cast<int>(h.Phase()));

  h.Run(BRASSAGE_PHASE_DURATION + 2);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kExtraction), static_cast<int>(h.Phase()));

  // Extraction returns to Brassage rather than ending anything: the cycle loops
  // until someone presses STOP.
  h.Run(EXTRACTION_PHASE_DURATION + 2);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kBrassage), static_cast<int>(h.Phase()));

  h.Run(BRASSAGE_PHASE_DURATION + 2);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kExtraction), static_cast<int>(h.Phase()));
}

void test_init_ends_early_when_the_temperature_is_reached(void)
{
  Harness h;
  h.session.Start();

  h.Run(5, 45.0f);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kBrassage), static_cast<int>(h.Phase()));
}

void test_stopped_session_does_not_transition(void)
{
  Harness h;
  h.Run(INIT_PHASE_DURATION * 2);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kStop), static_cast<int>(h.Phase()));
}

// --- The climate programme ----------------------------------------------------

void test_climate_starts_in_its_own_phase_and_never_leaves(void)
{
  Harness h;
  h.session.SetProgram(DryerProgram::kClimate);
  h.session.Start();

  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kClimat), static_cast<int>(h.Phase()));

  // Longer than every drying duration put together, at a temperature and a
  // humidity that would have ended any of them.
  h.Run(INIT_PHASE_DURATION + BRASSAGE_PHASE_DURATION + 60, 45.0f, 90.0f);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kClimat), static_cast<int>(h.Phase()));
}

void test_climate_puts_the_register_on_the_humidity_threshold(void)
{
  // kThreshold has been implemented since v3 and selected by nothing. Holding a
  // climate is what it was written for: open while the air is too damp, shut
  // once it is not.
  Harness h;
  h.session.SetProgram(DryerProgram::kClimate);
  h.session.Start();

  TEST_ASSERT_EQUAL(static_cast<int>(HumidityManager::Mode::kThreshold),
                    static_cast<int>(h.humidity.GetMode()));
}

void test_climate_with_a_dehumidifier_leaves_the_register_shut(void)
{
  // A dehumidifier takes the water out without opening anything. A humidity
  // threshold underneath would be a second opinion on the same vane; its
  // register answers overheating and dry air from Dryer instead.
  Harness h;
  h.temperature.SetHeatSource(HeatSourceType::kDehumidifier);
  h.session.SetProgram(DryerProgram::kClimate);
  h.session.Start();

  TEST_ASSERT_EQUAL(static_cast<int>(HumidityManager::Mode::kDisabled),
                    static_cast<int>(h.humidity.GetMode()));
}

void test_the_programme_cannot_change_under_a_running_session(void)
{
  // The phase machine is already inside a programme. The menu greys the entry
  // out; this is the half of that guarantee the menu cannot make.
  Harness h;
  h.session.Start();
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kInit), static_cast<int>(h.Phase()));

  h.session.SetProgram(DryerProgram::kClimate);
  h.Run(10);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kInit), static_cast<int>(h.Phase()));
}

// --- A dehumidifier has no use for Extraction ---------------------------------

void test_dehumidifier_drying_never_reaches_extraction(void)
{
  Harness h;
  h.temperature.SetHeatSource(HeatSourceType::kDehumidifier);
  h.session.Start();

  h.Run(INIT_PHASE_DURATION + 2);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kBrassage), static_cast<int>(h.Phase()));

  // Brassage is now the whole running state: it does not time out into
  // Extraction, and it does not end on the humidity threshold either.
  h.Run(BRASSAGE_PHASE_DURATION * 2, 20.0f, 95.0f);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kBrassage), static_cast<int>(h.Phase()));
}

void test_a_source_swapped_mid_extraction_leaves_at_once(void)
{
  // The phase machine is left in a phase that no longer exists for it, part way
  // through a duration that no longer means anything.
  Harness h;
  h.session.Start();
  h.Run(INIT_PHASE_DURATION + 2);
  h.Run(BRASSAGE_PHASE_DURATION + 2);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kExtraction), static_cast<int>(h.Phase()));

  h.temperature.SetHeatSource(HeatSourceType::kDehumidifier);
  h.Run(1);
  TEST_ASSERT_EQUAL(static_cast<int>(DryerPhase::kBrassage), static_cast<int>(h.Phase()));
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_phase_names);
  RUN_TEST(test_phase_enum_values);
  RUN_TEST(test_factory_durations);

  RUN_TEST(test_drying_cycles_init_brassage_extraction_for_ever);
  RUN_TEST(test_init_ends_early_when_the_temperature_is_reached);
  RUN_TEST(test_stopped_session_does_not_transition);

  RUN_TEST(test_climate_starts_in_its_own_phase_and_never_leaves);
  RUN_TEST(test_climate_puts_the_register_on_the_humidity_threshold);
  RUN_TEST(test_climate_with_a_dehumidifier_leaves_the_register_shut);
  RUN_TEST(test_the_programme_cannot_change_under_a_running_session);

  RUN_TEST(test_dehumidifier_drying_never_reaches_extraction);
  RUN_TEST(test_a_source_swapped_mid_extraction_leaves_at_once);
  return UNITY_END();
}
