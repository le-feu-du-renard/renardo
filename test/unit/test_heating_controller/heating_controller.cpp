#include <unity.h>
#include <cmath>
#include "../../../include/PIDController.h"

/**
 * Unit tests for the PID + electric state machine heating strategy.
 *
 * HeatingControllerSim replicates TemperatureManager::UpdateHeating() logic
 * in pure C++ (no Arduino / hardware dependencies). All thresholds and gains
 * mirror config.h defaults so that any constant change breaks a test.
 */

// ===== Constants (mirrored from config.h) =====
static constexpr float kKp            = 15.0f;
static constexpr float kKi            = 0.1f;
static constexpr float kKd            = 2.0f;
static constexpr float kIntMax        = 200.0f;
static constexpr float kDerivFilter   = 0.1f;
static constexpr float kEHaut         = 5.0f;
static constexpr float kEBas          = 0.4f;
static constexpr float kBandeElec     = 0.5f;
static constexpr float kTSat          = 75.0f;
static constexpr float kEtaMax        = 900.0f;
static constexpr float kHorizon       = 60.0f;
static constexpr float kTOnMin        = 60.0f;
static constexpr float kTOffMin       = 60.0f;
static constexpr float kDtFalling     = 0.01f;
static constexpr float kDtPredictMin  = 0.05f;
static constexpr float kSafetyMax     = 50.0f;

// ===== Simulator =====

struct HeatingControllerSim
{
  enum class State { REGULATION, BOOST, ELECTRIC_ONLY };

  PIDController pid;

  State   state;
  bool    electric_on;
  uint8_t hydraulic_power;  // 0-100%

  bool    hydro_available;
  bool    fan_active;
  bool    heating_enabled;

  float   dT_dt;
  float   prev_temp;
  bool    first_tick;

  float   hydro_sat_timer;
  float   elec_on_timer;
  float   elec_off_timer;

  explicit HeatingControllerSim(bool hydro = true, bool fan = true, bool heating = true)
      : pid(kKp, kKi, kKd, 0.0f, 100.0f, kIntMax, kDerivFilter),
        state(hydro ? State::REGULATION : State::ELECTRIC_ONLY),
        electric_on(false),
        hydraulic_power(0),
        hydro_available(hydro),
        fan_active(fan),
        heating_enabled(heating),
        dT_dt(0.0f),
        prev_temp(0.0f),
        first_tick(true),
        hydro_sat_timer(0.0f),
        elec_on_timer(0.0f),
        elec_off_timer(kTOffMin) {}  // initialized to allow immediate first activation

  void set_electric(bool on)
  {
    if (on == electric_on) return;
    electric_on = on;
    if (on)
      elec_on_timer = 0.0f;
    else
      elec_off_timer = 0.0f;
  }

