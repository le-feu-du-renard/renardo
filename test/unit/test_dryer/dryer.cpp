// Unit tests for Dryer — the coordinator, compiled natively with every manager
// it owns.
//
// What is pinned down here is the one decision Dryer makes that no manager
// could: why a dehumidifier's register is being held open. Both reasons are air
// renewals, both have hysteresis, and both have to announce themselves to the
// regulation — a register that travels without NotifyAirRenewal() is the exact
// fault the air-renewal window was introduced to fix.

#include <unity.h>

#include "Dryer.h"

namespace
{

using ForceOpen = HumidityManager::ForceOpen;

struct Harness
{
  Dryer         dryer;
  DryerSettings settings;

  // A running session with a dehumidifier fitted, a 40 C setpoint and a 50 %RH
  // target. The extraction register is given a live feedback reading, since
  // Start() refuses on a dead one and a fault would be answering these tests
  // instead of the control law.
  void Arm(uint8_t heat_source, float target_humidity = 50.0f)
  {
    TestSetMillis(0);
    settings.Reset();
    settings.heat_source        = heat_source;
    settings.target_temperature = 40.0f;
    settings.target_humidity    = target_humidity;
    settings.hydraulic_enabled  = false;

    dryer.Begin();
    dryer.ApplySettings(settings, false);

    // Past STATUS_FAULT_GRACE_MS, so FaultReason() is answering about the
    // machine rather than about the boot window, and Start() will take.
    TestAdvanceMillis(STATUS_FAULT_GRACE_MS + 1000);
    FeedDamperPosition();
    Feed(30.0f, 60.0f);
    dryer.GetTemperatureManager()->SetHeatingPermitted(true);
    dryer.Start();
    TEST_ASSERT_TRUE(dryer.IsRunning());
  }

  // Mid-travel, comfortably inside the calibrated range and well clear of
  // DAMPER_SIGNAL_MIN_RAW, confirmed over enough samples to count as a signal.
  void FeedDamperPosition()
  {
    const uint16_t raw = (DAMPER_RAW_MIN_DEFAULT + DAMPER_RAW_MAX_DEFAULT) / 2;
    for (uint8_t i = 0; i < DAMPER_SIGNAL_CONFIRM_SAMPLES + 1; i++)
    {
      dryer.GetAirDamper()->Extraction().SetRawPosition(raw);
    }
  }

  void Feed(float temperature, float humidity)
  {
    dryer.SetInletTemperature(temperature);
    dryer.SetInletHumidity(humidity);
  }

  // Hold both readings for `seconds`, one control interval per second.
  void Hold(float temperature, float humidity, uint32_t seconds)
  {
    for (uint32_t i = 0; i < seconds; i++)
    {
      Feed(temperature, humidity);
      FeedDamperPosition();
      TestAdvanceMillis(CONTROL_LOOP_INTERVAL);
      dryer.Update();
    }
  }

  ForceOpen Reason() const
  {
    return const_cast<Dryer &>(dryer).GetHumidityManager()->GetForceOpen();
  }

  float RenewalRemaining() const
  {
    return const_cast<Dryer &>(dryer).GetTemperatureManager()->GetAirRenewalRemaining();
  }
};

} // namespace

void setUp(void) {}
void tearDown(void) {}

// --- An electric source never forces the register -----------------------------

void test_electric_source_never_forces_the_register_open(void)
{
  // A resistance throws its air away as a matter of course; the phase machine
  // owns the register and neither of the two reasons below exists for it.
  Harness h;
  h.Arm(HEAT_SOURCE_ELECTRIC);

  h.Hold(60.0f, 90.0f, 5);   // far too hot
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));

  h.Hold(20.0f, 5.0f, 5);    // far too dry
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));
}

// --- Too hot ------------------------------------------------------------------

void test_overheat_opens_past_the_threshold_and_closes_at_the_setpoint(void)
{
  Harness h;
  h.Arm(HEAT_SOURCE_DEHUMIDIFIER);
  const float threshold = DEHUM_EXTRACTION_THRESHOLD_DEFAULT;

  // Above the setpoint but inside the threshold: nothing yet. The dehumidifier
  // is allowed to overshoot a little, that is what the threshold is for.
  h.Hold(40.0f + threshold - 0.5f, 60.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));

  h.Hold(40.0f + threshold + 0.5f, 60.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kOverheat), static_cast<int>(h.Reason()));

  // The threshold is the hysteresis: it stays open all the way back down to the
  // setpoint, not merely back under the threshold.
  h.Hold(40.0f + threshold - 0.5f, 60.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kOverheat), static_cast<int>(h.Reason()));

  h.Hold(39.5f, 60.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));
}

