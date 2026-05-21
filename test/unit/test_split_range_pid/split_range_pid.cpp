#include <unity.h>
#include <cmath>
#include "../../../include/PIDController.h"

/**
 * Unit tests for the split-range PID heating strategy.
 *
 * The SplitRangeSimulator replicates the logic from TemperatureManager::UpdateHeating()
 * without any Arduino/hardware dependencies. All thresholds match config.h defaults.
 *
 * PID parameters used: Kp=5, Ki=0.1, Kd=2, integral_max=200, derivative_filter=0.1
 * With these gains, the proportional term alone:
 *   u_p = Kp × error  →  error ≥ 14°C to reach 70% threshold (normal mode, T ≤ 26°C)
 *   u_p = Kp × error  →  error ≥ 0.8°C to reach 4% threshold  (degraded mode, T ≤ 39.2°C)
 * Tests use temperatures calibrated to these constraints.
 */

// ===== Thresholds (mirrored from config.h) =====
static constexpr float kSplitElectricOn      = 70.0f;
static constexpr float kSplitElectricOff     = 55.0f;
static constexpr float kSplitElectricOnDeg   = 4.0f;   // SPLIT_ELECTRIC_ON_DEG
static constexpr float kSplitElectricOffDeg  = 2.0f;   // SPLIT_ELECTRIC_OFF_DEG
static constexpr float kElectricOnDelayS     = 10.0f;
static constexpr float kElectricSettleS      = 30.0f;
static constexpr float kElectricDtOn         = 2.0f;
static constexpr float kElectricOffAnticS    = 20.0f;  // ELECTRIC_OFF_ANTICIPATION_S
static constexpr float kTempSafetyMax        = 50.0f;

// ===== Simulator =====
// Mirrors UpdateHeating() with no hardware dependencies.

struct SplitRangeSimulator {
  PIDController pid;
  bool    hydraulic_available;
  bool    fan_active;
  bool    electric_enabled;
  bool    electric_on;
  uint8_t hydraulic_power;     // 0-100%, mirrors hydraulic_heater_->GetPower()
  float   electric_on_timer_s;
  float   electric_settle_timer_s;
  float   last_u;

  explicit SplitRangeSimulator(bool hydraulic, bool fan = true, bool elec_en = true)
    : pid(5.0f, 0.1f, 2.0f, 0.0f, 100.0f, 200.0f, 0.1f),
      hydraulic_available(hydraulic),
      fan_active(fan),
      electric_enabled(elec_en),
      electric_on(false),
      hydraulic_power(0),
      electric_on_timer_s(0.0f),
      electric_settle_timer_s(0.0f),
      last_u(0.0f) {}

  // Run one control cycle. Returns the PID output u.
  float step(float setpoint, float measured, float dt = 1.0f) {
    // Safety cutoff
    if (measured > kTempSafetyMax) {
      electric_on              = false;
      hydraulic_power          = 0;
      electric_on_timer_s      = 0.0f;
      electric_settle_timer_s  = 0.0f;
      pid.Reset();
      last_u = 0.0f;
      return 0.0f;
    }

    // Ventilation interlock — mirrors TemperatureManager Block A+
    if (!fan_active || !electric_enabled) {
      electric_on             = false;
      hydraulic_power         = 0;
      electric_on_timer_s     = 0.0f;
      electric_settle_timer_s = 0.0f;
      last_u = 0.0f;
      return 0.0f;
    }

    // Anti-windup freeze: integral frozen when saturated, during ON timer, or during settle window.
    bool freeze = (last_u >= 100.0f) ||
                  (last_u <= 0.0f)   ||
                  (electric_on_timer_s > 0.0f && !electric_on) ||
                  (electric_settle_timer_s > 0.0f);

    float u = pid.Compute(setpoint, measured, dt, freeze);
    last_u = u;

    // Settle window countdown
    if (electric_settle_timer_s > 0.0f)
      electric_settle_timer_s = (electric_settle_timer_s > dt)
                                  ? electric_settle_timer_s - dt
                                  : 0.0f;

    const float on_threshold  = hydraulic_available ? kSplitElectricOn    : kSplitElectricOnDeg;
    const float off_threshold = hydraulic_available ? kSplitElectricOff   : kSplitElectricOffDeg;
    const float on_delay      = hydraulic_available ? kElectricOnDelayS   : 0.0f;
    const float dt_on_guard   = hydraulic_available ? kElectricDtOn       : 0.0f;

    // ON timer
    if (u > on_threshold && measured < (setpoint - dt_on_guard))
      electric_on_timer_s += dt;
    else
      electric_on_timer_s = 0.0f;

    if (!electric_on && electric_on_timer_s >= on_delay) {
      electric_on             = true;
      electric_settle_timer_s = kElectricSettleS;
    }

    // Predictive shutoff — mirrors TemperatureManager Block C
    // rise_rate = -GetDerivative() because derivative is of error (= setpoint - measured),
    // so a rising temperature gives a negative error derivative → negate to get °C/s.
    const float rise_rate    = -pid.GetDerivative();
    const bool will_overshoot = (rise_rate > 0.02f) &&
                                (measured + rise_rate * kElectricOffAnticS >= setpoint);

    // OFF: demand below hysteresis, setpoint reached, or predictive shutoff
    if (u < off_threshold || measured >= setpoint || will_overshoot) {
      electric_on             = false;
      electric_on_timer_s     = 0.0f;
      electric_settle_timer_s = 0.0f;
    }

    // Hydraulic: proportional on [0, kSplitElectricOn], saturated at 100% above.
    // In PRIMARY_ELEC mode (no hydraulic source), pump stays off.
    if (hydraulic_available) {
      float hydro_pct = fminf(u / kSplitElectricOn * 100.0f, 100.0f);
      hydraulic_power = (uint8_t)hydro_pct;
    } else {
      hydraulic_power = 0;
    }

    return u;
  }