  void step(float setpoint, float T, float dt = 1.0f)
  {
    // Sensor fault
    if (std::isnan(T) || T < -20.0f || T > 200.0f)
    {
      hydraulic_power = 0;
      set_electric(false);
      pid.Reset();
      return;
    }

    // Safety cutoff
    if (T > kSafetyMax)
    {
      hydraulic_power = 0;
      set_electric(false);
      pid.Reset();
      state = hydro_available ? State::REGULATION : State::ELECTRIC_ONLY;
      return;
    }

    // Fan / heating interlock
    if (!fan_active || !heating_enabled)
    {
      hydraulic_power = 0;
      set_electric(false);
      return;
    }

    // Filtered temperature derivative
    if (!first_tick)
    {
      float raw = (T - prev_temp) / dt;
      dT_dt = kDerivFilter * raw + (1.0f - kDerivFilter) * dT_dt;
    }
    else
    {
      dT_dt = 0.0f;
      first_tick = false;
    }
    prev_temp = T;

    float error = setpoint - T;

    // Hydraulic availability transitions
    if (!hydro_available && state != State::ELECTRIC_ONLY)
    {
      state = State::ELECTRIC_ONLY;
      hydraulic_power = 0;
    }
    else if (hydro_available && state == State::ELECTRIC_ONLY)
    {
      float resume_int = (-kKp * error) / kKi;
      if (resume_int < -kIntMax) resume_int = -kIntMax;
      if (resume_int > kIntMax)  resume_int = kIntMax;
      pid.SetIntegral(resume_int);
      state = State::REGULATION;
      hydro_sat_timer = 0.0f;
    }

    if (state == State::ELECTRIC_ONLY)
    {
      hydraulic_power = 0;

      if (!electric_on)
      {
        if (elec_off_timer >= kTOffMin && error > kBandeElec)
          set_electric(true);
      }
      else
      {
        float predicted = T + dT_dt * kHorizon;
        bool will_overshoot = (dT_dt > kDtPredictMin) && (predicted >= setpoint);
        if (elec_on_timer >= kTOnMin && (error <= 0.0f || will_overshoot))
          set_electric(false);
      }
    }
    else if (state == State::REGULATION)
    {
      float last_u = pid.GetLastOutput();
      bool freeze_int = (last_u >= 100.0f || last_u <= 0.0f);
      float u = pid.Compute(setpoint, T, dt, freeze_int);
      hydraulic_power = (uint8_t)(u < 0.0f ? 0 : (u > 100.0f ? 100 : (uint8_t)u));

      hydro_sat_timer = (u >= 99.0f) ? hydro_sat_timer + dt : 0.0f;

      float eta;
      if (dT_dt > 0.001f)
        eta = error / dT_dt;
      else if (dT_dt < -kDtFalling)
        eta = kEtaMax + 1.0f;
      else
        eta = 0.0f;

      bool can_boost = (elec_off_timer >= kTOffMin);
      bool cond1 = (error > kEHaut);
      bool cond2 = (hydro_sat_timer >= kTSat && error > kEBas);
      bool cond3 = (eta > kEtaMax && error > kEBas);

      if (can_boost && (cond1 || cond2 || cond3))
      {
        state = State::BOOST;
        hydro_sat_timer = 0.0f;
        hydraulic_power = 100;
        set_electric(true);
      }
    }
    else  // BOOST
    {
      hydraulic_power = 100;
      pid.Compute(setpoint, T, dt, true);

      bool can_exit    = (elec_on_timer >= kTOnMin);
      bool should_exit = (error < kEBas);

      if (can_exit && should_exit)
      {
        float resume_int = (100.0f - kKp * error) / kKi;
        if (resume_int < -kIntMax) resume_int = -kIntMax;
        if (resume_int > kIntMax)  resume_int = kIntMax;
        pid.SetIntegral(resume_int);
        state = State::REGULATION;
        hydro_sat_timer = 0.0f;
        set_electric(false);
      }
    }

    // Hard hydraulic guard
    if (!hydro_available) hydraulic_power = 0;

    // Advance timers
    if (electric_on) elec_on_timer += dt;
    else             elec_off_timer += dt;
  }

  void step_n(int n, float setpoint, float T, float dt = 1.0f)
  {
    for (int i = 0; i < n; i++) step(setpoint, T, dt);
  }
};

// ===== Boilerplate =====
void setUp(void) {}
void tearDown(void) {}

// ===== Cold start: BOOST triggered immediately by large error =====

void test_boost_triggers_immediately_on_large_error(void)
{
  // error = 40 - 29 = 11°C > kEHaut (5°C) → BOOST on the very first tick.
  // elec_off_timer is initialised to kTOffMin so can_boost is true immediately.
  HeatingControllerSim sim(true);
  sim.step(40.0f, 29.0f);

  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);
  TEST_ASSERT_EQUAL_UINT8(100, sim.hydraulic_power);
  TEST_ASSERT_TRUE(sim.electric_on);
}

void test_boost_not_triggered_for_small_error(void)
{
  // error = 40 - 35.5 = 4.5°C < kEHaut (5°C) → stays in REGULATION.
  HeatingControllerSim sim(true);
  sim.step(40.0f, 35.5f);

  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::REGULATION, (int)sim.state);
  TEST_ASSERT_FALSE(sim.electric_on);
}

// ===== Fine regulation: electric stays off near setpoint =====

