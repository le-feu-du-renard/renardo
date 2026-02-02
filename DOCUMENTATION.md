# Dryer Control System Documentation

## Table of Contents
- [PID Temperature Control](#pid-temperature-control)
  - [Overview](#overview)
  - [Understanding PID Control](#understanding-pid-control)
  - [Dual Heater System](#dual-heater-system)
  - [PID Parameters Reference](#pid-parameters-reference)
  - [Tuning Guide](#tuning-guide)
  - [Water Temperature Constraint](#water-temperature-constraint)
  - [Troubleshooting](#troubleshooting)

---

## PID Temperature Control

### Overview

The dryer uses **PID (Proportional-Integral-Derivative) control** to maintain precise temperature regulation. This replaces the previous hysteresis-based control that caused large temperature oscillations.

The system implements **two independent PID controllers**:
- **Hydraulic Heater PID**: Controls water circulation heater (0-100% power)
- **Electric Heater PID**: Controls backup electric heater (ON/OFF)

Both controllers work together to maintain the target temperature with minimal oscillation.

---

### Understanding PID Control

A PID controller calculates the control output based on three terms:

#### P - Proportional Term
```
P = Kp × error
```
- **What it does**: Provides immediate response proportional to the current error
- **Error** = Target Temperature - Current Temperature
- **Effect**: Larger error → stronger response

**Example**: If target is 40°C and current is 35°C:
- Error = 5°C
- With Kp = 5.0: P term = 25 (strong heating)
- As temperature approaches 40°C, P term gradually decreases

#### I - Integral Term
```
I = Ki × (accumulated error over time)
```
- **What it does**: Eliminates steady-state error by accumulating past errors
- **Effect**: If temperature consistently stays below target, integral builds up to increase heating

**Example**: Temperature stays at 39.5°C when target is 40°C:
- Small constant error of 0.5°C
- Integral accumulates: 0.5 + 0.5 + 0.5... = grows over time
- Eventually provides enough correction to reach exactly 40°C

#### D - Derivative Term
```
D = Kd × (rate of change of error)
```
- **What it does**: Anticipates future error based on how fast temperature is changing
- **Effect**: Dampens oscillations and prevents overshoot

**Example**: Temperature rising rapidly from 38°C to 39°C:
- Error decreasing quickly (good trend)
- Derivative term reduces heating power preemptively
- Prevents overshoot past target

#### Combined Output
```
PID Output = P + I + D
```
The output is then clamped to valid range (0-100 for hydraulic, 0-1 for electric).

---

### Dual Heater System

#### Hydraulic Heater (Primary)
- **Type**: Water circulation heater
- **Control**: Proportional (0-100% power)
- **Characteristics**: High thermal inertia, slow but efficient
- **PID Output**: Direct mapping to circulator power

```cpp
float hydraulic_output = hydraulic_pid.Compute(target, current, dt);
hydraulic_heater->SetPower((uint8_t)hydraulic_output);  // 0-100
```

#### Electric Heater (Backup)
- **Type**: Resistive heating element
- **Control**: Binary (ON/OFF)
- **Characteristics**: Undersized, provides supplemental heat
- **PID Output**: Converted to binary using 0.5 threshold

```cpp
float electric_output = electric_pid.Compute(target, current, dt);
electric_heater->SetPower(electric_output > 0.5 ? 1.0 : 0.0);
```

**Why separate PIDs?**
- Different thermal characteristics require different tuning
- Hydraulic: needs strong damping (high Kd) due to inertia
- Electric: needs strong response (high Kp) for quick on/off switching

---

### PID Parameters Reference

#### Hydraulic Heater Parameters

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `hydraulic_kp` | 5.0 | 0.1-20 | Proportional gain - immediate response strength |
| `hydraulic_ki` | 0.1 | 0.0-2 | Integral gain - steady-state error correction |
| `hydraulic_kd` | 2.0 | 0.0-5 | Derivative gain - oscillation damping |

**Typical behavior with defaults**:
- Moderate immediate response to temperature error
- Slow integral accumulation (prevents windup)
- Strong damping (anticipates slow thermal response)

#### Electric Heater Parameters

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `electric_kp` | 10.0 | 0.1-20 | Proportional gain - trigger threshold |
| `electric_ki` | 0.2 | 0.0-2 | Integral gain - persistent demand |
| `electric_kd` | 1.0 | 0.0-5 | Derivative gain - overshoot prevention |

**Typical behavior with defaults**:
- Strong immediate response (needed for binary control)
- Moderate integral (provides backup when hydraulic insufficient)
- Moderate damping (prevents excessive cycling)

#### Advanced Parameters

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `pid_integral_max` | 50.0 | 10-100 | Anti-windup limit for integral term |
| `pid_derivative_filter` | 0.1 | 0.01-0.5 | Low-pass filter coefficient for derivative |
| `water_temp_margin` | 2.0 | 0-10 | Min temperature margin for hydraulic operation (°C) |

---

### Tuning Guide

#### Step 1: Start with Default Values
The default parameters are designed for typical operation. Start here and only adjust if needed.

#### Step 2: Identify the Problem

**System too slow / Never reaches target**:
→ Increase Kp (proportional gain)

**System oscillates / Overshoots target**:
→ Decrease Kp, or increase Kd (damping)

**Target reached but offset remains** (e.g., settles at 39°C instead of 40°C):
→ Increase Ki (integral gain)

**System becomes unstable after running for hours**:
→ Decrease Ki, or decrease `pid_integral_max`

**Control output is jittery/noisy**:
→ Decrease `pid_derivative_filter` (more filtering)

#### Step 3: Adjust One Parameter at a Time

**Example: System oscillates between 38°C and 42°C**

1. Problem: Oscillation → likely high Kp or low Kd
2. Try: Increase `hydraulic_kd` from 2.0 to 3.0
3. Observe: If oscillations reduce, good. If not, try decreasing `hydraulic_kp`
4. Iterate until stable

#### Step 4: Fine-Tune

**Ziegler-Nichols Method** (advanced):
1. Set Ki = 0, Kd = 0 (P-only control)
2. Increase Kp until system oscillates with constant amplitude
3. Note Kp_critical and oscillation period T
4. Calculate:
   - Kp = 0.6 × Kp_critical
   - Ki = 1.2 × Kp_critical / T
   - Kd = 0.075 × Kp_critical × T
5. Use these as starting point and fine-tune

#### Step 5: Test Edge Cases

- **Startup**: Does system reach target smoothly?
- **Setpoint change**: Change target from 40°C to 50°C - does it transition well?
- **Disturbance**: Open door briefly - does it recover quickly?
- **Long run**: After 24 hours, is temperature still stable?

---

### Water Temperature Constraint

The hydraulic heater has a physical limitation: it can only heat air if the water temperature is sufficiently high.

#### Constraint Logic

```
min_water_temp = target_air_temp + water_temp_margin

IF water_temp < min_water_temp THEN
  disable hydraulic heater (set power to 0)
  reset hydraulic PID (prevent integral windup)
ELSE
  use hydraulic PID output normally
END IF
```

**Example**:
- Target air temperature: 40°C
- Water temperature margin: 2°C
- Minimum water temperature required: 42°C

If water is at 41°C:
- Hydraulic heater disabled (water not hot enough)
- Electric heater takes over
- When water reaches 42°C, hydraulic re-enabled

#### Why Reset PID?

When hydraulic is disabled, its PID is reset to prevent **integral windup**:
- Without reset, integral term would keep accumulating during constraint
- When constraint lifts, huge accumulated integral causes massive overshoot
- Reset keeps system stable

#### Circulator State

Water temperature is only valid when circulator is running:
- If circulator off (power = 0), water is stagnant
- Stagnant water temperature doesn't represent heating capacity
- Constraint is not applied when circulator off

---

### Troubleshooting

#### Problem: Temperature oscillates ±5°C around target

**Likely cause**: Kp too high or Kd too low

**Solution**:
1. Reduce `hydraulic_kp` by 20% (e.g., 5.0 → 4.0)
2. If still oscillating, increase `hydraulic_kd` by 50% (e.g., 2.0 → 3.0)

---

#### Problem: Temperature stabilizes 2°C below target

**Likely cause**: Ki too low (insufficient integral action)

**Solution**:
1. Increase `hydraulic_ki` by 50% (e.g., 0.1 → 0.15)
2. Wait 10+ minutes to observe effect (integral is slow)

---

#### Problem: Temperature is stable, but electric heater cycles too frequently

**Likely cause**: Electric Kp too sensitive to small errors

**Solution**:
1. Decrease `electric_kp` by 20% (e.g., 10.0 → 8.0)
2. Slightly increase `electric_ki` to compensate (e.g., 0.2 → 0.25)

---

#### Problem: After 6+ hours, temperature suddenly overshoots to 45°C (target 40°C)

**Likely cause**: Integral windup

**Solution**:
1. Decrease `pid_integral_max` by 30% (e.g., 50.0 → 35.0)
2. Or decrease Ki if integral accumulates too fast

---

#### Problem: Hydraulic heater stays at 0% even though water is hot

**Likely cause**: Water temperature constraint too strict

**Solution**:
1. Check water temperature sensor reading
2. Reduce `water_temp_margin` if reasonable (e.g., 2.0 → 1.5)
3. Verify circulator is running (power > 0)

---

#### Problem: Control output changes erratically every second

**Likely cause**: Temperature sensor noise affecting derivative term

**Solution**:
1. Increase filtering: decrease `pid_derivative_filter` (e.g., 0.1 → 0.05)
2. This adds more smoothing to derivative calculation

---

## Additional Resources

### Code References
- PID implementation: [include/PIDController.h](include/PIDController.h), [src/PIDController.cpp](src/PIDController.cpp)
- Temperature manager: [include/TemperatureManager.h](include/TemperatureManager.h), [src/TemperatureManager.cpp](src/TemperatureManager.cpp)
- Default parameters: [include/config.h](include/config.h)

### Unit Tests
- PID controller tests: [test/unit/test_pid_controller/pid_controller.cpp](test/unit/test_pid_controller/pid_controller.cpp)
- Water constraint tests: [test/unit/test_temperature_manager_pid/temperature_manager_pid.cpp](test/unit/test_temperature_manager_pid/temperature_manager_pid.cpp)

Run tests with:
```bash
pio test -e native
```

### Menu Navigation
PID parameters can be adjusted via the device menu:
```
Main Menu → Heating → Hydraulic Kp/Ki/Kd
Main Menu → Heating → Electric Kp/Ki/Kd
Main Menu → Heating → Water margin
```

---

**Last Updated**: 2026-02-02