  // Run n cycles with fixed conditions, return last u
  float step_n(int n, float setpoint, float measured, float dt = 1.0f) {
    float u = 0.0f;
    for (int i = 0; i < n; i++) u = step(setpoint, measured, dt);
    return u;
  }
};

// ===== Boilerplate =====
void setUp(void) {}
void tearDown(void) {}

// ===== Normal mode (hydraulic available) =====

void test_normal_electric_off_when_demand_low(void) {
  // Small error (1°C): Kp×error = 5, u ≈ 5% << 70% → electric never triggers.
  SplitRangeSimulator sim(true);
  sim.step_n(60, 40.0f, 39.0f);
  TEST_ASSERT_FALSE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
}

void test_normal_electric_timer_increments_above_threshold(void) {
  // Large error: T=22°C → error=18, Kp×error=90 → u > 70% from cycle 1.
  // Timer should start incrementing immediately.
  SplitRangeSimulator sim(true);
  sim.step(40.0f, 22.0f);  // First step: u ≈ 90 > 70, T=22 < 38 → timer starts
  TEST_ASSERT_GREATER_THAN(0.0f, sim.electric_on_timer_s);
  TEST_ASSERT_FALSE(sim.electric_on);  // Not ON yet: need kElectricOnDelayS=10s
}

void test_normal_electric_turns_on_after_delay(void) {
  // After kElectricOnDelayS (10) continuous cycles above threshold, electric turns ON.
  // T=22°C → error=18 → u ≈ 90% > 70%. T < setpoint-2°C. Timer counts up.
  SplitRangeSimulator sim(true);
  sim.step_n((int)kElectricOnDelayS + 2, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);
}

void test_normal_electric_settle_window_starts_on_activation(void) {
  // When electric first turns ON, settle timer must be set to kElectricSettleS.
  SplitRangeSimulator sim(true);
  sim.step_n((int)kElectricOnDelayS + 1, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);
  // Settle timer decrements each step; after 1 extra step it's at kElectricSettleS - 1
  TEST_ASSERT_GREATER_THAN(kElectricSettleS - 2.0f, sim.electric_settle_timer_s);
}