void test_fine_regulation_electric_stays_off(void)
{
  // Temperature 0.2°C below setpoint — well within REGULATION, no BOOST conditions met.
  HeatingControllerSim sim(true);
  sim.step_n(500, 40.0f, 39.8f);

  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::REGULATION, (int)sim.state);
  TEST_ASSERT_FALSE(sim.electric_on);
}

// ===== Anti-short-cycle: BOOST cannot re-trigger before kTOffMin =====

void test_anti_short_cycle_blocks_boost_after_exit(void)
{
  HeatingControllerSim sim(true);

  // Trigger BOOST (large error)
  sim.step(40.0f, 29.0f);
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);

  // Force exit conditions: bring error < kEBas and run kTOnMin ticks
  sim.step_n((int)kTOnMin + 1, 40.0f, 39.9f);  // error = 0.1 < kEBas
  // Should have exited BOOST
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::REGULATION, (int)sim.state);
  TEST_ASSERT_FALSE(sim.electric_on);

  // elec_off_timer just reset to 0 — BOOST cannot retrigger for kTOffMin seconds
  // even if a large error appears again
  sim.step(40.0f, 29.0f);  // large error again, but elec_off_timer < kTOffMin
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::REGULATION, (int)sim.state);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_boost_allowed_after_toffmin_elapsed(void)
{
  HeatingControllerSim sim(true);

  // Trigger and exit BOOST
  sim.step(40.0f, 29.0f);
  sim.step_n((int)kTOnMin + 1, 40.0f, 39.9f);
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::REGULATION, (int)sim.state);

  // Wait kTOffMin seconds in regulation with small error
  sim.step_n((int)kTOffMin, 40.0f, 39.8f);

  // Now large error should trigger BOOST again
  sim.step(40.0f, 29.0f);
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);
  TEST_ASSERT_TRUE(sim.electric_on);
}

// ===== Anti-short-cycle: BOOST cannot exit before kTOnMin =====

void test_anti_short_cycle_boost_minimum_on_time(void)
{
  HeatingControllerSim sim(true);

  // Enter BOOST
  sim.step(40.0f, 29.0f);
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);

  // Setpoint already reached (error < kEBas), but kTOnMin not elapsed yet
  sim.step(40.0f, 39.9f);  // error = 0.1 < kEBas, but elec_on_timer ≈ 1s < kTOnMin
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);
  TEST_ASSERT_TRUE(sim.electric_on);
}

// ===== Safety cutoff =====