// --- Too dry ------------------------------------------------------------------

void test_dry_air_is_renewed_and_the_band_holds_it_open(void)
{
  Harness h;
  h.Arm(HEAT_SOURCE_DEHUMIDIFIER);

  // Above target: there is water to condense, the machine has work to do.
  h.Hold(38.0f, 60.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));

  // Below target: nothing left to condense, so renew the air and carry on.
  h.Hold(38.0f, 45.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kAirRenewal), static_cast<int>(h.Reason()));

  // Just back over the target is not enough — the register takes 150 s to
  // travel, and hunting on the target itself would have it moving constantly.
  h.Hold(38.0f, 51.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kAirRenewal), static_cast<int>(h.Reason()));

  h.Hold(38.0f, 50.0f + DEHUM_RENEWAL_BAND + 1.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));
}

void test_no_humidity_target_means_no_renewal(void)
{
  // Zero is the "no target set" convention. Read as a target it would put the
  // register open for ever, since no reading is below zero.
  Harness h;
  h.Arm(HEAT_SOURCE_DEHUMIDIFIER, 0.0f);

  h.Hold(38.0f, 5.0f, 5);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));
}

void test_a_dead_humidity_reading_does_not_renew(void)
{
  Harness h;
  h.Arm(HEAT_SOURCE_DEHUMIDIFIER);

  h.Hold(38.0f, NAN, 5);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));
}

// --- Ranking ------------------------------------------------------------------

void test_overheating_outranks_dry_air(void)
{
  // Both true at once. They want the same register, so the answer is which one
  // is being reported, not which one applies — and the urgent one wins.
  Harness h;
  h.Arm(HEAT_SOURCE_DEHUMIDIFIER);

  h.Hold(50.0f, 10.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kOverheat), static_cast<int>(h.Reason()));
}

// --- Every movement is announced ----------------------------------------------

void test_forcing_the_register_open_announces_an_air_renewal(void)
{
  // The window exists because the chamber's recovery after a register moves is
  // steeper than any approach to setpoint the predictive shutoff was sized for.
  // These two movements are the ones that could previously happen with nothing
  // anywhere being told.
  Harness h;
  h.Arm(HEAT_SOURCE_DEHUMIDIFIER);

  // Let any window from the session start expire.
  h.Hold(38.0f, 60.0f, (uint32_t)CTRL_AIR_RENEWAL_S + 5);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, h.RenewalRemaining());

  h.Hold(38.0f, 40.0f, 1);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kAirRenewal), static_cast<int>(h.Reason()));
  TEST_ASSERT_TRUE(h.RenewalRemaining() > 0.0f);
}

void test_closing_the_register_announces_one_too(void)
{
  // Coming back is as much a change of air as going out, and it is the half
  // that used to do the damage: the chamber recovers fastest with the register
  // shut, which is exactly what the prediction misreads.
  Harness h;
  h.Arm(HEAT_SOURCE_DEHUMIDIFIER);

  h.Hold(38.0f, 40.0f, 3);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kAirRenewal), static_cast<int>(h.Reason()));

  // Let the window from the opening run all the way out, so what is measured
  // after the close is the close's own window and not the leftovers of the
  // opening's.
  h.Hold(38.0f, 40.0f, (uint32_t)CTRL_AIR_RENEWAL_S + 5);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, h.RenewalRemaining());

  h.Hold(38.0f, 60.0f, 1);
  TEST_ASSERT_EQUAL(static_cast<int>(ForceOpen::kNone), static_cast<int>(h.Reason()));
  TEST_ASSERT_TRUE(h.RenewalRemaining() > 0.0f);
}

int main(int argc, char **argv)
{
  UNITY_BEGIN();
  RUN_TEST(test_electric_source_never_forces_the_register_open);
  RUN_TEST(test_overheat_opens_past_the_threshold_and_closes_at_the_setpoint);
  RUN_TEST(test_dry_air_is_renewed_and_the_band_holds_it_open);
  RUN_TEST(test_no_humidity_target_means_no_renewal);
  RUN_TEST(test_a_dead_humidity_reading_does_not_renew);
  RUN_TEST(test_overheating_outranks_dry_air);
  RUN_TEST(test_forcing_the_register_open_announces_an_air_renewal);
  RUN_TEST(test_closing_the_register_announces_one_too);
  return UNITY_END();
}
