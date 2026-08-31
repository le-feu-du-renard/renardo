# Technical Documentation — renard'o Dryer Controller (v4)

## Table of Contents

- [Programmes](#programmes)
- [Drying Sequence](#drying-sequence)
- [Temperature Control](#temperature-control)
- [Safety Interlocks](#safety-interlocks)
- [Heat Sources](#heat-sources)
- [Humidity and Air Damper](#humidity-and-air-damper)
- [Operator Interface](#operator-interface)
- [ECO Mode](#eco-mode)
- [Persistence](#persistence)
- [Open questions](#open-questions)

---

## Programmes

A dryer and a climate chamber are the same machine asked two different
questions, and the answer is one setting: **Programme > Programme**.

| | Sechage | Climat |
|---|---|---|
| Phases | `Init → [Brassage → Extraction] ×∞` | one, `Climat` |
| Clock | four configurable durations | none |
| Register | driven by the phase | driven by the humidity threshold |
| Ends | on STOP | on STOP |

The whole of the difference is in `SessionManager`. Everything under it — the
interlocks, the safety cutoff, the air-renewal window, the source law — is
common, and never learns which programme is running.

**Climat is what `HumidityManager::Mode::kThreshold` was written for.** The mode
has existed since v3 and was selected by nothing: open while the air is too
damp, shut once it is not, with a 5 %RH deadband and a 10 s cooldown between
movements. Holding a climate is exactly that and nothing more.

The page is named for the choice rather than for the mechanism — "Phases" named
what the drying programme happens to be made of, which is exactly what a climate
has none of. The four phase durations are greyed out under Climat, and the programme itself
is greyed out while a session runs — the phase machine is already inside one,
and `SessionManager` refuses the change from its own side as well, because the
menu is not the only way in.

With a dehumidifier fitted, Climat leaves the register shut instead of putting
it on the threshold: that machine takes the water out without opening anything,
and a threshold underneath would be a second opinion on the same vane. See
[Heat Sources](#heat-sources).

---

## Drying Sequence

The `Sechage` programme. A fixed three-phase cycle; durations are configurable
from the menu and persisted, and `config.h` only supplies the factory defaults.

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

Every phase transition moves the damper, and every damper movement calls
`TemperatureManager::NotifyAirRenewal()` — including the Init sub-extraction,
which happens in the middle of a phase and used to announce itself to nothing.
See [Air renewal](#air-renewal--the-two-minutes-the-loop-is-told-to-ignore).
Only entering Init resets the control outright, because only entering Init is a
session start.

---

## Temperature Control

Two heat sources, and the dryer regulates **one** of them. The split is one of
authority, not of speed.

### The hydraulic is not regulated here

The remote module owns the three-way valve, the circulator, **and its own
regulation**: it decides when to fire and holds the water at the setpoint it is
given. What travels over RS485 is a *run permission* and that setpoint, and the
permission is held for the whole session rather than toggled.

Earlier firmware cycled the module on a 1.5 °C band with 300 s minimum on and off
times, which put a second regulator on a valve that takes minutes to travel — and
the slower of the two controllers was not this one. Worse, the protection was not
real: `ResetControl()` fired on every phase transition, so the 300 s guard was
wiped roughly every 19 minutes and the phase machine could drop the valve out and
re-energise it seconds later. Handing the loop back to the module removes the
argument and the timers along with it.

| Condition | Permission |
|---|---|
| `hydraulic_enabled` at the menu, a session running, every interlock holding | raised |
| anything in [Safety Interlocks](#safety-interlocks) failing, or the session ending | withdrawn |

Deliberately **not** conditioned on the module answering: an unreachable module
cannot be written to either way, and folding that in would only restate what the
availability check already says. If the bus dies while the module is running,
nothing on the dryer can reach it — that is the module's own watchdog to handle,
and [HARDWARE.md](HARDWARE.md) requires one for exactly this reason.

### Electric — the regulated source

Narrow hysteresis on the measured inlet temperature, fast cycling.

| Condition | Effect |
|---|---|
| error > `CTRL_BANDE_ELEC` (0.5 °C) and OFF for ≥ `CTRL_T_OFF_MIN` | turn ON |
| error ≤ 0, or overshoot predicted, and ON for ≥ `CTRL_T_ON_MIN` | turn OFF |

There is no PID here either, and for the same reason there never was: the
electric is a contactor. A continuous output would have nothing to act on.

### Predictive shutoff

The electric cuts out early when the temperature is climbing fast enough to sail
past the setpoint on inertia alone:

```
T + dT_dt × CTRL_HORIZON ≥ setpoint   →   turn off
```

This only applies when `dT_dt > CTRL_DT_PREDICT_MIN` (0.05 °C/s). The probes
report in 0.1 °C steps, and a single quantisation step produces a filtered
derivative around 0.03 °C/s — without that gate, sensor noise alone would cut
the heating short.

### Air renewal — the two minutes the loop is told to ignore

Opening the extraction injects outside air, and the inlet temperature falls. That
part is fine and expected. The damage was on the way back: with the register shut
again the chamber recovers at a rate far steeper than any approach to setpoint the
60 s horizon was sized for, the prediction above reads it as an impending
overshoot, and the electric is cut degrees short — where `CTRL_T_OFF_MIN` then
holds it for another minute. A short transient turned into a sag, every cycle.

So every damper movement calls `NotifyAirRenewal()`, which opens a window of
`CTRL_AIR_RENEWAL_S` (120 s) and restarts the derivative — either side of the
movement the probe is reading a different body of air, and the slope built up
before it describes nothing still true.

| Inside the window | |
|---|---|
| **Suspended** | the predictive shutoff, and the electric minimum off-time |
| **Untouched** | the band, the error ≤ 0 cutoff, the minimum on-time, the safety maximum, every interlock |

The window relaxes **when the heater may restart, never how hot it is allowed to
get**. Two minutes off-setpoint while the air is being renewed is an accepted
cost; the controller sawtoothing because of it was not.

It hangs off the damper rather than the phase because the damper is what causes
it. Init's sub-extraction opens the register for two minutes in the middle of a
phase, and hooking phase transitions would have missed it entirely. So does the
end of a fault purge, which returns the register to whatever the phase wanted.

The transition itself no longer cuts the electric. `ResetControl()` used to fire
on every phase entry — opening the contactor at the exact moment cold air arrived
and the chamber most needed heat — and is now reserved for session start.

### Reported state

`ControlState` describes which sources are usable, not a mode being switched
between: `HYDRAULIC_ELECTRIC`, `HYDRAULIC_ONLY`, `ELECTRIC_ONLY`, or `OFF`.

---

## Safety Interlocks

Four independent conditions gate heating. All must hold.

| Interlock | Effect when false |
|---|---|
| `heating_permitted_` — inlet probe fresher than `SENSOR_TIMEOUT_MS` | electric off, hydraulic permission withdrawn |
| `fan_active_` — the fan is running | electric off, hydraulic permission withdrawn |
| `electric_enabled_` / `hydraulic_enabled_` — menu toggles | that source off |
| `hydraulic_online_` — the module answered within 30 s | the fault and the display only |

Plus two hard cutoffs inside the control loop: a sensor fault (NaN, or outside
−20…200 °C) and the measured temperature exceeding the configurable safety
maximum. Both withdraw the hydraulic permission as well as cutting the electric.

**Sensor freshness matters most.** If the probe goes silent, its last value
would otherwise sit frozen forever while the heaters chased it. The reading
carries a timestamp published across cores, and heating is blocked as soon as
it goes stale. A fault is shown as `SONDE` in the status bar.

`hydraulic_online_` is the one row that gates nothing. It raises the fault and
greys the cell, but the permission says what the dryer *wants* and losing the bus
does not change that — it only stops the answer getting through. Losing the module
degrades the machine to electric-only; it never stops a session.

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

## Heat Sources

Three of them, and only two are commanded here.

**Hydraulic** — a remote module on RS485 @10, which owns its own start, its
circulator and its water loop. What leaves the dryer is a run permission and a
setpoint. See [The hydraulic is not regulated here](#the-hydraulic-is-not-regulated-here).

**The command output**, `OUT_ELECTRIC_PIN`, which drives either a resistance or
a dehumidifier. One relay, so which one is fitted is a setting — **Sources >
Type source** — and not a second pin. A machine carries one or the other, never
both.

It is not a preference. The two dry by opposite means: a resistance heats air
that is then thrown away carrying the moisture with it, a dehumidifier condenses
the moisture out and keeps the air. Getting the setting wrong does not degrade
the regulation, it inverts it.

| | Chauffage electrique | Deshumidificateur |
|---|---|---|
| Starts when | `error > band` | `error > band` **or** `RH > target + 2 %` |
| Stops when | `error ≤ 0` or predicted overshoot | `error ≤ 0` **and** `RH ≤ target` |
| Predictive shutoff | yes | **no** |
| Register | follows the phase | recirculation, except as below |
| Extraction phase | yes | **skipped** |

The predictive shutoff is calibrated on the thermal step of a resistance. A
compressor whose heat is a by-product does not produce one, and cutting it
degrees short on a ramp it did not cause is simply wrong. The minimum on and off
times, by contrast, matter more here than they ever did: a compressor
short-cycled is a compressor damaged.

Both calls must be answered before a dehumidifier stops, so an unmet humidity
holds it running past its temperature setpoint. That is deliberate, and it is
what the register threshold below exists to catch.

### A dehumidifier's register opens for two reasons, and both are air renewals

The circuit stays in recirculation: the machine condenses the water out rather
than throwing the air away, so the register is not what removes moisture.

| | Opens | Closes |
|---|---|---|
| Too hot | `T > setpoint + Seuil extract.` | `T ≤ setpoint` |
| Too dry | `RH < target` | `RH ≥ target + 3 %` |

The first is the only case where the register is a cooling device: the machine's
own waste heat has carried the chamber past the setpoint and outside air is the
only way down. The threshold *is* the hysteresis — a separate return value would
be a second setting saying one thing.

The second is the drying loop itself, and it runs the opposite way round from
the electric's. Below the humidity target there is nothing left in the air to
condense and the machine has nothing to work on; renewing brings damp air back
in and the drying continues. **The electric throws out air that has become
humid; the dehumidifier renews air that has become dry.**

Overheating outranks dry air. They want the same register, so the question is
never which applies but which is being answered, and a fault purge outranks them
both — that is a safety, these are regulation.

Every one of these movements calls `NotifyAirRenewal()`. They were the last two
the dryer could command with nothing anywhere being told, which is the exact
fault [that window](#air-renewal--the-two-minutes-the-loop-is-told-to-ignore)
was introduced to fix.

The Extraction phase is skipped entirely with a dehumidifier fitted. A phase
that periodically empties the circuit works against a machine that condenses out
of it; Brassage simply repeats. A source swapped mid-session leaves Extraction
at once rather than serving out a duration that no longer means anything.

---

## Humidity and Air Damper

The damper is strictly binary — recirculation or extraction. Under the drying
programme it follows the phase: `HumidityManager` runs `kDisabled` (closed, Init
and Brassage) and `kForceOpen` (Extraction and the Init sub-extraction). Under
the climate programme it runs `kThreshold` on the humidity target. Above the
mode sits one ranked override, `ForceOpen` — `kPurge`, `kOverheat`,
`kAirRenewal` — for the three things that happen *to* a session rather than
being a stage of one.

Under the drying programme, humidity does not modulate the damper; it decides
**phase transitions**. The target is compared against the inlet reading to leave
Brassage early.

A dryer has one or two registers, set from the menu (`Nb registres`). With two
they are complementary — one relay drives both, one of them travelling the other
way — and which way each one travels is itself a setting, because each Belimo
carries its own mechanical direction switch that the firmware cannot read.

Which relay state that single command means is a setting too (`Sens commande`),
and it is the only one that decides where the air goes: the direction flags
describe travel and feedback, and no combination of them can move a vane. It
stops at the output layer — `Dryer::GetDamperOutput()` is the pin, and everything
above it reads `GetDamperOpen()`, the commanded air path.

The Belimo's 2-10 V position feedback is read on its own ADC channel per
register. It drives the display, showing the vane travelling during its ~150 s
stroke, and one safety decision: the airflow interlock above. Calibration is two
ordered raw marks per register (`mini`, `maxi`) plus the `Sens signal` flag,
captured from the menu.

That flag is the actuator model's sense, not the register's: the direction switch
mirrors the position feedback as well as the travel, so a complementary pair puts
out the *same* voltage on both channels with one register open and the other
shut. Each register's real sense is `Sens signal` turned round again wherever its
own direction flag is set — combined once, in `AirDamper::ApplyConfig()`. The two
signal rows on the Registres page read out `ouvert` / `ferme`, or `ouverture` /
`fermeture` while a register travels, beside the raw count — so the pair can be
checked from that page alone, and without sitting through the 150 s stroke; see
HARDWARE.md.

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
| 28–107 | Cards | INJECTION · CONSIGNE |
| 112–145 | Strip | hydraulic state, circulating and tank water temperatures |
| 150–221 | Devices | fan (animated), heating or dehumidifier, extraction, recycling |
| 226–239 | Hint | empty, or the highest-ranked active alarm |

Every gap between bands is one gutter, 4 px — the same figure that separates the
cells inside a row, so the screen reads on one rhythm vertically and
horizontally. The progress bar is the deliberate exception: it is a full-bleed
hairline in the phase colour and sits flush under the top edge and against the
header, which is what makes it read as the header's own edge rather than a band
of its own. The rhythm is asserted at compile time, not merely checked for
overlaps.

**With the hydraulic switched off the strip is not blanked — it is gone**, and
its 38 px are given back, 19 to each: the cards run to 126 and the device row
from 131 to 221. Three cells reporting a water loop that is not fitted are a third of the
usable height spent on nothing. Only the vertical figures change between the two
variants; every width is shared, which is what leaves all nine assertions about
text fitting its cell true as written. A change of variant forces a full
repaint, because per-region diffing cannot chase a band whose region has moved.

The second device cell is captioned `CHAUFF.` or `DESHU.` after what is actually
wired to the command output.

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
| Green `#5bd97f` | running as intended | fan, heater, hydraulic cleared to run, the register on the commanded air path, eco while it lowers the setpoint |
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

On the hydraulic cell, `ON` reads "cleared to run", not "the circulator is
turning". The module regulates itself and never reports when it fires, so
claiming more would be claiming something nobody measured.

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

What the vane's angle used to say, the words below it say instead: `OUVERT`,
`FERME`, `OUV.nn%` / `FER.nn%` or `--`, from the measured position **judged
against the commanded end**. That last part is what stops the cell contradicting
the command that has just been given: a register commanded open sits at 0 % for
the first seconds of its 150 s stroke, and testing the two ends on their own —
which is what this did — printed `FERME` there, indistinguishable from a
register refusing to move. Arrival uses the same `DAMPER_POSITION_TOLERANCE` as
`IsMoving()`, so the word and the cell's amber in-transit colour turn over
together. A feedback that has become unusable, which the vaneless duct used to
signal by absence, raises `RECOPIE REGISTRE HS` in the hint bar, where it gets a
full line.

Rendering stays region-based: each frame is compared against the previous model
and only the regions whose contents changed are redrawn, each through its own
sprite. The fan animation repaints only its 30×30 disc, not the 320×72 device
row.

### Menu

Rotation moves the cursor, a click enters or edits, and each page ends with an
explicit `< Retour` — there is no long press. Booleans flip on a click. The
cursor stops at the ends rather than wrapping.

Root page, in order: Consignes, Sources, Programme, Mode ECO, Régulation,
Registres, Télémétrie, Système. Ordered by how often a page is reached and by
what depends on what — the setpoints first, then what is fitted to serve them,
then the programme that runs them, above ECO, which only shifts a setpoint the
programme has already been given.

Registres and Télémétrie are root entries rather than living under Système:
neither is a system setting. One describes the air path, which is the machine;
the other is where readings go. Filing them under a page named for the firmware
was filing them by who wrote them rather than by what they are. Système keeps
the clock and the factory reset.

Seven rows fit between the header and the hint bar, so the root page is now two
rows taller than the screen and scrolls — a deliberate trade for a flatter tree,
since the two pages that came up were each a click deeper than they were worth.
The scroll thumb on the right says so.

Two rows are two-way choices rather than numbers, operated exactly as booleans
are: `Sources > Type source` and `Programme > Programme`. Type source comes above
the toggle it governs, because that toggle means two different machines
depending on it and reading them the other way round is reading the answer
first.

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
is fitted. What is gated, and on what:

| Entry | Available when |
|---|---|
| Mode ECO, Date / Heure | an RTC answered at boot |
| Régulation > Seuil extract. | a dehumidifier is fitted |
| Programme > Init … Ouv. registre | the programme is Séchage |
| Programme > Programme | no session is running |
| Télémétrie > WiFi Grafana | this is a Pico W build |
| Registres > recycling rows | two registers are declared |

`Système > Télémétrie` carries the two uplink switches — the RS485 extension
port and, on a Pico W, the Grafana Cloud link — and four read-only rows
reporting the latter: network state and address, queue depth, writes and
failures. They print dashes rather than zeros when there is no radio to report
about: `0 sent, 0 failed` reads exactly like a link sitting perfectly idle. See
[DEVELOPMENT.md](DEVELOPMENT.md) for what the Pico W build needs.

`Système > Registres` holds everything about the air path the firmware cannot
measure for itself: how many registers the dryer has, which relay state means
extraction (`Sens commande`), which end of the feedback signal means open (one
flag for all of them — same actuator model everywhere), where each actuator's own
direction switch is set, and the two raw calibration marks per register.
`Sens commande` sits first because it is the only one of them that moves a vane —
the rest describe how a reading is read, and a dryer whose air goes the wrong way
is wired the other way round, not calibrated wrong. Two read-only rows show each channel's live raw value and
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
state the dryer is already in does nothing, and the start precondition — that
`Dryer::FaultReason()` reports nothing wrong — stays inside `Dryer::Start()`, so
every route into a session goes through the same door. A refused start is always
a start refused beside a blinking red LED and an on-screen reason; see
[Status LEDs](#status-leds).

## Status LEDs

Two LEDs on the panel, green and red, say what the machine is doing from across
the room:

Two axes on two lamps, read independently — **green is the session, red is the
fault** — rather than one lamp per state:

| | Green | Red |
|---|---|---|
| running | steady | — |
| cooling down | blinking | — |
| stopped | out | steady |
| *and* a fault | *unchanged* | **blinking** |

So a running dryer with something wrong shows **steady green and blinking red**,
which no other condition does. Red carries two meanings and the blink separates
them: steady red is a machine at rest, blinking red is a machine in fault.
Reading it needs no table — the operator looks at the green for motion and the
red for trouble, and the panel never has to choose between the two.

No state leaves both dark, so an unpowered board does not look like a dryer at
rest. The cooldown is the fan still turning after a stop, for
`FAN_COOLDOWN_DURATION_S` — a blinking green rather than the steady red of a
machine that has actually finished.

### What blinks red, and why

`Dryer::FaultReason()` returns the single worst thing wrong, ranked, as a
`DryerFault`. One reason rather than a set of flags, because everything
downstream wants exactly one: the LED blinks or it does not, the screen's alarm
band has room for one line, and `Start()` refuses with one message.

| `DryerFault` | Condition | Alarm band |
|---|---|---|
| `kAirflowBlocked` | both registers shut | `REGISTRES FERMES - PAS DE CIRCULATION` |
| `kDamperFeedback` | a declared register reports no usable opening | `RECOPIE REGISTRE HS - DEMARRAGE BLOQUE` |
| `kSensorStale` | inlet probe silent (`SENSOR_TIMEOUT_MS`) | `SONDE INJECTION HS - ARRET IMMINENT` |
| `kHydraulicOffline` | module not answering, source enabled in the menu | `HYDRAULIQUE INJOIGNABLE - VOIR SOURCES` |

**Every one of them refuses a start**, and the panel, the alarm band and the
interlock all read the same `FaultReason()`. That is the point of the single
enum: a button that will not take is always a button beside a blinking red LED
and a line on screen naming the cause. The operator is never left pressing a
dead switch.

### What a fault does to a session already running

Three of the four bring it down, via `FaultStopHoldoffMs()` in
`Dryer::UpdateFaultResponse()`. The dividing line is not severity — it is
whether the dryer can still be trusted to be doing what it says.

| `DryerFault` | Running session | Hold-off | Why |
|---|---|---|---|
| `kAirflowBlocked` | **stopped** | none | a fan against two shut vanes moves no air, and the electric heater sits in that duct |
| `kDamperFeedback` | **stopped** | none | the recopy is the only evidence the air path is open — see below |
| `kSensorStale` | **stopped** | 50 s, purging | the probe is the control input, and its threshold is a timeout rather than a measurement |
| `kHydraulicOffline` | continues | — | optional at runtime, degrades to electric-only |

A stop runs the normal `FAN_COOLDOWN_DURATION_S` purge, so the heater gives up
its heat before the fan quits.

### The probe hold-off, and the purge

Only the probe waits, and only because it is the one fault whose *detection* is
a timeout rather than a measurement. The other two arrive already confirmed —
`DAMPER_BLOCKED_CONFIRM_MS` of both registers reading shut,
`DAMPER_SIGNAL_CONFIRM_SAMPLES` below the signal floor — so by the time they are
reported there is nothing left to wait for, and waiting again would be the same
debounce served twice on a dryer that is provably unfit.

Core 1 polls the inlet every `SENSOR_UPDATE_INTERVAL` (2 s), so the two sensor
thresholds are really counted in missed polls:

| | Silence | Missed polls | What happens |
|---|---|---|---|
| `SENSOR_TIMEOUT_MS` | 10 s | 5 | heat cut, fault raised, **purge starts** |
| `SENSOR_SESSION_TIMEOUT_MS` | 60 s | 30 | session stopped |

Two thresholds because the two decisions cost very different things. Cutting the
heat is instantly reversible — the reading comes back and the heaters resume.
Ending a batch is not, and under the fault rules the dryer cannot even be
restarted until the probe answers again, so a ten-second bus hiccup must not be
in a position to destroy a night's drying.

The 50 s between them is not idle waiting. It is a **purge**:

- **heat off** — `TemperatureManager::AllOff()`, both sources;
- **extraction held open** — `HumidityManager::SetPurge(true)`;
- **phase transitions suspended** — Init, Brassage and Extraction all end on a
  humidity threshold, and that reading is stale by definition; letting them run
  would have a batch reach a finish it never actually got to.

Opening the extraction matters more than it first looks. With the probe dead
there is no temperature reading at all, so the `safety_max` cutoff is blind on
the same wire — venting is the one heat-removal action left that does not depend
on knowing the temperature.

`SetPurge()` is an override *above* the mode rather than a mode of its own,
because the mode belongs to the phase and the phase has not ended.
`HumidityManager::Update()` is a pure function of mode every cycle, so clearing
the purge restores whatever the phase wanted with nothing to save and nothing to
put back. If the session ends instead, the damper is left open: no `Update()`
runs during the fan cooldown, which is exactly where an open extraction was
wanted anyway.

`kDamperFeedback` is the one most easily argued out of that list, and the one
that most needs to be in it. `DamperFeedback::IsClosed()` reads NAN as *not
known to be shut* — deliberately, so the interlock only ever trips on a positive
reading. The consequence is that a dead recopy does not leave the dryer one
indicator short: it **silently disarms the airflow interlock**, which is the
thing that would have caught the failure. A dryer that cannot tell whether air
is moving must not keep heating on the assumption that it is.

The same asymmetry once produced a genuinely misleading log line. If the recopy
died while a blockage was already confirmed, `AirDamper::UpdateInterlock()` saw
`both_shut` go false and printed *"airflow restored"* — announcing recovery at
the instant the interlock went blind. It now distinguishes the two and says
`feedback lost — airflow can no longer be judged`.

`RestoreSession()` takes no preconditions, because a reboot mid-batch has to
pick the batch back up; one pass through `Dryer::Update()` brings it down again
if the machine it woke into is not fit to run.

The hydraulic module only counts when its source is enabled — a dryer fitted
without one would otherwise blink red for ever — and the way out of that fault
is the **Sources** menu toggle, which is what the alarm line says.

The two polled faults, `kSensorStale` and `kHydraulicOffline`, are held back for
`STATUS_FAULT_GRACE_MS` (15 s) after boot: neither the probe nor the module has
been asked yet, so nobody is late. The two air-path faults come off the ADC,
which reads from the first loop, and are not graced — fifteen seconds in which a
dryer with both registers shut would accept a start is fifteen seconds too many.

Each transition is logged once, with the cause:
`Status: running — fault: hydraulic module not answering`.

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

`SETTINGS_VERSION` is at **6**. v6 added the programme, the heat source type,
the dehumidifier's extraction threshold and the two telemetry switches. v5
before it dropped the four hydraulic regulation knobs, which stopped meaning
anything once the module took its own loop back, and put the air-renewal window
in their place. v4 dropped the LoRa telemetry interval along with the radio; the
field sat immediately before the checksum, so the record is shorter and no v3
file can be read as a v4 one. v3 replaced each register's named calibration ends
(closed, open) with ordered marks (min, max) plus an explicit signal direction,
and added the register count and the two actuator direction flags.

Either way the effect on a board being upgraded is the same and it is worth
stating plainly: **the stored calibration is discarded and has to be captured
again from the menu.**

A reboot mid-cycle resumes the session at its phase and elapsed time. Elapsed
time is `millis()`-based, so the wall-clock gap during the outage is lost.

---

## Open questions

Decisions the firmware has not made, kept here rather than in the code so that
reading a module does not mean reading someone's doubt about it. Each is a
question about behaviour, not a bug.

**Does a session ever end on its own?** `DRYING_SESSION_DURATION` has existed
since the first version and has never been read. The drying cycle loops
`Brassage -> Extraction` until someone presses STOP. Whether a dryer should be
able to finish unattended is a question about the food in it, not about the
firmware.

**Init exits on the raw setpoint, not the ECO-effective one.** Init ends when
the temperature reaches `GetTargetTemperature()`, while the loop underneath is
holding `GetEffectiveTargetTemperature()`. Under ECO the reduced setpoint is the
one being regulated to, the exit test never sees the full one, and Init runs its
whole hour. Either the test moves to the effective target, or warming up at the
full target is what Init is *for* — and then it should say so.

**Three things that would earn their place on the screen.** A diagnostics page
with bus state and error counters; a version report over the extension port; and
`HydraulicRemote::GetStatusBits()`, which is read every cycle and used nowhere.
That last one is the only thing that would let the screen show whether the
module is actually firing rather than merely whether it was cleared to.