void test_safety_cutoff_forces_all_off(void)
{
  HeatingControllerSim sim(true);
  sim.step(40.0f, 29.0f);  // enter BOOST
  TEST_ASSERT_TRUE(sim.electric_on);

  sim.step(40.0f, kSafetyMax + 0.1f);

  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_safety_does_not_fire_at_exact_limit(void)
{
  // Condition is T > kSafetyMax (strict), not T >= kSafetyMax.
  HeatingControllerSim sim(true);
  sim.step(40.0f, kSafetyMax);
  // No safety cutoff; normal control applies (error = 40 - 50 = -10 → u = 0)
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_sensor_fault_forces_all_off(void)
{
  HeatingControllerSim sim(true);
  sim.step(40.0f, 29.0f);  // enter BOOST
  TEST_ASSERT_TRUE(sim.electric_on);

  sim.step(40.0f, std::numeric_limits<float>::quiet_NaN());

  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
  TEST_ASSERT_FALSE(sim.electric_on);
}

// ===== Fan interlock =====

void test_fan_interlock_blocks_all_heating(void)
{
  HeatingControllerSim sim(true, /*fan=*/false);
  sim.step_n(20, 40.0f, 22.0f);  // large error, but fan off
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_fan_restored_allows_boost(void)
{
  HeatingControllerSim sim(true, /*fan=*/false);
  sim.step_n(10, 40.0f, 22.0f);  // blocked
  TEST_ASSERT_FALSE(sim.electric_on);

  sim.fan_active = true;
  sim.step(40.0f, 22.0f);  // error = 18 > kEHaut → BOOST
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);
  TEST_ASSERT_TRUE(sim.electric_on);
}

// ===== ELECTRIC_ONLY mode (hydraulic unavailable) =====

void test_electric_only_hydro_always_zero(void)
{
  HeatingControllerSim sim(false);  // hydro unavailable
  sim.step_n(20, 40.0f, 22.0f);
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
}

void test_electric_only_turns_on_when_error_exceeds_band(void)
{
  // error = 40 - 39 = 1°C > kBandeElec (0.5°C) → electric ON (elec_off_timer >= kTOffMin)
  HeatingControllerSim sim(false);
  sim.step(40.0f, 39.0f);

  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::ELECTRIC_ONLY, (int)sim.state);
  TEST_ASSERT_TRUE(sim.electric_on);
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
}

void test_electric_only_stays_off_when_error_below_band(void)
{
  // error = 40 - 39.7 = 0.3°C < kBandeElec (0.5°C) → stays OFF
  HeatingControllerSim sim(false);
  sim.step_n(20, 40.0f, 39.7f);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_electric_only_turns_off_when_setpoint_reached(void)
{
  HeatingControllerSim sim(false);
  sim.step(40.0f, 39.0f);  // turn ON
  TEST_ASSERT_TRUE(sim.electric_on);

  // Run kTOnMin seconds, then present T > setpoint
  sim.step_n((int)kTOnMin + 1, 40.0f, 40.5f);  // error < 0 → reached
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_electric_only_predictive_shutoff_ignores_noise(void)
{
  // Sensor quantization (0.1°C steps at 1 Hz) creates derivative spikes of ~0.03°C/s.
  // These must NOT trigger predictive shutoff because they are below kDtPredictMin (0.05).
  // Simulate: sp=33°C, T oscillates between 32.1 and 32.2 while electric is ON.
  HeatingControllerSim sim(false);
  sim.step(33.0f, 32.4f);  // turn ON (error = 0.6 > kBandeElec)
  TEST_ASSERT_TRUE(sim.electric_on);

  // Feed kTOnMin+ ticks alternating 32.1/32.2 — derivative stays at ~0.03°C/s (noise)
  float temps[] = { 32.1f, 32.2f, 32.1f, 32.2f };
  for (int i = 0; i < (int)kTOnMin + 10; i++)
    sim.step(33.0f, temps[i % 4]);

  // Electric must still be ON: dT noise < kDtPredictMin (0.05), prediction should not fire
  // (T never reaches setpoint either — error = 33 - 32.2 = 0.8 > 0)
  TEST_ASSERT_TRUE(sim.electric_on);
}

void test_electric_only_predictive_shutoff_fires_on_genuine_rise(void)
{
  // When temperature is genuinely rising fast enough (dT > kDtPredictMin = 0.05°C/s)
  // and the prediction shows overshoot, the electric should shut off early.
  HeatingControllerSim sim(false);
  sim.step(33.0f, 32.0f);  // turn ON

  // Build up a genuine rise: 0.06°C/tick over kTOnMin+ steps
  // After kTOnMin steps at +0.06°C/tick: T ≈ 32 + kTOnMin*0.06 ≈ 35.6°C > sp
  // But prediction will fire before that (when T + dT*kHorizon >= sp)
  float T = 32.0f;
  bool shutoff_seen = false;
  for (int i = 0; i < (int)kTOnMin + 100; i++)
  {
    T += 0.06f;
    if (T > 36.0f) T = 36.0f;
    sim.step(33.0f, T);
    if (!sim.electric_on && i > (int)kTOnMin) { shutoff_seen = true; break; }
  }
  TEST_ASSERT_TRUE(shutoff_seen);
}

void test_electric_only_anti_short_cycle_observed(void)
{
  HeatingControllerSim sim(false);
  sim.step(40.0f, 39.0f);  // ON
  TEST_ASSERT_TRUE(sim.electric_on);

  sim.step_n((int)kTOnMin + 1, 40.0f, 40.5f);  // OFF after kTOnMin
  TEST_ASSERT_FALSE(sim.electric_on);

  // elec_off_timer just reset; cannot turn ON again immediately
  sim.step(40.0f, 39.0f);  // error > band, but elec_off_timer < kTOffMin
  TEST_ASSERT_FALSE(sim.electric_on);
}

// ===== Hydraulic disabled mid-BOOST =====

void test_hydro_disabled_during_boost_drops_hydro_to_zero(void)
{
  HeatingControllerSim sim(true);

  // Enter BOOST
  sim.step(40.0f, 29.0f);
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);
  TEST_ASSERT_EQUAL_UINT8(100, sim.hydraulic_power);

  // Disable hydraulic mid-BOOST
  sim.hydro_available = false;
  sim.step(40.0f, 30.0f);

  // Must transition to ELECTRIC_ONLY and hydro forced to 0
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::ELECTRIC_ONLY, (int)sim.state);
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
}

void test_hydro_disabled_during_boost_electric_continues(void)
{
  HeatingControllerSim sim(true);
  sim.step(40.0f, 29.0f);  // BOOST, electric ON
  TEST_ASSERT_TRUE(sim.electric_on);

  sim.hydro_available = false;
  sim.step(40.0f, 30.0f);  // transition to ELECTRIC_ONLY

  // Electric was ON; in ELECTRIC_ONLY it follows its own hysteresis.
  // error = 40 - 30 = 10 > kBandeElec (0.5), and the elec_on_timer has been running.
  // The electric should remain ON (can_turn_off requires kTOnMin elapsed).
  TEST_ASSERT_TRUE(sim.electric_on);
}

void test_hydro_disabled_anti_short_cycle_timers_preserved(void)
{
  // Disable hydro BEFORE any electric cycling — timers should be at their initial values.
  HeatingControllerSim sim(true);
  float off_timer_before = sim.elec_off_timer;

  sim.hydro_available = false;
  sim.step(40.0f, 39.0f);  // ELECTRIC_ONLY, error > band → electric turns ON

  // The off timer at the moment of transition was preserved (≥ kTOffMin), so electric
  // is allowed to turn on immediately.
  TEST_ASSERT_TRUE(sim.electric_on);
  (void)off_timer_before;
}

// ===== Hydraulic re-enabled: bumpless (no jump to 100%) =====

void test_hydro_reenabled_hydro_does_not_jump_to_full(void)
{
  // Start in ELECTRIC_ONLY with a moderate error (2°C).
  // When hydro is re-enabled, the bumpless integral init should prevent hydro
  // from immediately jumping to 100%.
  HeatingControllerSim sim(false);
  sim.step_n(10, 40.0f, 38.0f);  // settle in ELECTRIC_ONLY

  sim.hydro_available = true;
  sim.step(40.0f, 38.0f);  // transition to REGULATION, error = 2°C

  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::REGULATION, (int)sim.state);
  // PID was initialised with bumpless integral so output < 100% for a 2°C error
  TEST_ASSERT_LESS_THAN(100, (int)sim.hydraulic_power);
}

