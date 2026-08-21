# Technical Documentation — renard'o Dryer Controller (v4)

## Table of Contents

- [Drying Sequence](#drying-sequence)
- [Temperature Control](#temperature-control)
- [Safety Interlocks](#safety-interlocks)
- [Humidity and Air Damper](#humidity-and-air-damper)
- [Operator Interface](#operator-interface)
- [ECO Mode](#eco-mode)
- [Persistence](#persistence)

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

### The airflow interlock — the one that acts on the session

Every interlock above gates the **heat sources** and leaves the session running.
One does not, and it is the only one: on a dryer configured for two registers,
both of them reading shut means the air path is closed and the fan is moving
nothing.

| | |
|---|---|
| **Trips when** | both registers ≤ 10 % for 30 s, two registers declared |
| **Effect** | refuses `Dryer::Start()`, whatever asked for it, and stops a running session |
| **Clears** | by itself, on the next good reading. Nothing latches, nothing to acknowledge |
| **Shown as** | `REGISTRES FERMES - PAS DE CIRCULATION`, in the hint bar |

A second, narrower rule covers the case where the interlock cannot be evaluated
at all: with two registers declared and either feedback unusable — no signal, or
no usable calibration — **the start is refused** (`RECOPIE REGISTRE HS -
DEMARRAGE BLOQUE`). It does not stop a running session: a wire failing mid-cycle
must not cost the batch.

Both rules live in `Dryer::Start()` and `Dryer::Update()`; the decision itself is
in `AirDamper`, which is why it is covered by host tests.

---

## Humidity and Air Damper

The damper is strictly binary — recirculation or extraction — driven by the
current phase. `HumidityManager` operates it in two modes: `kDisabled` (closed,
Init and Brassage) and `kForceOpen` (Extraction and the Init sub-extraction).

Humidity does not modulate the damper; it decides **phase transitions**. The
target is compared against the inlet reading to leave Brassage early.

A dryer has one or two registers, set from the menu (`Nb registres`). With two
they are complementary — one relay drives both, one of them travelling the other
way — and which way each one travels is itself a setting, because each Belimo
carries its own mechanical direction switch that the firmware cannot read.

The Belimo's 2-10 V position feedback is read on its own ADC channel per
register. It drives the display, showing the vane travelling during its ~150 s
stroke, and one safety decision: the airflow interlock above. Calibration is two
ordered raw marks per register (`mini`, `maxi`) plus one `Sens signal` flag
shared by all of them, all captured from the menu.

---

## Operator Interface

A 320×240 TFT and a rotary encoder replace every panel control of v3. The layout
follows the design mock-up in `design_handoff_sechoir_tft`, transposed from CSS
to drawing primitives.

### Typography

The interface is set in monospace faces generated from Hack Bold by
`tools/make_gfx_font.py` into `include/fonts/`. The fonts built into TFT_eSPI do
not work here: only the GLCD font is monospace and it is far too coarse, while
fonts 2, 4 and 6 are proportional, so a reading moving from `41.9` to `42.3`
shifts sideways every refresh — the most distracting defect there is on a screen
that is glanced at.

**The type scale is one step above the design mock-up's, and bold throughout.**
The mock-up was drawn to be read on a monitor at 2.5×; transposed literally it
is unreadable, and that is arithmetic, not taste. The GMT020 is a 2 inch part,
so 320 × 240 lands on 40.6 × 30.5 mm and one pixel is 0.127 mm:

| Cut | Cap height | On the panel |
|---|---|---|
| Mono12B | 9 px | 1.14 mm — captions |
| Mono14 / Mono14B | 10 px | 1.27 mm — values, menu rows |
| Mono18B | 13 px | 1.65 mm — setpoint figures |
| Mono26B | 20 px | 2.54 mm — injection figures |

ISO 9241 puts comfortable reading at roughly 20 arcminutes of subtended height,
which is 2.3 mm at 40 cm. Only the injection figures clear that; the captions
are read closer. Going further would mean showing less, which is a design
decision rather than a typographic one.

Glyphs are rasterised through FreeType's **monochrome** mode, not greyscale and
a threshold. The hinting instructions then snap stems and bowls onto the pixel
grid; thresholding a greyscale render instead leaves stems landing half way
across a pixel boundary, so at 12 px some come out one pixel wide and others
two, and letters stop looking like themselves.

Every glyph in a cut shares one cell, so the width of a string is its length
times the advance. `TftDisplay` uses that to check its own layout: the longest
label each cell can hold is a `static_assert`, so a future change of type scale
fails to build rather than silently clipping `HYDRAULIQUE` to `HYDRAULIQU`.

Labels are unaccented, as the rest of the firmware's on-screen strings are: a
GFXfont covers one contiguous range of code points, and reaching Latin-1 for
three accented words would take the glyph count from 95 to 224 and force a
UTF-8 transcode on every string. The one non-ASCII character the interface does
need, the degree sign, rides on the tilde; `UI_DEGREE` is the only place that
spells that out. Regenerating the fonts costs ~10 KB of bitmap data and ~23 KB
of flash all told, on 1.5 MB.

### Main screen

| y | Region | Contents |
|---|---|---|
| 0–3 | Progress | how far the running phase has gone, in the phase colour |
| 4–23 | Header | phase dot and name (left), elapsed time (centred) |
| 32–107 | Cards | INJECTION · CONSIGNE |
| 112–145 | Strip | hydraulic state, circulating and tank water temperatures |
| 150–221 | Devices | fan (animated), electric heating, extraction, recycling |
| 226–239 | Hint | empty, or the highest-ranked active alarm |

Every block centres its contents. Each card carries a label and two figures, and
each figure carries its own unit — `42.3°` and `38%` — so the captions that named
them are gone and the numbers have the room instead. There is no `C` after the
degree sign: everything this machine measures is in Celsius, and the letter
costs a whole glyph cell at this size.

The figures are right-aligned inside fixed slots, five cells for a temperature
and four for a humidity. Neither field is fixed width — `9.5°` is a cell
narrower than `42.3°`, `38%` a cell narrower than `100%` — so left-aligning them
would slide the centred block sideways the first cold morning, and right-aligning
also keeps the degree and percent signs on unmoving columns.

The mock-up's two vertical gauges beside the injection figures are gone. They
plotted the reading against a 0–90 °C scale on a machine whose safety cut-out is
at 50 °C, so they never left the bottom third and repeated, less precisely, what
the number beside them already said.

Elapsed time is now the centred field and the phase name is pinned left — the
opposite of the previous layout. With a monospace font `HH:MM:SS` never changes
width, so it is the one that can be centred without the header jittering, and
the phase reads better beside its own coloured dot.

The progress bar is an estimate, not a countdown: brassage and extraction both
end on a humidity threshold when the crop dries faster than the clock allows, so
the bar can fill before the phase changes, or the phase change while the bar is
part way. It answers "is this phase well along".

The recycling cell reads `ABSENT`, greyed and slashed, on a dryer configured for
a single register. Dashes there would mean "should be reading and is not", which
is a fault; a register the machine does not have is not one, and the screen must
not blur the two.

The hint band carries one alarm at a time, ranked rather than rotated so the
worst news is never the hardest to catch: blocked airflow first, because it is
the only one that stops the dryer, then an unusable register feedback, which
refuses the next start, then a stale inlet probe, which blocks the heating.

### What the colours mean

One hue, one meaning, across both screens — the code should be learnable once
from across the room and never need the word underneath it read.

| Colour | Means | Where |
|---|---|---|
| Red `#ff5a5a` | broken, wants attention now | sensor alarm, hydraulic `ABSENT` |
| Amber `#ffb020` | transient or noteworthy, nothing wrong | register in transit, `REFROID.`, edit mode, extraction phase |
| Green `#5bd97f` | running as intended | fan, heater, circulator, the register on the commanded air path, eco while it lowers the setpoint |
| Blue `#5aa9e6` | available and idle, or a plain reading | `OFF` pills, water temperatures, the warm-up phase, eco outside its hours |
| Grey `#6f8a88` | switched off in the configuration, or a caption | `DESACT.`, captions, unavailable menu rows |
| Cyan `#3fe6d4` | live, or selected | injection figures, menu cursor, brassage phase |
| White `#e9f4f2` | asked for, not measured | setpoint figures, the clock |

The distinction that earns its keep is blue against grey against red. A stopped
fan, a fan disabled at the menu and a hydraulic module that has stopped
answering are three different situations, and only the last is a fault; painting
all three red — which is what colouring "not ON" invites — teaches the operator
to ignore red. For the same reason a running heater is green like everything
else that runs, not amber: a heater doing its job is not a warning.

Degraded states stay words rather than a missing `ON`: the hydraulic cell reads
`ABSENT` when the module has stopped answering and `DESACT.` when it is switched
off at the menu. The electric cell reads `DESACT.` the same way, the fan reads
`REFROID.` through the post-stop cooldown, and a register with no usable feedback
reads `--` rather than the `FERME` that would be taken for a measurement.

The sensor alarm moved from the header to the hint bar, which the mock-up leaves
empty on the dashboard. It buys the alarms a full 320 px line, where the header
corner they used to share could only ever hold the shortest of them.

A missing reading renders as `--.-`, never as `0.0`.

The header's right-hand corner carries the eco badge — a leaf and the word
`ECO` — in two states, because armed and acting are different facts. Green says
the setpoint on the card below has actually been lowered, which is the only
thing on screen that explains why it dropped; blue says the schedule is set and
waiting for its hours, blue rather than grey because eco outside its window is
enabled and idle, not switched off. Nothing is drawn when eco is off, and nothing is
drawn without an RTC, since eco cannot keep a schedule it cannot read the clock
for. That corner was the one piece of the layout with nothing in it: the phase
name stops around x=117 and the centred clock ends at 192.

### The icons

They are rasterised from the Material Design set carried by Hack Nerd Font —
the same font file the interface type is generated from — rather than drawn out
of triangles as they were. See `tools/make_icons.py`.

The two registers each have their own icon now: air leaving a box for
extraction, the recycling loop for recirculation. They used to share one drawing
of a duct with a vane in it, which is the one thing two neighbouring status
cells must not do — it makes the operator read the caption to learn which is
which, every time.

What the vane's angle used to say, the words below it say instead: `FERME`,
`OUVERT`, `OUV. nn%` or `--` from the measured position. A feedback that has
become unusable, which the vaneless duct used to signal by absence, raises
`RECOPIE REGISTRE HS` in the hint bar, where it gets a full line.

Rendering stays region-based: each frame is compared against the previous model
and only the regions whose contents changed are redrawn, each through its own
sprite. The fan animation repaints only its 30×30 disc, not the 320×72 device
row.

### Menu

Rotation moves the cursor, a click enters or edits, and each page ends with an
explicit `< Retour` — there is no long press. Booleans flip on a click. The
cursor stops at the ends rather than wrapping.

Pages: Consignes, Sources, Mode ECO, Phases, Régulation, Système. Seven rows fit
between the header and the hint bar, so the root page is on screen whole and the
most-used page never scrolls.

A hint bar along the bottom says what the knob does — `TOURNER naviguer · CLIC
ouvrir`, `CLIC modifier` on a value, `TOURNER regler · CLIC valider` while
editing. This is where the mock-up and the machine part company: the mock-up
navigates with five keys and names them along the bottom of every screen, and
the dryer has one encoder, so the band names the knob's actions instead.

The cursor is a filled, outlined pill rather than a bar across the screen — the
same shape as the dashboard's cards, so the two screens read as one interface —
and the selected row is set in the bold cut, which shows at a distance where a
highlight alone might not.

Entries that make no sense in the current configuration are greyed out and
skipped rather than hidden, so the menu keeps the same shape whatever hardware
is fitted.

`Système > Registres` holds everything about the air path the firmware cannot
measure for itself: how many registers the dryer has, which end of the feedback
signal means open (one flag for all of them — same actuator model everywhere),
where each actuator's own direction switch is set, and the two raw calibration
marks per register. Two read-only rows show each channel's live raw value and
opening, which is how the marks are captured; `absent` there means the channel is
carrying no signal at all. Two actions, `Vers extraction` and `Vers recirc.`,
drive the air path from the menu — needed both to send a register to its stops
while calibrating, and as the only way out of an airflow fault, since nothing
else commands the damper while the dryer is stopped.

`Système > Date / Heure` sets the RTC. Its entries edit a staging copy read from
the chip when the page opens, and nothing is written until `Valider` — a date
typed one field at a time would otherwise pass through impossible values. A day
beyond the end of the chosen month is corrected on commit, not while turning the
knob. The read-only `Horloge` row shows what the chip currently holds.

**START and STOP remain physical and always act**, whatever is on screen. Two
dedicated buttons rather than one toggling both ways: a press means the same
thing whatever the dryer is doing, which is what matters for the hand reaching
for STOP. Each fires once per press and is ignored while held, and a button
stuck low at boot is treated as already consumed — otherwise a stuck STOP would
end a session restored from flash the moment the board came back up.

STOP is read first, so pressing both at once stops the dryer. Asking for the
state the dryer is already in does nothing, and the start preconditions — the
airflow interlock and a usable register feedback — stay inside `Dryer::Start()`,
so every route into a session goes through the same door.

## Status LEDs

Two LEDs on the panel, green and red, say what the machine is doing from across
the room:

| State | Green | Red |
|---|---|---|
| running | steady | out |
| cooling down | blinking | out |
| stopped | out | steady |
| fault | out | blinking |

No state leaves both dark, so an unpowered board does not look like a dryer at
rest. The cooldown is the fan still turning after a stop, for
`FAN_COOLDOWN_DURATION_S` — a blinking green rather than the steady red of a
machine that has actually finished.

A fault wins over a running session: a silent probe blocks the heat sources but
not the fan, so the dryer can be turning while something is wrong, and that is
when the panel must say so. Three conditions light it — no airflow, a silent
inlet probe, and a hydraulic module that stopped answering while its source is
enabled in the menu. Nothing is reported for the first 15 s after boot, where
neither the probe nor the hydraulic module has answered yet.

---

## ECO Mode

Reduces the setpoint to a configurable percentage during a night window.

**Requires the optional RTC.** Without a wall clock the window cannot be
evaluated, so the whole ECO submenu is greyed out and the mode is forced to
PERFORMANCE regardless of what is stored. The window wraps around midnight when
the start hour is later than the end hour.

The clock itself is set from `Système > Date / Heure`. Firmware only seeds the
RTC with its build time when the chip has clearly never been set (year before
2020) or its oscillator has stopped: a time set by hand survives both reboots
and firmware updates, which it would not if every flash reset it.

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
or checksum does not match is discarded in favour of the factory defaults —
there is no migration, by design.

`SETTINGS_VERSION` is at **4**. v4 dropped the LoRa telemetry interval along
with the radio; the field sat immediately before the checksum, so the record is
shorter and no v3 file can be read as a v4 one. v3 before it replaced each
register's named calibration ends (closed, open) with ordered marks (min, max)
plus an explicit signal direction, and added the register count and the two
actuator direction flags.

Either way the effect on a board being upgraded is the same and it is worth
stating plainly: **the stored calibration is discarded and has to be captured
again from the menu.**

A reboot mid-cycle resumes the session at its phase and elapsed time. Elapsed
time is `millis()`-based, so the wall-clock gap during the outage is lost.
