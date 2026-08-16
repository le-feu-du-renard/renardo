# Technical Documentation — renard'o Dryer Controller (v4)

## Table of Contents

- [Drying Sequence](#drying-sequence)
- [Temperature Control](#temperature-control)
- [Safety Interlocks](#safety-interlocks)
- [Humidity and Air Damper](#humidity-and-air-damper)
- [Operator Interface](#operator-interface)
- [ECO Mode](#eco-mode)
- [Persistence](#persistence)
- [Remote Link](#remote-link)

---

## Drying Sequence

A fixed three-phase cycle. Durations are configurable from the menu and
persisted; `config.h` only supplies the factory defaults.

```
Init ──► Brassage ──► Extraction ──► Brassage ──► Extraction ──► ...
 (×1)        (×∞ loop)
```

The cycle loops until STOP is pressed — there is no automatic end.

### Phase: Init

- **Purpose:** bring the chamber up to target temperature before cycling.
- **Exit:** inlet temperature ≥ target, or the Init duration elapses.
- **Damper:** closed. If inlet humidity reaches the target during Init, the
  damper opens for the configured extraction window, then closes and Init
  continues. If less than that window remains, the controller goes straight to
  Extraction.

### Phase: Brassage

- **Purpose:** homogenise temperature and humidity through the chamber.
- **Exit:** inlet humidity ≥ target (early transition), or the Brassage
  duration elapses.
- **Damper:** closed (recirculation).

### Phase: Extraction

- **Purpose:** evacuate accumulated moisture.
- **Exit:** always runs its full duration.
- **Damper:** open.

Every phase transition calls `TemperatureManager::ResetControl()`, which clears
both sources and their anti-short-cycle timers.

---

## Temperature Control

Two **independent on/off sources** share the same measured inlet temperature.
There is no PID: the remote hydraulic module only accepts a state and a fixed
water setpoint, and its three-way valve is far too slow to be modulated, so
there is nothing left for a continuous output to act on.

### Hydraulic — base heat

Wide hysteresis, slow cycling. The module holds a fixed water temperature set
from the menu; the dryer only decides when it runs.

| Condition | Effect |
|---|---|
| error > `CTRL_BANDE_HYDRO` (1.5 °C) and OFF for ≥ `CTRL_HYDRO_T_OFF_MIN` | turn ON |
| error ≤ 0, or overshoot predicted, and ON for ≥ `CTRL_HYDRO_T_ON_MIN` | turn OFF |

The long minimum on/off times (300 s each) protect the valve and the
circulator, and must exceed the time the valve needs to travel.

### Electric — fine trim

Narrow hysteresis, fast cycling, closing the last fraction of a degree the
hydraulic cannot resolve.

| Condition | Effect |
|---|---|
| error > `CTRL_BANDE_ELEC` (0.5 °C) and OFF for ≥ `CTRL_T_OFF_MIN` | turn ON |
| error ≤ 0, or overshoot predicted, and ON for ≥ `CTRL_T_ON_MIN` | turn OFF |

Because `CTRL_BANDE_HYDRO` sits well above `CTRL_BANDE_ELEC`, a large error
engages both sources while a small one is trimmed by the electric alone.

### Predictive shutoff

Both sources cut out early when the temperature is climbing fast enough to
sail past the setpoint on inertia alone:

```
T + dT_dt × horizon ≥ setpoint   →   turn off
```

This only applies when `dT_dt > CTRL_DT_PREDICT_MIN` (0.05 °C/s). The probes
report in 0.1 °C steps, and a single quantisation step produces a filtered
derivative around 0.03 °C/s — without that gate, sensor noise alone would cut
the heating short. The hydraulic uses a longer horizon than the electric,
because the water loop keeps giving off heat well after the circulator stops.

### Reported state

`ControlState` describes which sources are usable, not a mode being switched
between: `HYDRAULIC_ELECTRIC`, `HYDRAULIC_ONLY`, `ELECTRIC_ONLY`, or `OFF`.

---

## Safety Interlocks

Four independent conditions gate heating. All must hold.

| Interlock | Effect when false |
|---|---|
| `heating_permitted_` — inlet probe fresher than `SENSOR_TIMEOUT_MS` | both sources off |
| `fan_active_` — the fan is running | both sources off |
| `electric_enabled_` / `hydraulic_enabled_` — menu toggles | that source off |
| `hydraulic_online_` — the module answered within 30 s | hydraulic off |

Plus two hard cutoffs inside the control loop: a sensor fault (NaN, or outside
−20…200 °C) and the measured temperature exceeding the configurable safety
maximum.

**Sensor freshness matters most.** If the probe goes silent, its last value
would otherwise sit frozen forever while the heaters chased it. The reading
carries a timestamp published across cores, and heating is blocked as soon as
it goes stale. A fault is shown as `SONDE` in the status bar.

Losing the hydraulic module degrades to electric-only; it never stops a
session.

---

## Humidity and Air Damper

The damper is strictly binary — recirculation or extraction — driven by the
current phase. `HumidityManager` operates it in two modes: `kDisabled` (closed,
Init and Brassage) and `kForceOpen` (Extraction and the Init sub-extraction).

Humidity does not modulate the damper; it decides **phase transitions**. The
target is compared against the inlet reading to leave Brassage early.

The Belimo's 2-10 V position feedback is read on ADC2 and used **only for
display**, showing the vane travelling during its ~150 s stroke. Calibration is
two-point, from the menu.

---

## Operator Interface

A 320×240 TFT and a rotary encoder replace every panel control of v3.

### Main screen

| Region | Contents |
|---|---|
| Status bar | elapsed time (left, fixed width), phase (centred), LoRa icon |
| Tiles | injection measurement / setpoint |
| Hydraulic block | circulator ON/OFF, circulating and tank water temperatures |
| Status band | fan (animated), electric heating, damper state |

Elapsed time is on the left because `HH:MM:SS` never changes width, while the
phase name does; centring the one that moves keeps the bar from jittering.

A missing reading renders as `--.-`, never as `0.0`.

### Menu

Rotation moves the cursor, a click enters or edits, and each page ends with an
explicit `< Retour` — there is no long press. Booleans flip on a click. The
cursor stops at the ends rather than wrapping.

Pages: Consignes, Sources, Mode ECO, Phases, Régulation, Système.

Entries that make no sense in the current configuration are greyed out and
skipped rather than hidden, so the menu keeps the same shape whatever hardware
is fitted.

**START and STOP remain physical and always act**, whatever is on screen.

---

## ECO Mode

Reduces the setpoint to a configurable percentage during a night window.

**Requires the optional RTC.** Without a wall clock the window cannot be
evaluated, so the whole ECO submenu is greyed out and the mode is forced to
PERFORMANCE regardless of what is stored. The window wraps around midnight when
the start hour is later than the end hour.

---

## Persistence

LittleFS on internal flash, replacing the v3 SD card. Two independent records,
each versioned and checksummed:

| File | Contents | Written |
|---|---|---|
| `/settings.bin` | everything the menu can change | on each commit |
| `/session.bin` | phase and elapsed time | on start/stop, then every 60 s |

Records are written to a temporary file and renamed over the target, so a power
cut costs the new values rather than the previous ones. A record whose version
or checksum does not match is discarded in favour of the factory defaults.

A reboot mid-cycle resumes the session at its phase and elapsed time. Elapsed
time is `millis()`-based, so the wall-clock gap during the outage is lost.

---

## Remote Link

An SX1262 at 868 MHz talks to the Commander, which has the internet connection.
Session logging happens server-side; the dryer keeps none.

**Uplink:** a 43-byte telemetry frame every 60 s — both probes, both water
temperatures, setpoints, phase, elapsed time, actuator states, damper position.
Readings travel as signed tenths with a distinct sentinel for "no value", so a
missing probe is not reported as a real zero.

**Downlink:** START, STOP, set temperature, set humidity. Each frame carries a
device id and a sequence number. Frames addressed to another dryer are dropped.
The Commander repeats until acknowledged, so duplicates are normal and executed
only once; a superseded setpoint arriving late is discarded. Sequence numbers
wrap in a byte, handled as a signed window.

A remote setpoint change goes through the same record the menu edits, so it is
persisted and shown on screen like any other.

The radio never participates in regulation: if it fails to initialise, the
dryer logs it and carries on.