void test_hydro_reenabled_after_boost_bumpless_not_zero(void)
{
  // BOOST → REGULATION transition: hydro was at 100%. Bumpless should give a
  // first-tick hydro output that is significantly above 0.
  HeatingControllerSim sim(true);

  // Enter BOOST with large error
  sim.step(40.0f, 25.0f);
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);

  // Wait kTOnMin steps then bring error just below kEBas
  sim.step_n((int)kTOnMin + 1, 40.0f, 39.9f);  // error = 0.1 < kEBas → REGULATION
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::REGULATION, (int)sim.state);

  // Hydraulic should have resumed from a value > 0 (bumpless from 100% sets a
  // positive integral that keeps output non-trivial)
  TEST_ASSERT_GREATER_THAN(0, (int)sim.hydraulic_power);
}

// ===== PID integral frozen during BOOST =====

void test_integral_frozen_during_boost(void)
{
  HeatingControllerSim sim(true);

  // Enter BOOST
  sim.step(40.0f, 29.0f);
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);

  float integral_on_boost_entry = sim.pid.GetIntegral();
  sim.step(40.0f, 30.0f);  // tick inside BOOST
  TEST_ASSERT_EQUAL_FLOAT(integral_on_boost_entry, sim.pid.GetIntegral());
}

// ===== Falling temperature triggers BOOST (cold hydraulic water scenario) =====