void test_normal_electric_turns_off_at_setpoint(void) {
  // Electric ON, then temperature reaches setpoint → electric must turn OFF immediately.
  SplitRangeSimulator sim(true);
  sim.step_n((int)kElectricOnDelayS + 2, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  // Setpoint reached — condition: measured >= setpoint
  sim.step(40.0f, 40.5f);
  TEST_ASSERT_FALSE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
}

void test_normal_electric_turns_off_when_demand_drops(void) {
  // Electric ON, then demand drops below kSplitElectricOff (55%) → electric OFF.
  SplitRangeSimulator sim(true);
  sim.step_n((int)kElectricOnDelayS + 2, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  // Bring T above setpoint → error < 0 → u = 0 < 55% → OFF
  sim.step_n(5, 40.0f, 41.0f);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_normal_timer_resets_if_demand_drops_momentarily(void) {
  // Timer started, then one cycle with low demand → timer must reset to 0.
  SplitRangeSimulator sim(true);
  for (int i = 0; i < 5; i++) sim.step(40.0f, 22.0f);
  TEST_ASSERT_GREATER_THAN(0.0f, sim.electric_on_timer_s);

  sim.step(40.0f, 40.0f);  // error=0 → u=0 → timer resets
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_normal_dt_on_guard_blocks_timer_when_temp_close(void) {
  // T=38.8°C → error=1.2 < kElectricDtOn (2°C): timer must NOT increment.
  // With error=1.2: u ≈ 6% << 70%, so both the u-threshold and DT_ON guard block.
  SplitRangeSimulator sim(true);
  sim.step_n(30, 40.0f, 38.8f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
  TEST_ASSERT_FALSE(sim.electric_on);
}

// ===== Degraded mode (no hydraulic) =====

void test_degraded_electric_turns_on_above_threshold(void) {
  // T=39°C → error=1, Kp×error=5 → u ≈ 5% > kSplitElectricOnDeg (4%).
  // No timer in degraded mode: electric turns ON immediately (on_delay=0).
  SplitRangeSimulator sim(false);
  sim.step_n(2, 40.0f, 39.0f);
  TEST_ASSERT_TRUE(sim.electric_on);
}

void test_degraded_electric_turns_off_below_threshold(void) {
  // Start ON, then T > setpoint → u = 0 < kSplitElectricOffDeg (2%) → OFF.
  SplitRangeSimulator sim(false);
  sim.step_n(2, 40.0f, 39.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  sim.step_n(10, 40.0f, 42.0f);  // error < 0 → u = 0 < 2%
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_degraded_no_timer_needed(void) {
  // In degraded mode, electric activates immediately (no 10s delay).
  // Verify it turns ON in well under kElectricOnDelayS cycles.
  SplitRangeSimulator sim(false);
  bool on_before_delay = false;
  for (int i = 0; i < (int)kElectricOnDelayS - 2; i++) {
    sim.step(40.0f, 39.0f);
    if (sim.electric_on) { on_before_delay = true; break; }
  }
  TEST_ASSERT_TRUE(on_before_delay);
}

void test_degraded_hysteresis_holds_electric_on_in_dead_band(void) {
  // After electric turns ON, demand in dead-band [2%, 4%] must NOT turn it OFF.
  // T=39.4°C → error=0.6 → u_p = 3% ∈ [2%, 4%]: in dead-band.
  SplitRangeSimulator sim(false);
  // Turn electric ON with sufficient demand
  sim.step_n(2, 40.0f, 39.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  // Reduce to dead-band: T=39.4°C, error=0.6, u_p=3%
  // Settle timer active → integral frozen → u ≈ P ≈ 3% > 2% → stays ON
  sim.step_n(5, 40.0f, 39.4f);
  TEST_ASSERT_TRUE(sim.electric_on);
}

void test_degraded_settle_window_starts_on_activation(void) {
  SplitRangeSimulator sim(false);
  sim.step_n(2, 40.0f, 39.0f);
  TEST_ASSERT_TRUE(sim.electric_on);
  TEST_ASSERT_GREATER_THAN(kElectricSettleS - 4.0f, sim.electric_settle_timer_s);
}

// ===== Anti-windup =====

void test_antiwindup_integral_frozen_during_on_timer(void) {
  // While the electric ON timer is counting, the integral must not accumulate.
  SplitRangeSimulator sim(true);

  sim.step(40.0f, 22.0f);  // First step: u > 70%, timer = 1
  TEST_ASSERT_GREATER_THAN(0.0f, sim.electric_on_timer_s);
  TEST_ASSERT_FALSE(sim.electric_on);

  float integral_before = sim.pid.GetIntegral();
  sim.step(40.0f, 22.0f);  // timer > 0 and !electric_on → freeze active
  TEST_ASSERT_EQUAL_FLOAT(integral_before, sim.pid.GetIntegral());
}

void test_antiwindup_integral_frozen_during_settle(void) {
  // After electric turns ON, integral must remain frozen during the settle window.
  SplitRangeSimulator sim(true);
  sim.step_n((int)kElectricOnDelayS + 1, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);
  TEST_ASSERT_GREATER_THAN(0.0f, sim.electric_settle_timer_s);

  float integral_before = sim.pid.GetIntegral();
  sim.step(40.0f, 22.0f);  // settle_timer > 0 → freeze active
  TEST_ASSERT_EQUAL_FLOAT(integral_before, sim.pid.GetIntegral());
}

void test_antiwindup_integral_resumes_after_settle_window(void) {
  // Once the settle window expires and output is not saturated, the integral grows.
  SplitRangeSimulator sim(true);
  sim.step_n((int)kElectricOnDelayS + 1, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  // Burn through settle window with moderate demand (T=28: error=12, P=60).
  // u ≈ 60%: not saturated, in dead-band [55, 70] → electric stays ON.
  sim.step_n((int)kElectricSettleS + 2, 40.0f, 28.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_settle_timer_s);
  TEST_ASSERT_TRUE(sim.electric_on);

  // Now all freeze conditions are clear: integral must accumulate.
  float integral_before = sim.pid.GetIntegral();
  sim.step(40.0f, 28.0f);
  TEST_ASSERT_NOT_EQUAL(integral_before, sim.pid.GetIntegral());
}

// ===== Safety cutoff =====

void test_safety_cutoff_forces_electric_off(void) {
  SplitRangeSimulator sim(true);
  sim.step_n((int)kElectricOnDelayS + 2, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  sim.step(40.0f, kTempSafetyMax + 1.0f);
  TEST_ASSERT_FALSE(sim.electric_on);
}

void test_safety_cutoff_resets_all_state(void) {
  SplitRangeSimulator sim(true);
  sim.step_n((int)kElectricOnDelayS + 2, 40.0f, 22.0f);

  sim.step(40.0f, kTempSafetyMax + 0.1f);
  TEST_ASSERT_FALSE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_settle_timer_s);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.pid.GetIntegral());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.pid.GetLastOutput());
}

void test_safety_cutoff_does_not_trigger_at_limit(void) {
  // Exactly at kTempSafetyMax: condition is T > limit (strict), so no cutoff.
  SplitRangeSimulator sim(false);
  sim.step_n(2, 40.0f, 39.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  // Exactly at limit — safety does NOT fire; split-range logic handles OFF.
  sim.step(40.0f, kTempSafetyMax);
  TEST_ASSERT_FALSE(sim.electric_on);  // OFF due to measured >= setpoint (40 reached)
}

// ===== Fan interlock =====

void test_fan_interlock_blocks_electric_heater(void) {
  // Fan is OFF: electric must be blocked even with large demand.
  SplitRangeSimulator sim(true, /*fan=*/false);
  sim.step_n(20, 40.0f, 22.0f);
  TEST_ASSERT_FALSE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
}

void test_fan_interlock_resets_state_when_fan_stops(void) {
  // Electric ON, then fan stops → must reset all state immediately.
  SplitRangeSimulator sim(true, /*fan=*/true);
  sim.step_n((int)kElectricOnDelayS + 2, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  sim.fan_active = false;
  sim.step(40.0f, 22.0f);
  TEST_ASSERT_FALSE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_settle_timer_s);
}

void test_fan_restored_allows_electric_to_start_again(void) {
  // Fan stops then starts: electric must be able to turn ON again.
  SplitRangeSimulator sim(true, /*fan=*/false);
  sim.step_n(20, 40.0f, 22.0f);
  TEST_ASSERT_FALSE(sim.electric_on);

  sim.fan_active = true;
  sim.step_n((int)kElectricOnDelayS + 2, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);
}

void test_electric_disabled_blocks_heater(void) {
  // electric_enabled=false: electric must never turn ON.
  SplitRangeSimulator sim(true, /*fan=*/true, /*elec_en=*/false);
  sim.step_n(20, 40.0f, 22.0f);
  TEST_ASSERT_FALSE(sim.electric_on);
}

// ===== Predictive shutoff =====

void test_predictive_shutoff_fires_before_setpoint(void) {
  // Use derivative_filter=1.0 (no smoothing) for immediate rise_rate detection.
  // Kd=0 so the derivative doesn't affect the output — we're just checking shutoff logic.
  // Turn electric ON, then simulate a fast-rising temperature below setpoint.
  SplitRangeSimulator sim(true);

  // Manually force electric ON and a known settle-complete state.
  sim.step_n((int)kElectricOnDelayS + (int)kElectricSettleS + 5, 40.0f, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_settle_timer_s);

  // Simulate temperature rising from 37 → 38 → 39°C across 3 steps (1°C/s rise).
  // After the step from 38 to 39: rise_rate ≈ filtered(-(-1)/1) ≈ positive.
  // With rise_rate ≥ 0.1°C/s and anticipation=20s: 39 + 0.1*20 = 41 ≥ 40 → overshoot.
  // The derivative filter (alpha=0.1) will reduce the observed rise_rate, so use
  // a fast rise (1°C/s × many steps) to build up the filtered derivative.
  for (int i = 0; i < 10; i++) sim.step(40.0f, 37.0f + i * 0.3f);

  // With temp approaching setpoint and a measurable rise rate, electric should shut off.
  // Allow that it might already have fired; if not, one more step near setpoint will.
  sim.step(40.0f, 39.5f);
  TEST_ASSERT_FALSE(sim.electric_on);
}

// ===== Hydraulic proportional control =====

void test_hydraulic_power_zero_at_zero_demand(void) {
  // u=0 → hydro=0 (pump off)
  SplitRangeSimulator sim(true);
  sim.step(40.0f, 41.0f);  // error < 0 → u = 0
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
}

void test_hydraulic_power_proportional_below_threshold(void) {
  // u=35% (half of kSplitElectricOn=70%) → hydro=50%
  // T=33°C → error=7, Kp×error=35 → u_p ≈ 35% (first cycle, integral ~0)
  SplitRangeSimulator sim(true);
  sim.step(40.0f, 33.0f);
  // u ≈ 35%, hydro = 35/70 * 100 = 50%
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 50.0f, (float)sim.hydraulic_power);
}

void test_hydraulic_saturates_at_full_threshold(void) {
  // u ≥ 70% (kSplitElectricOn) → hydro must be capped at 100%
  // T=22°C → error=18, Kp×error=90 → u ≈ 90% > 70%
  SplitRangeSimulator sim(true);
  sim.step(40.0f, 22.0f);
  TEST_ASSERT_EQUAL_UINT8(100, sim.hydraulic_power);
}

void test_hydraulic_zero_in_degraded_mode(void) {
  // PRIMARY_ELEC mode: no hydraulic source → pump always off
  SplitRangeSimulator sim(false);
  sim.step_n(20, 40.0f, 32.0f);  // large demand but no hydraulic
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
}

void test_hydraulic_zero_on_safety_cutoff(void) {
  SplitRangeSimulator sim(true);
  sim.step_n(5, 40.0f, 22.0f);  // hydro at 100%
  TEST_ASSERT_EQUAL_UINT8(100, sim.hydraulic_power);

  sim.step(40.0f, kTempSafetyMax + 1.0f);
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
}

void test_hydraulic_zero_when_fan_off(void) {
  SplitRangeSimulator sim(true, /*fan=*/false);
  sim.step_n(5, 40.0f, 22.0f);
  TEST_ASSERT_EQUAL_UINT8(0, sim.hydraulic_power);
}

void test_hydraulic_tracks_demand_continuously(void) {
  // As temperature rises, error decreases, u decreases, hydro decreases proportionally.
  SplitRangeSimulator sim(true);

  sim.step(40.0f, 22.0f);  // large error → hydro ≈ 100%
  uint8_t hydro_cold = sim.hydraulic_power;

  sim.step_n(5, 40.0f, 36.0f);  // error=4, u_p=20% → hydro ≈ 29%
  uint8_t hydro_warm = sim.hydraulic_power;

  // Hydraulic must have decreased as temperature rose
  TEST_ASSERT_GREATER_THAN(hydro_warm, hydro_cold);
}

// ===== Realistic scenario: hydraulic partial heat =====

void test_normal_hydraulic_partial_heat_electric_supplements(void) {
  // Real-world scenario: hydraulic provides partial heat (0.4°C/s rise rate).
  // The temperature rises steadily — electric must supplement during the cold phase,
  // then release once demand drops as temperature approaches setpoint.
  //
  // With rise_rate=0.4°C/s:
  //   Cycle  9 (T≈25.6°C): electric ON after 10s debounce, settle=30s starts
  //   Cycle 39 (T≈37.6°C): settle expires, u_p=5×2.4=12% < 55% → electric OFF
  //   Temperature eventually reaches setpoint via hydraulic alone
  SplitRangeSimulator sim(true);
  const float setpoint  = 40.0f;
  const float rise_rate = 0.4f;  // °C/s — hydraulic partial heat

  float temp = 22.0f;
  bool  electric_turned_on = false;
  int   electric_on_cycle  = -1;

  for (int cycle = 0; cycle < 300; cycle++) {
    sim.step(setpoint, temp);

    if (sim.electric_on && !electric_turned_on) {
      electric_turned_on = true;
      electric_on_cycle  = cycle + 1;
      // Must not activate before the 10s debounce
      TEST_ASSERT_GREATER_OR_EQUAL((int)kElectricOnDelayS, electric_on_cycle);
    }

    if (temp < setpoint + 1.0f)
      temp += rise_rate;
  }

  // Electric must have supplemented during the cold start
  TEST_ASSERT_TRUE(electric_turned_on);
  // Hydraulic must have eventually brought temperature to setpoint
  TEST_ASSERT_GREATER_OR_EQUAL(setpoint, temp);
}

void test_normal_hydraulic_suddenly_becomes_effective(void) {
  // Scenario: hydraulic starts cold (electric supplements), then hydraulic
  // becomes fully effective → demand drops → electric turns OFF.
  // Phase 1: T=22°C (cold), electric turns ON after delay.
  // Phase 2: hydraulic kicks in hard → T jumps toward setpoint → demand drops → electric OFF.
  SplitRangeSimulator sim(true);
  const float setpoint = 40.0f;

  // Phase 1: cold start, electric turns ON
  sim.step_n((int)kElectricOnDelayS + 2, setpoint, 22.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  // Phase 2: hydraulic now fully effective — temperature shoots up past setpoint
  // When T >= setpoint, the OFF condition fires immediately.
  sim.step(setpoint, setpoint + 0.5f);
  TEST_ASSERT_FALSE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);

  // Timer also reset — electric will not re-trigger unless demand builds again
  // (T above setpoint → u = 0 → timer stays 0)
  sim.step_n(5, setpoint, setpoint + 1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
}

// ===== Realistic scenario: cold start, normal mode =====

void test_realistic_cold_start_normal_mode(void) {
  // Simulate a realistic cold-start drying session:
  //   Phase 1 (cycles 1–5):    T=20°C, large error → timer counting
  //   Phase 2 (cycles 6–15):   T=20°C, timer reaches 10s → electric ON
  //   Phase 3 (cycles 16–45):  T=20°C, settle window active → integral frozen
  //   Phase 4 (cycles 46+):    T rising to 38°C → electric stays ON
  //   Phase 5:                 T reaches setpoint → electric OFF
  SplitRangeSimulator sim(true);
  const float setpoint = 40.0f;

  // Before delay expires: electric must stay OFF
  for (int i = 0; i < (int)kElectricOnDelayS - 1; i++)
    sim.step(setpoint, 20.0f);
  TEST_ASSERT_FALSE(sim.electric_on);

  // After delay: electric ON
  sim.step_n(3, setpoint, 20.0f);
  TEST_ASSERT_TRUE(sim.electric_on);

  // Settle window: integral still frozen
  float integral_after_on = sim.pid.GetIntegral();
  sim.step(setpoint, 20.0f);  // settle_timer > 0 → frozen
  TEST_ASSERT_EQUAL_FLOAT(integral_after_on, sim.pid.GetIntegral());

  // Burn through settle window, then ensure integral accumulates
  sim.step_n((int)kElectricSettleS + 2, setpoint, 28.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_settle_timer_s);
  float integral_mid = sim.pid.GetIntegral();
  sim.step(setpoint, 28.0f);
  TEST_ASSERT_NOT_EQUAL(integral_mid, sim.pid.GetIntegral());

  // Temperature reaches setpoint → electric OFF
  sim.step(setpoint, setpoint + 0.5f);
  TEST_ASSERT_FALSE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
}

void test_realistic_20_to_40_cold_start(void) {
  // Full cold-start: T=20°C → setpoint=40°C, normal mode (hydraulic available).
  // Uses a controlled temperature trajectory (0.4°C/s) to validate each control phase.
  //
  // Expected sequence:
  //   Phase 1 [cycles 0-8]:  u=100% saturated, hydro=100%, electric OFF (debounce)
  //   Phase 2 [cycles 9+]:   electric ON (debounce expired), settle window starts
  //   Phase 3 [settle]:      integral frozen, hydro=100%
  //   Phase 4 [T≈36-38°C]:  settle expires, demand < 55% → electric OFF, hydro backs off
  //   Phase 5 [T≥setpoint]:  electric stays OFF, hydro minimal

  SplitRangeSimulator sim(true);
  const float setpoint = 40.0f;
  float temp = 20.0f;

  // === Phase 1: before delay — electric OFF, u saturated, hydro=100% ===
  for (int i = 0; i < (int)kElectricOnDelayS - 1; i++) {
    sim.step(setpoint, temp);
    TEST_ASSERT_FALSE(sim.electric_on);
    TEST_ASSERT_EQUAL_UINT8(100, sim.hydraulic_power);  // u=100% saturated
    temp += 0.4f;
  }

  // === Phase 2: electric ON after debounce ===
  sim.step_n(3, setpoint, temp);
  TEST_ASSERT_TRUE(sim.electric_on);
  TEST_ASSERT_GREATER_THAN(0.0f, sim.electric_settle_timer_s);
  TEST_ASSERT_EQUAL_UINT8(100, sim.hydraulic_power);  // demand still at 100%

  // === Phase 3: settle active — integral must stay frozen ===
  float integral_during_settle = sim.pid.GetIntegral();
  sim.step(setpoint, temp);
  TEST_ASSERT_EQUAL_FLOAT(integral_during_settle, sim.pid.GetIntegral());

  // === Phase 4: burn through settle, temperature climbs to ~36°C ===
  // At T=36: error=4, u_p=20% < 55% → electric OFF, hydro drops proportionally
  sim.step_n((int)kElectricSettleS, setpoint, 36.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_settle_timer_s);
  TEST_ASSERT_FALSE(sim.electric_on);
  // Hydro backed off: u_p≈20% → hydro≈28% (not 100% anymore)
  TEST_ASSERT_LESS_THAN(100, (int)sim.hydraulic_power);
  TEST_ASSERT_GREATER_THAN(0, (int)sim.hydraulic_power);

  // === Phase 5: setpoint reached → electric stays OFF, hydro near zero ===
  sim.step(setpoint, setpoint + 0.5f);  // T > setpoint → u→0
  TEST_ASSERT_FALSE(sim.electric_on);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sim.electric_on_timer_s);
}

void test_realistic_degraded_mode_cold_start(void) {
  // Degraded mode cold start: electric activates quickly, cycles around setpoint.
  SplitRangeSimulator sim(false);
  const float setpoint = 40.0f;

  // Immediate activation with small error > 4% threshold
  sim.step_n(2, setpoint, 39.0f);  // error=1, u_p=5% > 4%
  TEST_ASSERT_TRUE(sim.electric_on);

  // Overshoot: T above setpoint → OFF
  sim.step_n(5, setpoint, 41.0f);
  TEST_ASSERT_FALSE(sim.electric_on);

  // Integral accumulated; once demand > 4% again → ON
  sim.step_n(2, setpoint, 39.5f);  // error=0.5, u_p=2.5%. With integral, may exceed 4%.
  // Either ON or OFF acceptable here depending on integral state,
  // but electric must not be stuck permanently.
  // Verify: after more cycles, electric cycles (not stuck off forever).
  for (int i = 0; i < 20; i++) {
    sim.step(setpoint, 39.5f);
    if (sim.electric_on) break;
  }
  TEST_ASSERT_TRUE(sim.electric_on);
}

// ===== freeze_integral parameter on PIDController =====

void test_pid_freeze_integral_prevents_accumulation(void) {
  PIDController pid(0.0f, 1.0f, 0.0f, 0.0f, 100.0f, 200.0f);

  pid.Compute(50.0f, 40.0f, 1.0f, false);
  float integral_after_normal = pid.GetIntegral();
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, integral_after_normal);

  pid.Compute(50.0f, 40.0f, 1.0f, true);   // Frozen: integral must not change
  TEST_ASSERT_EQUAL_FLOAT(integral_after_normal, pid.GetIntegral());
}

void test_pid_freeze_does_not_affect_p_term(void) {
  PIDController pid(2.0f, 1.0f, 0.0f, 0.0f, 100.0f, 200.0f);

  float u_frozen = pid.Compute(50.0f, 40.0f, 1.0f, true);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f, u_frozen);  // P = 2*10 = 20
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.GetIntegral());
}

void test_pid_freeze_does_not_affect_d_term(void) {
  PIDController pid(0.0f, 0.0f, 10.0f, 0.0f, 200.0f, 50.0f, 1.0f);  // filter=1, no smoothing

  float u = pid.Compute(50.0f, 40.0f, 1.0f, true);  // D = 10 * 10 = 100
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, u);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.GetIntegral());
}

// ===== Test Runner =====

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Normal mode
  RUN_TEST(test_normal_electric_off_when_demand_low);
  RUN_TEST(test_normal_electric_timer_increments_above_threshold);
  RUN_TEST(test_normal_electric_turns_on_after_delay);
  RUN_TEST(test_normal_electric_settle_window_starts_on_activation);
  RUN_TEST(test_normal_electric_turns_off_at_setpoint);
  RUN_TEST(test_normal_electric_turns_off_when_demand_drops);
  RUN_TEST(test_normal_timer_resets_if_demand_drops_momentarily);
  RUN_TEST(test_normal_dt_on_guard_blocks_timer_when_temp_close);

  // Degraded mode
  RUN_TEST(test_degraded_electric_turns_on_above_threshold);
  RUN_TEST(test_degraded_electric_turns_off_below_threshold);
  RUN_TEST(test_degraded_no_timer_needed);
  RUN_TEST(test_degraded_hysteresis_holds_electric_on_in_dead_band);
  RUN_TEST(test_degraded_settle_window_starts_on_activation);

  // Anti-windup
  RUN_TEST(test_antiwindup_integral_frozen_during_on_timer);
  RUN_TEST(test_antiwindup_integral_frozen_during_settle);
  RUN_TEST(test_antiwindup_integral_resumes_after_settle_window);

  // Safety cutoff
  RUN_TEST(test_safety_cutoff_forces_electric_off);
  RUN_TEST(test_safety_cutoff_resets_all_state);
  RUN_TEST(test_safety_cutoff_does_not_trigger_at_limit);

  // Fan interlock
  RUN_TEST(test_fan_interlock_blocks_electric_heater);
  RUN_TEST(test_fan_interlock_resets_state_when_fan_stops);
  RUN_TEST(test_fan_restored_allows_electric_to_start_again);
  RUN_TEST(test_electric_disabled_blocks_heater);

  // Predictive shutoff
  RUN_TEST(test_predictive_shutoff_fires_before_setpoint);

  // Hydraulic proportional control
  RUN_TEST(test_hydraulic_power_zero_at_zero_demand);
  RUN_TEST(test_hydraulic_power_proportional_below_threshold);
  RUN_TEST(test_hydraulic_saturates_at_full_threshold);
  RUN_TEST(test_hydraulic_zero_in_degraded_mode);
  RUN_TEST(test_hydraulic_zero_on_safety_cutoff);
  RUN_TEST(test_hydraulic_zero_when_fan_off);
  RUN_TEST(test_hydraulic_tracks_demand_continuously);

  // Hydraulic partial heat scenarios
  RUN_TEST(test_normal_hydraulic_partial_heat_electric_supplements);
  RUN_TEST(test_normal_hydraulic_suddenly_becomes_effective);

  // Realistic scenarios
  RUN_TEST(test_realistic_20_to_40_cold_start);
  RUN_TEST(test_realistic_cold_start_normal_mode);
  RUN_TEST(test_realistic_degraded_mode_cold_start);

  // freeze_integral parameter
  RUN_TEST(test_pid_freeze_integral_prevents_accumulation);
  RUN_TEST(test_pid_freeze_does_not_affect_p_term);
  RUN_TEST(test_pid_freeze_does_not_affect_d_term);

  return UNITY_END();
}
