# Technical Documentation — renard'o Dryer Controller

## Table of Contents

- [Drying Sequence](#drying-sequence)
- [PID Temperature Control](#pid-temperature-control)
- [Operating Modes](#operating-modes)
- [Humidity Control](#humidity-control)
- [Session Persistence](#session-persistence)
- [Data Logging](#data-logging)

---

## Drying Sequence

The controller runs a fixed three-phase cycle. Durations and thresholds are defined in `config.h`.

```
Init ──► Brassage ──► Extraction ──► Brassage ──► Extraction ──► ...
 (×1)        (×∞ loop)
```

### Phase: Init

- **Purpose:** Bring the chamber up to target temperature before starting the cycle.
- **Exit condition:** Inlet temperature ≥ target, OR `INIT_PHASE_DURATION` seconds elapsed (whichever comes first).
- **Heaters:** Hydraulic at full power, electric follows PID.

### Phase: Brassage

- **Purpose:** Homogenise temperature and humidity throughout the chamber.
- **Exit condition:** `BRASSAGE_PHASE_DURATION` seconds elapsed.
- **Air damper:** Closed (recirculation).

### Phase: Extraction

- **Purpose:** Evacuate accumulated moisture from the chamber.
- **Exit condition:** Inlet humidity ≤ `EXTRACTION_HUM_THRESHOLD`, OR `EXTRACTION_PHASE_DURATION` seconds elapsed.
- **Air damper:** Opens when humidity exceeds threshold + 5 %RH deadband; closes when humidity drops to threshold.

---

## PID Temperature Control

Two independent PID controllers regulate the heating system.

### Hydraulic Heater (primary)

- **Output:** 0–100 % circulator power (proportional)
- **Characteristics:** High thermal inertia — slow but energy-efficient
- **Tuning:** Strong damping (Kd) to handle slow thermal response

```
hydraulic_output = PID(target, inlet_temperature, dt)
circulator_power = hydraulic_output  // 0–100%
```

### Electric Heater (supplement)

- **Output:** Binary ON/OFF (threshold: PID output > 0.5)
- **Characteristics:** Fast response, high power consumption
- **Disabled in ECO mode**

```
electric_output = PID(target, inlet_temperature, dt)
heater_state = electric_output > 0.5 ? ON : OFF
```

### Default PID Parameters

| Parameter | Hydraulic | Electric |
|-----------|-----------|---------|
| Kp | 5.0 | 10.0 |
| Ki | 0.1 | 0.2 |
| Kd | 2.0 | 1.0 |
| Integral max (anti-windup) | 50.0 | 50.0 |
| Derivative filter | 0.1 | 0.1 |

All values are defined in `config.h` and take effect at compile time.

### Tuning Guide

**System too slow / never reaches target** → increase Kp

**System oscillates / overshoots** → decrease Kp, or increase Kd

**Stable offset below target** (e.g. settles at 39 °C instead of 40 °C) → increase Ki

**Integral windup after hours of operation** → decrease `pid_integral_max` or Ki

**Noisy derivative output** → decrease `pid_derivative_filter` (more filtering)

Adjust one parameter at a time. Observe for at least 10 minutes before the next change (the integral term is slow).

---

## Operating Modes

Selected via physical switch on GPIO 22. Applies immediately each control cycle — no reboot needed.

Both heaters (hydraulic and electric) are available in **all modes**.

### PERFORMANCE

- Both heaters active at all times
- Full target temperature at all times

### ECO

ECO mode is time-aware. The switch enables the intent; the RTC clock determines whether the reduction applies.

| Time window | ECO switch OFF | ECO switch ON |
|-------------|---------------|--------------|
| Day (09h00 – 18h00) | Full target, both heaters | Full target, both heaters |
| Night (18h00 – 09h00) | Full target, both heaters | **85 % of target**, both heaters |

- **ECO LED** is lit only when the switch is ON **and** the current time is inside the night window (18h00 → 09h00)
- Default reduction: 85 % of target — e.g. target 40 °C → effective 34 °C at night
- The reduction percentage is set by `ECO_NIGHT_TARGET_PERCENTAGE` in `config.h`
- The time window boundaries are `ECO_START_HOUR` (18) and `ECO_END_HOUR` (9) in `config.h`

### User scenarios

**1. Lower the temperature target**
Turn the temperature potentiometer down before starting. The reduced setpoint is active immediately in both modes. Useful when the product being dried requires a gentler temperature.

**2. Daytime "flambée" — add calories before a low-energy night**
Keep the switch on PERFORMANCE during the day to run both heaters at full target and build up heat in the thermal store. Switch to ECO in the evening. The controller will then coast through the night at the reduced target, drawing on the stored calories.

**3. Anticipate an ECO night**
Activate the ECO switch before 18h00. During the day nothing changes (full target, both heaters). At 18h00 the controller automatically switches to the reduced target — no manual action needed overnight. Flip the switch back to PERFORMANCE the next morning if needed.

---

## Humidity Control

The air damper (Belimo LM24A-SR) is controlled as a binary open/close valve.

```
if inlet_humidity > target + 5 %RH deadband:
    open damper  (evacuate moisture)
elif inlet_humidity <= target:
    close damper (stop extraction)
```

A 10-second cooldown is enforced between state changes to prevent hunting.

During **Brassage** phase, the target is set to 0 (no control) — damper stays closed.
During **Extraction** phase, the target is `EXTRACTION_HUM_THRESHOLD`.

---

## Session Persistence

Session state is written to EEPROM via `PersistentStateManager` at key events:

| Event | What is saved |
|-------|--------------|
| START pressed | session_running = true, phase = Init, elapsed = 0 |
| STOP pressed | session_running = false |
| Every 5 minutes (while running) | current phase + elapsed times |

On boot, if `session_running = true` is found in EEPROM, the session resumes from the saved phase and elapsed time. The EEPROM checksum and version number guard against corrupt data.

EEPROM version: **9** — changing the `PersistentState` struct requires bumping `kStateVersion` in `PersistentStateManager.h`.

---

## Data Logging

Log files are written to the SD card in CSV format.

**Path:** `/sessions/YYYY/MM/YYMMXXXX.csv`
- `YYMM` — year + month (2-digit each)
- `XXXX` — sequential batch number within the month

**Log interval:** `DATA_LOG_INTERVAL` ms (default: 60 000 ms = 1 minute)

**Columns:**

| Column | Unit | Description |
|--------|------|-------------|
| timestamp | datetime | Human-readable date/time from RTC |
| inlet_temperature | °C | Air inlet temperature |
| inlet_hr | %RH | Air inlet humidity |
| outlet_temperature | °C | Air outlet temperature |
| outlet_hr | %RH | Air outlet humidity |
| fan_state | 0/1 | Fan relay state |
| hydraulic_heater_state | 0/1 | Circulator active |
| hydraulic_heater_power | % | Circulator power (0–100) |
| electric_heater_state | 0/1 | Electric heater relay state |
| air_damper_open | 0/1 | Damper open = 1, closed = 0 |
| target_temperature | °C | Current potentiometer setpoint |
| phase_name | string | Init / Brassage / Extraction |
| total_elapsed_s | seconds | Time since session start |
| phase_elapsed_s | seconds | Time since current phase start |

If the SD card is unavailable at session start, logging is disabled for that session. The controller retries SD initialisation every 30 seconds.