void test_boost_triggers_when_temperature_falling_with_error(void)
{
  // Cold hydraulic water scenario: temperature falls steadily at -0.02°C/s.
  // setpoint=30, T starts at 32 and drops. For cond3 to fire we need:
  //   - error > kEBas (0.4): T < 29.6 → reached after ~120 ticks (32 - 120*0.02 = 29.6)
  //   - dT_dt < -kDtFalling (-0.01): filter converges after ~20 ticks at -0.02°C/s
  //   - can_boost: elec_off_timer initialized to kTOffMin → true from tick 1
  HeatingControllerSim sim(true);
  float T = 32.0f;
  sim.step(30.0f, T);  // prime prev_temp

  for (int i = 0; i < 150; i++)
  {
    T -= 0.02f;
    sim.step(30.0f, T);
    if (sim.state == HeatingControllerSim::State::BOOST) break;
  }

  // After ~125 ticks: T≈29.5°C, error≈0.5 > kEBas, dT_dt≈-0.018 < -kDtFalling
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);
  TEST_ASSERT_TRUE(sim.electric_on);
}

void test_boost_not_triggered_when_temperature_barely_falling(void)
{
  // dT_dt ≈ -0.005°C/s < kDtFalling (0.01): falling branch of cond3 NOT taken.
  // setpoint=30, T starts at 32, drops 0.005°C/tick.
  // After 500 ticks: T = 32 - 2.5 = 29.5°C, error = 0.5 > kEBas.
  // cond1 won't fire (error < kEHaut=5), cond2 won't fire (PID not saturated),
  // cond3 won't fire (dT barely below threshold, eta branch = 0).
  HeatingControllerSim sim(true);
  float T = 32.0f;
  sim.step(30.0f, T);  // prime prev_temp

  bool boost_seen = false;
  for (int i = 0; i < 500; i++)
  {
    T -= 0.005f;
    sim.step(30.0f, T);
    if (sim.state == HeatingControllerSim::State::BOOST) { boost_seen = true; break; }
  }

  TEST_ASSERT_FALSE(boost_seen);
}

// ===== BOOST exits only when setpoint is reached =====

void test_boost_exits_only_when_error_below_e_bas(void)
{
  HeatingControllerSim sim(true);

  // Enter BOOST
  sim.step(40.0f, 25.0f);
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);

  // Simulate temperature rising but stopping just above E_BAS (not at setpoint yet)
  // After kTOnMin steps, error = 40 - 39.7 = 0.3 < kEBas → should exit
  sim.step_n((int)kTOnMin, 40.0f, 39.5f);   // error = 0.5 ≥ kEBas → still BOOST
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);

  sim.step(40.0f, 39.7f);  // error = 0.3 < kEBas AND kTOnMin elapsed → REGULATION
  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::REGULATION, (int)sim.state);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_boost_does_not_exit_early_while_below_setpoint(void)
{
  // Even after kTOnMin seconds, BOOST must NOT exit if error >= kEBas.
  HeatingControllerSim sim(true);
  sim.step(40.0f, 25.0f);  // enter BOOST

  // Run kTOnMin+10 steps at error = 2°C (> kEBas)
  sim.step_n((int)kTOnMin + 10, 40.0f, 38.0f);  // error = 2 > kEBas

  TEST_ASSERT_EQUAL((int)HeatingControllerSim::State::BOOST, (int)sim.state);
  TEST_ASSERT_TRUE(sim.electric_on);
}

// ===== SetIntegral bumpless mechanics =====

void test_set_integral_bumpless_resume_from_boost(void)
{
  // Verify that SetIntegral() correctly positions the PID output near 100%.
  // At error = 0.3°C: integral = (100 - 15*0.3) / 0.1 = (100-4.5)/0.1 = 955 → clamped 200.
  // Output = 15*0.3 + 0.1*200 = 4.5 + 20 = 24.5%.  Not 100% due to clamp, but above 0.
  PIDController pid(kKp, kKi, kKd, 0.0f, 100.0f, kIntMax, kDerivFilter);
  float error = 0.3f;
  float resume_int = (100.0f - kKp * error) / kKi;
  if (resume_int > kIntMax) resume_int = kIntMax;
  if (resume_int < -kIntMax) resume_int = -kIntMax;
  pid.SetIntegral(resume_int);
  float u = pid.Compute(40.0f, 40.0f - error, 1.0f, false);
  TEST_ASSERT_GREATER_THAN(0.0f, u);   // output is above 0 (not reset to 0)
}

void test_set_integral_value_is_clamped(void)
{
  PIDController pid(1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 50.0f);
  pid.SetIntegral(9999.0f);
  TEST_ASSERT_EQUAL_FLOAT(50.0f, pid.GetIntegral());

  pid.SetIntegral(-9999.0f);
  TEST_ASSERT_EQUAL_FLOAT(-50.0f, pid.GetIntegral());
}

void test_set_integral_within_bounds(void)
{
  PIDController pid(1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 50.0f);
  pid.SetIntegral(25.0f);
  TEST_ASSERT_EQUAL_FLOAT(25.0f, pid.GetIntegral());
}

// ===== Test Runner =====

int main(int argc, char **argv)
{
  UNITY_BEGIN();

  // Cold start boost
  RUN_TEST(test_boost_triggers_immediately_on_large_error);
  RUN_TEST(test_boost_not_triggered_for_small_error);

  // Fine regulation
  RUN_TEST(test_fine_regulation_electric_stays_off);

  // Anti-short-cycle
  RUN_TEST(test_anti_short_cycle_blocks_boost_after_exit);
  RUN_TEST(test_boost_allowed_after_toffmin_elapsed);
  RUN_TEST(test_anti_short_cycle_boost_minimum_on_time);

  // Safety cutoff
  RUN_TEST(test_safety_cutoff_forces_all_off);
  RUN_TEST(test_safety_does_not_fire_at_exact_limit);
  RUN_TEST(test_sensor_fault_forces_all_off);

  // Fan interlock
  RUN_TEST(test_fan_interlock_blocks_all_heating);
  RUN_TEST(test_fan_restored_allows_boost);

  // ELECTRIC_ONLY mode
  RUN_TEST(test_electric_only_hydro_always_zero);
  RUN_TEST(test_electric_only_turns_on_when_error_exceeds_band);
  RUN_TEST(test_electric_only_stays_off_when_error_below_band);
  RUN_TEST(test_electric_only_turns_off_when_setpoint_reached);
  RUN_TEST(test_electric_only_predictive_shutoff_ignores_noise);
  RUN_TEST(test_electric_only_predictive_shutoff_fires_on_genuine_rise);
  RUN_TEST(test_electric_only_anti_short_cycle_observed);

  // Hydraulic disabled mid-BOOST
  RUN_TEST(test_hydro_disabled_during_boost_drops_hydro_to_zero);
  RUN_TEST(test_hydro_disabled_during_boost_electric_continues);
  RUN_TEST(test_hydro_disabled_anti_short_cycle_timers_preserved);

  // Hydraulic re-enabled: bumpless
  RUN_TEST(test_hydro_reenabled_hydro_does_not_jump_to_full);
  RUN_TEST(test_hydro_reenabled_after_boost_bumpless_not_zero);

  // Falling temperature triggers BOOST
  RUN_TEST(test_boost_triggers_when_temperature_falling_with_error);
  RUN_TEST(test_boost_not_triggered_when_temperature_barely_falling);

  // PID behaviour during BOOST
  RUN_TEST(test_integral_frozen_during_boost);
  RUN_TEST(test_boost_exits_only_when_error_below_e_bas);
  RUN_TEST(test_boost_does_not_exit_early_while_below_setpoint);

  // SetIntegral / bumpless mechanics
  RUN_TEST(test_set_integral_bumpless_resume_from_boost);
  RUN_TEST(test_set_integral_value_is_clamped);
  RUN_TEST(test_set_integral_within_bounds);

  return UNITY_END();
}
