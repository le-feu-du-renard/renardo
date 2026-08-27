# Hardware — v4

Controller for the renard'o dryer, built on a **Raspberry Pi Pico H** (RP2040, no
WiFi). Everything that leaves the board leaves it over RS485, split across
**two buses**: one carries the inlet probe alone, the other carries the
hydraulic module and the extension port.

> The v3 board is gone: no more panel voltmeters, MCP23017 expander,
> potentiometers, mode selector, TM1637 display or SD card. Everything the
> operator *changes* goes through the rotary encoder and the menu; what they
> read is on the TFT, except the one thing that has to be readable from across
> the room, which is on two LEDs.

## GPIO map

25 of the 26 available GPIOs are used. **GP7 is the only one still free.**

The map is laid out **by side of the header**, because that is what the PCB
routes to. Each module's signals sit on consecutive header pins with the ground
they need already inside the block, so every peripheral takes one flat connector
with nothing to enjamb.

**Left side, header pins 1–20 — the front panel, the bus, and the display:**

| Header | GPIO | Function | Notes |
|---|---|---|---|
| 1 | 0 | STOP button | active LOW, internal pull-up |
| 2 | 1 | START button | active LOW, internal pull-up |
| 3 | GND | panel common | button commons **and** LED cathodes |
| 4 | 2 | Green status LED | active HIGH, 330 Ω to ground |
| 5 | 3 | Red status LED | active HIGH, 330 Ω to ground |
| 6 | 4 | RS485 "ext" TX | UART1 → MAX3485 DI |
| 7 | 5 | RS485 "ext" RX | UART1 ← MAX3485 RO |
| 8 | GND | MAX3485 ground | |
| 9 | 6 | RS485 "ext" DE/RE | HIGH = transmit |
| 10 | 7 | — | **free** |
| 11 | 8 | TFT CS | SPI1 RX pin, reused as an output |
| 12 | 9 | TFT DC | |
| 13 | GND | display ground | |
| 14 | 10 | SPI1 SCK | display only; module `SCL` |
| 15 | 11 | SPI1 MOSI | display only, MISO not wired; module `SDA` |
| 16 | 12 | TFT RST | module `RES` |
| 17 | 13 | Encoder A | EC11, internal pull-up |
| 18 | GND | encoder common | `C` **and** the switch return |
| 19 | 14 | Encoder B | EC11, internal pull-up |
| 20 | 15 | Encoder SW | EC11, internal pull-up |

**Right side, header pins 21–40 — everything that leaves for the plant:**

| Header | GPIO | Function | Notes |
|---|---|---|---|
| 21 | 16 | RS485 "probe" TX | UART0 → MAX3485 DI, second transceiver |
| 22 | 17 | RS485 "probe" RX | UART0 ← MAX3485 RO |
| 23 | GND | MAX3485 ground | shared with the two commands below |
| 24 | 18 | Fan command | BC337, contactor coil on the collector, **active HIGH** |
| 25 | 19 | Electric heating command | BC337, contactor coil on the collector, **active HIGH** |
| 26 | 20 | I2C0 SDA | optional RTC |
| 27 | 21 | I2C0 SCL | optional RTC |
| 28 | GND | RTC ground | |
| 29 | 22 | Damper command | BC337 driving the damper module, **active HIGH** |
| 31 | 26 | Extraction register feedback | ADC0 |
| 32 | 27 | Recycling register feedback | ADC1 |
| 33 | AGND | register feedback return | |
| 34 | 28 | RS485 "probe" DE/RE | HIGH = transmit; not adjacent to TX/RX, see below |
| 36 | 3V3 | supply | RTC, display and both MAX3485 |
| 40 | VBUS | 5 V | bench supply for the probes only |

Pin assignments live in [include/config.h](include/config.h). The TFT pins are
**duplicated** into the `TFT_eSPI` build flags in
[platformio.ini](platformio.ini) — change both together or the display will not
initialise.

### Why the probe bus's DE pin sits apart from its TX/RX

Every other peripheral on this header keeps its signals on consecutive pins,
ground included, so it takes one flat connector. The probe's RS485 bus is the
one exception: TX and RX land on GP16/GP17 (header 21–22, right next to a
ground at 23) because those are the only UART0 pins that cost nothing else —
freeing them only meant moving the fan and electric-heat commands one pin
over, to GP18/GP19, which have no constraint of their own to lose. But DE
needed a fourth free GPIO, and the only one left once GP18/GP19 were spoken
for is GP28 (header 34), well down the right side of the header, past the
RTC, the damper command and both register feedbacks.

The alternative was GP7 (header 10, the panel side, next to the "ext" bus's
own DE) — which would have kept a spare ADC channel but split the probe bus
itself across both sides of the header instead. Spending GP28 keeps the
probe's three signals within reach of one side of the board even though the
run to DE is a separate one from the run to TX/RX; the cost is ADC2, the last
previously-spare analog channel.

### What the one free pin is worth

GP7 is what is left: a bare GPIO on the panel side, no SPI, no I2C, no UART,
no ADC.

The extension port and the hydraulic module still cost nothing of their own:
each is a slave address on the "ext" segment, sharing the same transceiver, so
neither needs a pin the other does not already use. `Rs485Bus::kMaxBuses` is
2, and both slots are now spent — a third bus is not free without giving up
one of these two.

## The display owns SPI1 alone

Nothing else goes on this bus, whatever comes to the board later. `TFT_eSPI` on
the RP2040 may drive the panel through the **PIO** rather than the hardware SPI
block; a second device on the same pins would then be facing another master
entirely, and no amount of transaction bracketing would fix it. SPI0 is free and
costs two GPIOs — that is the answer if a peripheral ever needs SPI here.

The display is write-only, so SPI1 MISO is not wired at all (`TFT_MISO=-1`), and
the panel is driven at 40 MHz.

**SPI1 rather than SPI0 since the PCB.** The right-hand side of the header
exposes ten GPIOs and the plant needs all of them — two contactor commands, the
register command and its two ADC feedbacks, the RTC's I2C pair. The display was
the one peripheral whose pins are a free choice within a block, so it is the one
that moved. `TFT_SPI_PORT=1` in [platformio.ini](platformio.ini) is what selects
`spi1`; the library defaults it to 0.

### Never call `SPI.begin()`

That is arduino-pico's default SPI0 object, and its default pins are GP16, GP17,
GP18 and GP19 — **the probe RS485 bus and the fan and electric heating
commands, all four of them**. A call to it would silently take every one away
and reconfigure them as a bus.

Nothing does today, and the display will not do it by accident: `TFT_eSPI`
instantiates its own `SPIClassRP2040` on `spi1` and never touches the default
instance. This is a trap the pre-PCB pin map did not have, and it is worth
knowing before adding any SPI peripheral — use SPI0 by all means, but set its
pins explicitly elsewhere and leave GP16–GP19 alone.

## Command outputs

The three outputs are the same stage three times over: a **BC337**, a
small-signal NPN in common emitter, switching the low side. The fan and the
electric heating carry a 24 V contactor coil on the collector; the damper's
collector pulls down the register module's relay input. Note this is a bipolar
(800 mA, 45 V), not a MOSFET: it switches a coil or a control signal, never a
load directly.

- Base fed from the GPIO through **1 kΩ**, and **10 kΩ from base to emitter**.
- Emitter to the common ground.
- **A free-wheeling diode is mandatory on any coil** — and where it goes is not
  where the usual rule puts it, see below.
- **A common ground between the 24 V supply and the Pico is mandatory.**

The 10 kΩ across the base-emitter junction is not there to make the stage work.
An undriven RP2040 pad already sits low through its default pull-down, which is
what the polarity argument below rests on; the resistor makes that off state a
property of the **board** rather than of a register in the chip, so a pin left as
a plain `INPUT` with no pull, a leaky pad or a long base wire picking up noise
cannot bias the transistor on. It also speeds up turn-off, and it costs 90 µA out
of the 2.4 mA of base drive.

### Sizing the base resistor

The pad delivers 3.3 V and a saturated BC337 wants about 0.9 V of base-emitter
drop, so `Ib = (3.3 − 0.9) / Rb`. Saturation asks for a **forced beta of 20 or
less**, i.e. `Ib ≥ Ic / 20`:

| Rb | Ib | Ic at forced beta 20 | Smallest coil at 24 V |
|---|---|---|---|
| **1 kΩ** | 2.4 mA | 48 mA | 500 Ω |
| 680 Ω | 3.5 mA | 70 mA | 340 Ω |
| 470 Ω | 5.1 mA | 100 mA | 240 Ω |

**1 kΩ on all three** — the value the damper stage already uses, one line on the
bill of materials — and then let the measurement decide rather than the table.
The forced-beta-of-20 rule is deliberately pessimistic: a BC337-25 is specified
for hFE ≥ 160 at 100 mA and still saturates properly at a forced beta of 40. The
one judge is **Vce measured with the coil connected**. Below 0.4 V the 1 kΩ is
right; above it, drop to 680 Ω and measure again.

470 Ω is where this stops being comfortable. arduino-pico leaves the pads at
their **4 mA drive strength**, and a pad asked for 5 mA sags rather than
refusing, so the base current quietly ends up below the calculation. Holding the
level would take a `gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA)` in
`OutputDriver::Begin`. Needing it is the sign the coil has outgrown the stage.

**Measure the coil with an ohmmeter before choosing**: `Ic = 24 / R`. Check while
the meter is on it that the coil is a **DC** one — a 24 VAC coil cannot be
switched by an NPN at all, whatever the base resistor — and on a contactor with
an electronic coil it is the inrush, not the hold current, that has to fit.

**Past roughly 150 mA, stop using the BC337.** Not because the transistor cannot
carry it, it is good for 800 mA, but because the GPIO cannot supply the base
current that goes with it. A logic-level MOSFET is the right part there, and it
asks for no drive current at all.

### The free-wheeling diode, and where it goes

Without one, the collapsing field puts several hundred volts on the collector of
a part specified for 45 V. This is what kills the stage, and it kills it the
first time.

**Put it on the board, anode to the collector, cathode to the local +24 V** — not
only across the coil. The familiar rule of placing the diode as close to the coil
as possible is for a load on the same board. Here the contactor is two metres of
cable away, and a diode at that end leaves the cable's own inductance unclamped:
100 mA collapsing in a microsecond through 2 µH is still 200 V on the collector.
A diode between the collector and the +24 V node covers the coil **and** the
wiring in between.

If +24 V does not reach the board, fit one across the coil and a second one on
the board. A 1N4007 costs nothing and there is no trade-off to make here. With
the diode in place the collector never rises above about 24.7 V.

A diode fitted backwards does not hide: it shorts the 24 V rail as soon as the
supply comes up.

### The pinout is not the one the package suggests

**BC337 is E-B-C** — flat face towards you, left to right — the opposite of a
BC547's C-B-E, in an identical black TO-92. Fitted as a BC547 it destroys nothing
and switches nothing, which is the most expensive kind of fault to look for.

Read the pinout with a meter, not by eye, for the same reason as the resistor
colour bands below. On the diode range the base is the common anode of two
junctions; separating the emitter from the collector takes a component tester or
the meter's hFE socket.

### Polarity, and why all three are active HIGH

Until `pinMode()` runs — roughly two seconds after power-up — the GPIOs are
high-impedance inputs. The safe state has to be the one a **floating** GPIO
produces.

| Output | Wiring | Polarity | Floating GPIO |
|---|---|---|---|
| Fan | contactor coil between +24 V and the collector | active HIGH | base at 0 V → no collector current → **off** |
| Electric heating | contactor coil between +24 V and the collector | active HIGH | base at 0 V → no collector current → **off** |
| Damper | collector pulls down the register module's relay input, which is active LOW | active HIGH | base at 0 V → input released → relay off → **recirculation** |

All three land on active HIGH, for two different reasons. On the fan and the
heating the **stage does not invert the command**: the coil is the collector
load, so a base driven high saturates the transistor and energises it. The
inversion is at the collector *node*, and it only becomes an inversion of the
command when the collector drives something already pulled up and itself active
LOW — the damper module's relay input, which inverts a second time and arrives
back at the same polarity.

That direction is not a detail of taste. An inverting command on the fan or the
electric heating would energise them for the whole boot window, every reset,
with no firmware able to shorten it. Polarity is declared per output in
`config.h` (`OUT_*_ACTIVE_LOW`), not globally, so the wrong one is a one-line
mistake rather than a board-wide one.

The damper went from active LOW to active HIGH when its command moved onto the
BC337 module: the stage inverts, so the level that releases the relay is now the
low one — which is also the level an undriven RP2040 pad sits at, its pull-down
being enabled by default. `damper_test` reads that resting level back before it
drives anything, and `output_test` does the same for all three, which is the one
measurement that proves nothing travels or heats on every reset.

### The heating's safety cut-out does not pass through here

A BC337 that fails, fails **short** more often than not, and a shorted stage
holds the heating contactor closed with the firmware unable to do anything about
it. The mechanical over-temperature cut-out therefore belongs **in series with
the heating contactor's coil**, on a path that goes through neither the Pico nor
the transistor. Everything else in this document is a control path; that one is
not, and it is the only protection that survives this stage failing.

### Testing the outputs

`pio run -e output_test -t upload -t monitor` drives the three commands **one at
a time** through the production `OutputDriver`, so the polarity applied is the
firmware's own, and prints the GPIO level behind each logical state.

It starts with the measurement that matters most here: the resting level of
GP18, GP19 and GP22 read as plain inputs, **before `pinMode()` runs**, each
translated through its `OUT_*_ACTIVE_LOW` into on or off. All three must read
off. One that does not is a stage wired the other way round, and no amount of
firmware shortens the two seconds it spends energised on every reset.

The three commands moved off GP0–GP2 when the board was laid out, and the
argument above survives the move intact: GP18, GP19 and GP22 wake up exactly as
GP0–GP2 did, as inputs with the pull-down enabled. The RP2040 pins that behave
otherwise — GP23, GP24, GP25, GP29 — carry board functions on a Pico and never
reach the header, so a command cannot land on one by accident.

One output at a time is the point rather than a limitation: it is what lets a
collector be measured with nothing else moving, and on a dryer whose loads are
already wired it also keeps the electric heating from being held on with the fan
stopped. `m` lifts the restriction when that is genuinely what is being tested;
`f`, `d` and `e` drive one output, `0` drops everything, `a` walks the three at
5 s each, `h` prints the wiring.

The sequence, with the loads still disconnected:

1. **Transistor alone, no 24 V.** Collector through 1 kΩ to +3.3 V and nothing
   else. Driving the output must take the collector below 0.2 V and release it
   to 3.3 V. A collector that never moves is the E-B-C pinout fitted as C-B-E,
   or no base current at all.
2. **24 V, coil and diode, power load still off.** Measure **Vce with the output
   on**: this is what validates the base resistor, and only this. Below 0.4 V the
   1 kΩ stands.
3. **Reset, then a full power cycle, with the coils wired.** No contactor may
   close during the boot window. This is the test the whole polarity argument
   exists for.

## Air damper

**One or two** Belimo **LM24A-SR** registers, extraction and recycling, driven
purely on/off: recirculation or extraction, never a percentage. Travel takes
about 150 s each way. How many the dryer has is a **setting**, `Nb registres`
(Système → Registres), not a build option — see "Declaring the second register"
below.

With two, they are **complementary** — air is either extracted or recycled,
never both — so a single command drives both actuators, one of them travelling
the other way. That is why there is one `OUT_DAMPER_PIN` and not two.

They are also **asymmetric**: different vane geometry, different travel, so each
keeps its own two-point calibration and reports its own opening. Both 2-10 V
feedbacks go through their own divider to 0-3.3 V, on ADC0 and ADC1.

The readback used to feed the display and nothing else. It now also feeds one
safety decision, the **airflow interlock** below — the only reading in this
firmware allowed to stop the dryer.

A disconnected feedback wire is caught twice over: it reads near zero, below the
signal floor, and a calibration that was never captured leaves a degenerate span.
Either way the register reports "no position" rather than 0 %. One dead wire does
not mask the other register: they are evaluated independently.

### Four settings, and why each is a setting

Everything about the registers that the firmware cannot measure is on
Système → Registres:

| Setting | What it describes | Factory |
|---|---|---|
| `Nb registres` | how many registers the dryer has | 1 |
| `Sens signal` | which end of the 2-10 V output means open — **common to every register**, since they are the same actuator model | `Bas=ouvert` |
| `Sens extrac.` / `Sens recycl.` | where each actuator's own mechanical direction switch is set | `Normal` / `Inverse` |
| `Extrac./Recycl. mini`, `maxi` | the two raw ADC marks at the ends of that register's travel | 641 / 3179 |

The direction switch on each Belimo is the one the firmware cannot read and must
be told about: it decides which end of its travel a register goes to under the
single command. Get it wrong in the menu and travel detection chases the wrong
end, and the interlock misreads which reading means shut.

### The feedback runs backwards, and that is now a setting

Measured at the actuator, the extraction register puts out **10.10 V shut and
2.00 V open** — the opposite of the intuitive direction, and a property of the
linkage rather than a fault.

That used to be expressed by the *order* of the calibration pair: the "closed"
value was simply the higher one, and the arithmetic carried the negative span.
It worked, and it had one bad failure mode — entering the pair the wrong way
round was a perfectly valid calibration that reported every opening inside out
while looking entirely plausible.

The pair is now two **ordered marks**, `mini` and `maxi`, and a separate
`Sens signal` flag says which end is open. A descending pair is no longer a
backwards calibration, it is not a calibration at all: the span guard rejects it
and the register reports no position. The numbers to enter are the same ones you
always measured; only the question "which of these is open?" moved out of them.

### A first measurement that was wrong, and how it showed

An early bench run recorded 1392 and 3845 as the extraction register's end
stops. Both were wrong, in a way worth writing down because it is easy to
repeat: they came from a fixed 150 s hold rather than from the reading actually
going quiet. One end had arrived; the other was still creeping when the command
switched. Comparing a mid-travel sample against an end-stop voltage manufactures
an offset that does not exist, and a long investigation went looking for a ground
fault that was never there — the divider's foot measured 85 mV, which was fine.

What gave it away was arithmetic, not instinct. Against the measured 2.00 V and
10.10 V, those two raw values imply divider ratios of **0.561 and 0.307**, and a
divider has only one ratio. Whenever two calibration points disagree about the
ratio, at least one of them is not where it claims to be.

`damper_test` now decides arrival from the reading going quiet — 20 s inside a
±15 count band — and prints `SETTLED` when it does. A cycle that ends without
settling says so explicitly. **Never write down a value that has not settled.**

### Calibration, and the three false starts before it

The extraction register reads **3179 shut, 641 open**, both settled, on the
repaired wiring. Travel is 2538 counts, with 916 left before the ADC clips. In
today's menu that is `mini = 641`, `maxi = 3179`, `Sens signal = Bas=ouvert` —
enter 640 and 3180, the nearest values the menu's step of 10 can reach; one count
is 0.04 % of the travel.

What makes those numbers trustworthy is that both ends agree on **one ratio** —
0.2583 open, 0.2537 shut, against 0.2481 designed. That is a 1.8 % disagreement,
and it is the amount two 1 % resistors and an unregulated 3V3 pressed into
service as the ADC reference are entitled to. Fitted as a line rather than two
ratios: 313 counts per volt with a 14 count offset, against 308 predicted.

Three earlier attempts did not agree, and each failed differently:

1. **Readings taken on a moving vane.** A fixed 150 s hold is not proof of
   arrival. One end had settled, the other was still creeping, and comparing a
   mid-travel sample against an end-stop voltage manufactures an offset that
   does not exist — it sent a whole day after a ground fault that was never
   there. `damper_test` now decides arrival from the reading going quiet, 20 s
   inside a ±15 count band, and prints `SETTLED`. **Never write down a value
   that has not settled.**
2. **A wiring fault on the prototype**, which made the divider measure 0.280
   with both resistors confirmed correct.
3. **A full sweep reading 51..4095**, which is not a 2-10 V signal at all:
   below the actuator's own floor at one end, clipping at the other. A divider
   that is not dividing. The earlier published pair, 630 and 3104, dates from
   before this was found, which is why it is gone rather than kept as a
   footnote.

All three were caught by the same check, and it is the one to keep: **a divider
has exactly one ratio.** Whenever two calibration points disagree about it by
more than the components allow, at least one of them is not where it claims to
be. Software compensation is never the answer.

A note on the resistors, since they cost two rounds: both are 5-band and both
were first read from the wrong end. 3.3 k is orange-orange-black-brown-brown and
gives itself away reversed, because orange is not a tolerance colour. 10 k is
brown-black-black-red-brown and does **not** — reversed it decodes cleanly as
120 R, brown tolerance and all. Only a measurement rules that out. Read
resistance with an ohmmeter, not with your eyes.

### Declaring the second register

`Nb registres` is `1` out of the box, so GP27 is never sampled and the recycling
cell on the main screen reads `ABSENT`. An input with nothing on it is not a
harmless zero: it reads wandering noise that the screen would present as a live
opening, and it presents a high impedance to a multiplexed ADC. Unsampled,
`DamperFeedback` simply has no sample and reports no position.

**Set it to `2` only once that register's feedback is wired and calibrated.** On
a dryer declaring two registers, an unusable feedback on either of them **refuses
the start** — the airflow interlock cannot be evaluated without both readings,
and it is not a safety device that can be allowed to fail quietly. A feedback
that dies mid-session does *not* stop the session; it only refuses the next
start.

`damper_test` samples both channels regardless: it is the tool you use while
landing the wire, so it has to read a channel the firmware would not. Press `2`
there to drop it if the pin genuinely has nothing on it.

### The airflow interlock

Two registers can shut the air path completely, and a fan pushing against two
closed vanes moves nothing. When both read at or below `DAMPER_CLOSED_THRESHOLD`
(10 %) for `DAMPER_BLOCKED_CONFIRM_MS` (30 s), the firmware refuses a start —
whatever asked for it, since the guard is in `Dryer::Start()` rather than at the
button — and stops a running session.

Three properties are deliberate:

- **It only exists with two registers.** One register shut is a normal
  recirculation, not a fault.
- **It trips on a positive reading, never on a missing one.** A dead feedback
  reads near zero, which through this dryer's `Bas=ouvert` calibration would
  otherwise look exactly like a register at its shut stop. Missing readings block
  the *start* instead.
- **Nothing latches.** Fix the direction switch and the fault clears itself on
  the next sample. There is no acknowledgement step, because there is none
  anywhere else in this firmware either.

The confirmation delay costs nothing legitimate: complementary registers pass
each other mid-travel, one climbing while the other falls, and are never both
under 10 % at once. Two actuators whose direction switches are set the same way
round do settle there — which is precisely the fault this catches.

The way out, if a fault leaves both registers shut and the dryer will not start:
`Systeme → Registres → Vers extraction` drives the air path from the menu.
Nothing else commands the damper while the dryer is stopped.

### Testing the damper

`pio run -e damper_test -t upload -t monitor` switches the command every 30 s
and prints the raw ADC and the opening of **each** register beside it, driving
the production `OutputDriver`, `AirDamper` and `DamperFeedback` so the polarity
and the percentages are the firmware's own. It reports the command pin's resting
level before `pinMode()` runs, and flags per register a feedback that never
moved during a cycle.

The reading that matters is the pair: the two openings must **mirror** each
other, one climbing while the other falls. Both climbing together means an
actuator whose direction switch is set the same way round as its partner's
instead of the opposite way — a fault that leaves the screen plausible and the
air path wrong, and that the airflow interlock now stops the dryer on.

The cycle is 150 s, the actuator's own travel time, so each half ends with the
vane against a stop and the raw column resting on the value the calibration
wants. Press `s` for a 30 s cycle when only the relay and the wiring are in
question; `o`, `c` and `t` drive it by hand, `a` stops the automatic switching.

`1` and `2` drop a channel from the sampling. Use them while only one register
is wired: the RP2040 multiplexes one converter across the ADC channels and its
sample-and-hold carries charge between conversions, so an unwired pin — which
presents a very high impedance and never settles — can bias the reading of a
perfectly good neighbour sampled straight after it. The test probes both
channels at start-up and names any that sits on a rail.

## RS485 buses

**Two** MAX3485 transceivers, two independent Modbus RTU segments, 9600 8N1
each, each owned exclusively by Core 1. They do not share a wire and a fault
on one — a noisy cable, a slave stuck answering — cannot delay or corrupt a
transaction on the other.

| Bus | UART | Address | Device | Registers |
|---|---|---|---|---|
| "probe" | UART0 / `Serial1` | 1 | SHT30 probe, injection | FC03 `0x0000` %RH ×10, `0x0001` °C ×10 |
| "ext" | UART1 / `Serial2` | 2 | Extension module, optional | see below |
| "ext" | UART1 / `Serial2` | 10 | Hydraulic module | see below |

The extension module and the hydraulic module are addresses on the *same*
physical board (`dryer-extension`) today, which is why they share a segment:
splitting them would buy nothing since a fault in one is a fault in the other
anyway. The probe is a genuinely separate device, on the far side of the
plant, which is why it is not.

v4 carries **one probe**. The outlet one of v3 was polled and put on the air
every minute, and nothing downstream read it: the damper follows inlet humidity,
the heaters follow inlet temperature, and the screen has never shown it. It was
dropped rather than kept warm, and the address it left is what the extension port
now uses.

120 Ω termination at both ends of each segment.

### Wiring

The transceiver is a **MAX3485** — the 3.3 V part. The MAX485 of the same
outline is a 5 V part, and its `RO` would present 5 V to a GPIO that is not
5 V tolerant.

**Bus "ext" (UART1):**

| MAX3485 | Also marked | Pico GP | Header pin | Notes |
|---|---|---|---|---|
| DI (driver in) | `RXD` | GP4 | 6 | UART1 TX |
| RO (receiver out) | `TXD` | GP5 | 7 | UART1 RX |
| GND | | GND | 8 | |
| DE + RE | `EN` | GP6 | 9 | tied together, HIGH = transmit |
| VCC | | 3V3(OUT) | 36 | 3.3 V only |
| A / B | `D+` / `D−` | — | — | to the extension module's A / B, never crossed |

Header pins 6 to 9 are contiguous with the ground inside the block, so the
transceiver takes one flat connector. TX and RX could not move even if the
layout wanted them to: UART1 exists on GP4/GP8/GP12/GP20 for TX and
GP5/GP9/GP13/GP21 for RX, and of those only GP4/GP5 are still free once the
display has GP8–GP12 and the encoder GP13–GP15.

**Bus "probe" (UART0):**

| MAX3485 | Also marked | Pico GP | Header pin | Notes |
|---|---|---|---|---|
| DI (driver in) | `RXD` | GP16 | 21 | UART0 TX |
| RO (receiver out) | `TXD` | GP17 | 22 | UART0 RX |
| GND | | GND | 23 | shared with the fan/electric commands |
| DE + RE | `EN` | GP28 | 34 | tied together, HIGH = transmit — **not** adjacent to TX/RX, see "Why the probe bus's DE pin sits apart from its TX/RX" above |
| VCC | | 3V3(OUT) | 36 | 3.3 V only |
| A / B | `D+` / `D−` | — | — | to the probe's A / B, never crossed |

TX and RX land on GP16/GP17 because UART0 exists on GP0/GP12/GP16/GP28 for TX
and GP1/GP13/GP17/GP29 for RX, and GP16/GP17 is the only pair that cost
nothing else to take — see "What the one free pin is worth" above for why DE
had to go further down the header than TX/RX this time.

Two silkscreen conventions exist and they are opposites. A board marked
`DI`/`RO` names its pins from the transceiver's point of view; one marked
`RXD`/`TXD` names them from the microcontroller's, so its `RXD` **is** `DI` and
takes the Pico's TX. Wiring `TXD` to TX is the mistake this invites, and it
looks exactly like a dead bus.

A board that exposes a single `EN` instead of `DE` and `RE` has tied the two on
the PCB, which is what we want — unless it grounded `RE` and brought out only
`DE`, in which case the receiver never turns off and every transaction hears
itself. `rs485_test` says which within a second of booting.

`DE` and `RE` are tied because ModbusMaster reads the answer from the same
stream it wrote the request to: a receiver left enabled during transmission
feeds our own frame straight back, and the library takes it for the slave's
reply.

That fault reads as **`0xE3`, an invalid CRC** — not as the wrong-slave error
one would expect. The echo begins with the address and the function code that
were just sent, so both of ModbusMaster's identity checks pass; the next byte
is the high half of a register number where a byte count belongs, and the frame
falls apart there. A bus that is merely silent gives `0xE2` instead, so the two
are easy to tell apart once the mechanism is known — and impossible before.

The pair carries no polarity marking worth trusting — makers label the same two
wires A/B, D+/D−, and 485+/485− with no agreement on which is which. Reversing
them is the single most common fault, and it looks exactly like a dead probe.
`rs485_test` catches it before sending anything: see below.

On the bench, with no 24 V around, the probes run from **VBUS (header pin 40,
5 V)** — these SHT30 probes accept 5–30 V. Their 0 V and the Pico's ground must
be the same, as always.

### Bring-up

`pio run -e rs485_test -t upload -t monitor` drives the production `Rs485Bus`,
so a reading it prints is a reading the firmware would get. It adds what the
bus hides: the receiver's resting level, the raw bytes, and sweeps for a probe
whose address or baud rate is unknown.

It starts by sampling GP17 as a plain input, before the UART claims it. On an
idle pair with A above B the receiver output sits HIGH, which is the UART's
mark state; a **reversed pair holds it LOW**, a permanent break condition. That
one measurement separates a swapped pair from a dead probe — the two faults are
otherwise indistinguishable, since both simply time out. A pair with nothing
connected floats and reads as noise, so the check means something only once a
probe is on the wire.

Then it polls the inlet probe every two seconds and, when a read fails, names
the wire to look at rather than the error code:

| Error | Means |
|---|---|
| `0xE2` timeout | nobody answered: power, A/B, address, baud, or DE never rose |
| `0xE0` / `0xE1` | something answered under another identity |
| `0xE3` bad CRC | our own echo (RE not following DE), wrong baud, wrong parity, or noise |
| `0x02` | the probe is alive and addressed, but has no register at `0x0000` |

The echo is settled without any probe attached, at start-up and on `e`: the
test asks a question of address 247, which nothing on this bus owns, so any
byte that comes back is necessarily our own. Nothing else separates an echo
from a slave answering badly.

It is usually **not** a whole frame. `RE` left unconnected floats near its
threshold and the receiver flickers on and off through the transmission, so
what returns is some contiguous slice of the request — its first two bytes, its
last four. The test matches those slices too, which is why it recognises the
fault on a bus whose scan otherwise looks like a dozen half-alive slaves. A
real reply cannot be confused with one: it carries a byte count where the
request carries the high half of a register number, so the two diverge by the
third byte.

An echo is also a piece of good news. Those bytes travelled GP4 → `DI` → the
pair → `RO` → GP5 and came back intact, which proves in one shot that TX and RX
are on the right pins, that the transceiver is powered, and that `DE` rises —
the whole local chain, everything except the probe.

The other keys: `s` sweeps addresses 1–32, `b` sweeps the common baud rates,
`p` sweeps parity and stop bits, `m` walks the first sixteen registers to
recover an unknown map, `l` listens to the pair without transmitting. The
sweeps count only frames whose CRC holds — at the wrong baud rate bytes still
arrive, they simply mean nothing. They build their own frames instead of using
ModbusMaster, whose response timeout is a fixed two seconds: that alone would
make a 32-address sweep take a minute.

`p` exists because a probe set to even parity answers every single request and
parses as pure noise: the parity bit is read as the first stop bit and every
byte lands shifted. `Rs485Bus::Begin` opens the port as 8N1 and the whole bus
must agree, so a probe found on another framing gets reconfigured rather than
accommodated.

Breathing on the probe is the last check: humidity must climb within seconds. A
plausible but frozen value means the register is being read from a cache, or
that the wrong register is being read.

### Hydraulic module (to be built)

Deported over RS485. It owns the circulator **and its own interlock** — the
module decides when circulating is worth doing and does not wait to be told.
The three-way valve is not on this bus at all: it keeps its own probe and its
own dial and regulates itself, which is what keeps two controllers off one
valve.

Two blocks, both driven from here because a slave never speaks unprompted: the
dryer writes the command block with one FC16 and reads the telemetry block with
one FC03, every poll cycle.

| Register | Direction | Contents |
|---|---|---|
| `0x0000` | write | run permission, 0 = stand down, 1 = cleared to run |
| `0x0001` | write | water setpoint ×10 (°C) |
| `0x0002` | write | dryer inlet air temperature ×10, signed |
| `0x0010` | read | circulating water temperature ×10, signed |
| `0x0011` | read | storage tank temperature ×10, signed |
| `0x0012` | read | status bits |
| `0x0013` | read | commanded circulator speed, whole percent |

Readings are **signed tenths in an unsigned register**: cast to `int16_t` before
dividing, because the water loop legitimately reads below zero. A value the
module does not have is `INT16_MIN`, never a zero — and the speed, whose 0 and
100 are both legal, uses `0xFFFF` for the same purpose.

`0x0002` is what lets the module tell whether circulating would move heat *into*
the dryer rather than out of it. While it reads as the sentinel the module falls
back to the water setpoint alone, which is correct but more cautious.

Status bits:

| Bit | Meaning |
|---|---|
| 0 | circulating |
| 1 | permission, as received and echoed back |
| 2 | tank too cold — the local interlock is holding the pump off |
| 3 | water probe fault |
| 4 | tank probe fault |
| 5 | *reserved, unused* |
| 6 | watchdog tripped — bus silent, everything shut down |
| 7 | setpoint missed — regulating, but not reaching the target |

Bit 2 is the one worth understanding from this end: the module has accepted the
permission and is deliberately not circulating, because the tank is not warm
enough for circulating to move heat in the useful direction. That is normal
operation, not a fault.

Bit 5 was a fake-probe fault, from when the module synthesised a probe for the
valve. It is retired, and the bits above it were **not** renumbered to close the
gap — this table is read off a bench with a status word in front of it, and
shifting them would have gained one bit out of eight spare and made the paper
wrong.

`0x0000` is a **permission, not a command**, and this is the part the module
firmware has to be written against: the dryer raises it once when a session
starts with the hydraulic enabled and holds it for the whole session, across
every phase transition, dropping it only when the session ends or an interlock
fails. It is not a request to fire now. A module that treats each `1` as a start
pulse, or that expects the dryer to cycle it, will do nothing for hours.

Permission and setpoint are written in **one FC16 transaction**, so the module
never sees the permission raised paired with a stale setpoint.

**The module must implement its own watchdog** and shut down if it receives no
frame for 60 s. This is load-bearing rather than belt-and-braces: the dryer no
longer cycles the module, so if the bus dies with the permission raised, that
watchdog is the only thing left that can stop it. The dryer marks it unavailable
after 30 s of silence and falls back to electric-only.

## Extension port

One connector for optional modules — data logger, energy metering, a WiFi or
LoRa gateway, a deported panel. The dryer gains a feature by gaining a module,
and **none of them is ever load-bearing for regulation**. That is the difference
with the hydraulic module above, which carries a heat source: nothing on the
extension port can stop a session or change what the control loop does beyond
the setpoint it is allowed to move.

The extension is **another slave on the same RS485 segment**, not a connector of
its own. Modbus RTU allows one master per segment and the dryer is it, so nothing
here needs a transceiver, a UART or a GPIO beyond what the bus already has.
Wiring an extension module means landing it on the same A/B pair as the probe and
the hydraulic module, on **address 2** — vacant since the outlet probe was
dropped.

A slave never speaks unprompted, so both directions are driven by the dryer. Once
per poll cycle it writes the telemetry block with one FC16, then reads the
command mailbox with one FC03. **A command therefore waits at most one cycle,
2 s.** The wire format is in [`include/ExtensionProtocol.h`](include/ExtensionProtocol.h),
which the module firmware compiles too, and the addresses in `config.h`
(`EXT_REG_*`).

### Telemetry — the dryer writes, one FC16 per poll cycle from `0x0000`

The block is a **generic `{metric_id, value}` table**, not a fixed set of
named registers — the register layout below never changes as metrics are
added or removed. Six header registers, then two registers per metric, as
many metrics as the dryer has to report (up to 24):

| Register | Contents |
|---|---|
| `0x0000` | protocol version, currently 4 |
| `0x0001` | metric count — how many `{id, value}` pairs follow |
| `0x0002`–`0x0003` | uptime in seconds, high word then low |
| `0x0004` | acknowledged sequence — echo of the last command acted on |
| `0x0005` | result — 0 ok, 1 unknown opcode, 2 refused, 3 out of range, 4 version |
| `0x0006`, `0x0007` | first metric: id, then value |
| `0x0008`, `0x0009` | second metric: id, then value |
| … | one `{id, value}` pair per remaining metric |

Each metric id's **bit 15 selects its value's encoding** (`ExtensionProtocol.h`,
`kExtMetricKindMask`); the low 15 bits are the id itself. Bit 15 clear means
**signed tenths** (cast the value to `int16_t` before dividing by 10 — the
water loop legitimately reads below zero); a reading the dryer does not have
is `INT16_MIN` (`0x8000`), never a zero. Bit 15 set means a **raw whole-unit
counter**, no sentinel, wrapping at 65536 — used only for the session-elapsed
metric, whose seconds can exceed the tenths encoding's range.

What each id means is never spelled out on the wire. Both the dryer and the
module firmware compile the same
[`include/TelemetryMetricIds.h`](include/TelemetryMetricIds.h) — mirrored
byte-identically the same way `ExtensionProtocol.h` and `HydraulicProtocol.h`
are, checked by `scripts/check_shared_headers.sh` in the module repository —
so an id like `0` always means `inlet_temp` on both ends without ever
crossing the bus. The current catalog:

| id | name |
|---|---|
| 0 | `inlet_temp` |
| 1 | `inlet_humidity` |
| 2 | `water_temp` |
| 3 | `tank_temperature` |
| 4 | `target_temp` |
| 5 | `target_humidity` |
| 6 | `extraction_pos` |
| 7 | `recycling_pos` |
| 8 | `phase` — 0 stop, 1 init, 2 brassage, 3 extraction |
| 9 | `session_elapsed` — whole seconds, counter-encoded |
| 10 | `running` |
| 11 | `fan_on` |
| 12 | `electric_on` |
| 13 | `hydraulic_demand` — cleared to run, the permission, not a measured circulator |
| 14 | `hydraulic_online` |
| 15 | `damper_open` |
| 16 | `sensor_fault` — the inlet probe is stale and heating is inhibited |
| 17 | `airflow_fault` — every register reads shut |
| 18 | `feedback_fault` — a register's position readback is unusable |
| `0x7FFF` (reserved, not in `TelemetryMetricIds.h`) | `uptime_s` — the generic engine's own id, not a dryer metric |

Booleans and the phase number ride the tenths encoding as `0.0`/`1.0`/etc.
Openings (`extraction_pos`, `recycling_pos`) are whole percent, `NAN` (the
tenths sentinel) for no usable feedback — 0 % is a shut register and 100 % a
legal reading, so absence has to be a distinct value rather than either end
of the range.

### Commands — the dryer reads, one FC03 of 4 registers from `0x0040`

| Register | Contents |
|---|---|
| `0x0040` | sequence — increment for each new command; 0 means the mailbox is empty |
| `0x0041` | opcode |
| `0x0042` | argument, signed tenths |
| `0x0043` | protocol version the module speaks |

| Opcode | Command |
|---|---|
| 0 | none |
| 1 | start — **reserved and always refused** |
| 2 | stop |
| 3 | set temperature setpoint |
| 4 | set humidity setpoint |

**Starting a session is not available over the port.** The opcode exists so a
module author gets `2` (refused) rather than silence, but no remote can launch a
dryer nobody is standing in front of. Setpoints are checked against the same
limits the menu enforces and **refused rather than clamped**, so a module is
never told a value took when a different one did.

Write the sequence **last**, after the opcode and argument, or the dryer may read
a new sequence paired with the previous command's argument.

The dryer executes one command per sequence value and answers in `0x000F` /
`0x0010`. Repeat the command until the echoed sequence matches, then stop: the
mailbox is re-read every cycle, and the dryer's replay filter is what keeps a
command sitting there from firing again and again.

The module needs **no watchdog of its own** — unlike the hydraulic module above,
nothing here can leave a load energised. The dryer marks the port unavailable
after 30 s of silence and simply stops reporting to it. An absent module is
retried once every 30 s rather than every cycle, so leaving the port empty costs
the poll loop almost nothing, and a module plugged in later is picked up without
a reflash.

### Bring-up

`pio run -e extension_test -t upload -t monitor`, on a **second Pico** with its
own MAX3485, landed on the same A/B pair. Despite the name it plays **both**
remote modules — the extension at @2 and the hydraulic module at @10 — because
with nothing answering @10 the extension's telemetry shows dashes for both water
figures and carries the hydraulic-offline flag forever, so half of what the port
reports could never be seen. It decodes with the production `ExtensionProtocol`,
so building it at all is the check that the header really is compilable by a
module.

A telemetry block should land every two seconds and print decoded. Then, from the
console:

- `s` posts a STOP, `t 38.5` a temperature setpoint, `h 55` a humidity one. The
  acknowledgement comes back inside the next telemetry block, named.
- `r`, `o`, `x` and `v` post the four refusals — START, out of range, unknown
  opcode, wrong version. Each should come back with its own result code rather
  than being silently swallowed or silently obeyed.
- `w 48` and `k 55` set what the hydraulic module reports for water and tank;
  they should appear in the extension's telemetry a cycle later, and on the
  screen. `g` shows what the dryer is asking of the circulator.
- `e` and `u` take either module off the bus without unplugging it. That is how
  the 30 s availability timeout and the backoff are exercised: silence at @10
  should raise hydraulic-offline and fall back to electric-only, and silence at
  @2 should have the dryer retry once every 30 s rather than every cycle.
- `n` prints the bus counters, and is the first thing to press when nothing
  arrives: it separates "no traffic at all" from "traffic for other nodes but
  none for us" from "the write lands and the read never comes", which are three
  different faults.

**CRC errors are counted only on frames addressed to this board.** It hears the
whole pair, and a master request is separated from the slave's answer by a
turnaround routinely shorter than the 3.5 character times that end a frame — so
the dryer's exchange with the probe arrives glued into one buffer whose CRC
cannot check out. A passive listener cannot tell a request from the reply behind
it, and the only traffic whose integrity it can judge is its own.

Both boards must **share a ground**: RS485 is differential, not isolated. 120 Ω
at the two ends of the segment and nowhere in between — a third resistor in the
middle is the same fault as none at all.

Leaving an accepted command sitting in the mailbox is deliberate, and is the
check on the replay filter: the dryer re-reads it every cycle and must act on it
exactly once.

## Optional RTC

DS1307 on **I2C0**, address 0x68. Probed at startup.

| Module | Pico GP | Header pin |
|---|---|---|
| SDA | GP20 | 26 |
| SCL | GP21 | 27 |
| GND | GND | 28 |
| VCC | 3V3(OUT) | 36 |

Three of the module's four pins are consecutive, ground included. That pair is
not a free choice: the RP2040's I2C function repeats modulo 4 across the GPIOs
(0 → I2C0 SDA, 1 → I2C0 SCL, 2 → I2C1 SDA, 3 → I2C1 SCL), and GP20/GP21 is the
only **adjacent** I2C0 pair left on this side of the header once GP26/GP27 are
spoken for by the register feedbacks. `Wire` validates both pins against the
same table and would refuse the bus outright if either were wrong.

It runs at 10 kHz with a 1 s timeout, which is generous for a DS1307 and leaves
a long field cable no excuse.

**With no RTC there is no ECO mode**: the night window cannot be evaluated
without a wall clock, so the whole ECO submenu is greyed out and the mode is
forced to PERFORMANCE whatever is stored in the settings.

The clock is set from the menu, `Système > Date / Heure` — no reflash needed,
and reflashing no longer resets it. Firmware seeds the chip from its build time
only when the RTC has never been set or its backup cell is dead, so a stopped
clock coming back at the build date means the battery needs replacing.

## Display

**GMT020-02-7P v1.3**, ST7789, 240×320, used in **landscape (320×240)** via
`setRotation(3)` — landscape flipped 180°, so the panel reads the right way up
in its enclosure.

The 7-pin connector carries no backlight control; the backlight is permanently
on — so a lit backlight proves the supply, and nothing else.

### Wiring

The module's `SCL` and `SDA` are **SPI**, not I2C, despite the silkscreen.

Listed in the order the pins appear on the module, which is **not** the order
they appear on the Pico header — wiring positionally rather than by name is the
easy mistake here.

| Module | Pico GP | Header pin |
|---|---|---|
| CS | GP8 | 11 |
| DC | GP9 | 12 |
| GND | GND | 13 |
| SCL (clock) | GP10 | 14 |
| SDA (data) | GP11 | 15 |
| RES | GP12 | 16 |
| VCC | 3V3(OUT) | 36 |

3.3 V only. The six signals land on header pins 11–16, with GND at 13 — one flat
connector for the panel. Better still, `SCL`, `SDA` and `RES` sit on 14-15-16 in
the module's own silkscreen order, so three of the five wires run straight
across; only `CS` and `DC` cross over.

CS sits on GP8 and RES on GP12, which are both SPI1's RX pin. That is harmless:
the panel never drives data back, TFT_eSPI passes MISO as `-1` to its SPI
object, so RX is never enabled and the two stay plain outputs.

TFT_eSPI on the RP2040 does not call `spi_init()` or `gpio_set_function()`: it
constructs its own `SPIClassRP2040` from `TFT_SCLK` and `TFT_MOSI` and calls
plain `spi.begin()`. Those two are constrained by the silicon — SPI1 offers SCK
on GP10/GP14/GP26 and MOSI on GP11/GP15/GP27 — and `SPIClassRP2040` validates
them against exactly that list. A wrong number there fails at **run time**, with
a blank panel and no build error, so check it against
[include/config.h](include/config.h) rather than against a memory of it.

### Clock polarity

**This panel needs `TFT_SPI_MODE=SPI_MODE0`** — the clock idling low.

TFT_eSPI defaults ST7789 to `SPI_MODE3`, with the comment "some ST7789 boards do
not work with Mode 0". This module is the converse: on the library default it
shows nothing whatsoever, not even corruption, and does not answer a readback
either. It is the single setting that stood between a blank panel and a working
one, and nothing about the symptom points at it — a wiring fault looks exactly
the same.

### Colour order

**This panel needs `TFT_RGB_ORDER=TFT_BGR`.**

On the library default red rendered as blue and cyan as yellow, with green
untouched — the signature of the red and blue channels being exchanged. The
start-up splash is plain text and no longer shows this; to check the channel
order, run `pio run -e tft_test -t upload -t monitor`, which sweeps colour
patterns.

### When nothing appears

`TFT_eSPI::init()` writes its sequence blind and never reads back, so the log
line only reports that the sequence was *sent*. The start-up splash — the
"SECHOIR PAYSAN" banner, held on screen for the whole of setup with the stage in
progress on its bottom line — is the only real evidence the panel is alive. A
stage name still on screen minutes later names the step that hung.

Nothing on screen with the backlight lit means the panel is not receiving, not
leaving reset, or being clocked on the wrong edge. Check, in this order: the
clock polarity above, then **DC**, **RES**, **CS**, then SCL/SDA not swapped.

Two bring-up environments help:

`pio run -e pin_test -t upload -t monitor` drives each of the five signals on
its own at 1 Hz, announcing which one, so every wire can be confirmed with a
multimeter or an LED. It uses no libraries — it proves the wire, not the
driver. Probe at the **module** end: that is what distinguishes a broken wire
from a wrong pin.

`pio run -e tft_test -t upload -t monitor` asks the panel to identify itself.
The module exposes no MISO pin, but the ST7789 answers on the SDA line, which
is bidirectional in 4-wire SPI, so `TFT_SDA_READ` turns the pin around for the
read. An ST7789V normally reports `0x85 0x85 0x52`. A plausible ID means the
panel is powered, out of reset and wired correctly in both directions, so
anything still wrong is driver configuration; all zeroes or all ones means
nothing is answering and the fault is power, reset, CS or the clock/data pair.
Some modules put a series resistor on SDA and cannot be read at all, so a silent
answer is suggestive rather than conclusive. The test then sweeps colours and
draws corner markers, which also reveal orientation and any row/column offset.

## Rotary encoder

**EC11**, quadrature plus push switch. Since v4 it is the only way to change a
setpoint: the panel keeps two buttons, START and STOP, and nothing else —
everything else lives in the menu.

### Wiring

EC11 boards are silkscreened in at least three ways for the same three signals.
Wire by function, not by position:

| Signal | Also labelled | Pico GP | Header pin |
|---|---|---|---|
| A | S1, CLK | GP13 | 17 |
| GND | C, common | GND | 18 |
| B | S2, DT | GP14 | 19 |
| SW | Key | GP15 | 20 |
| + | VCC, breakouts only | 3V3(OUT) | 36 |

On the bare component, ground is the **middle** pin of the three-pin side plus
the second pin of the switch; a breakout routes both to its `GND` pad, so one
wire does it.

A and B are interchangeable — swapping them only reverses the direction. Which
pad a maker calls "A" is not standardised, so a knob that counts down when
turned clockwise is a property of the part, not a wiring fault: set
`ENCODER_REVERSED` in [include/config.h](include/config.h) rather than crossing
the two wires, so the pin map keeps matching the silkscreen. `encoder_test`
applies the same flag and prints its state, so the test and the firmware always
agree on which way is up.

All three lines are `INPUT_PULLUP` and read active LOW, so a bare EC11 needs no
external resistor and no supply at all; a breakout's own pull-ups simply sit in
parallel with the internal ones. **3.3 V only** — GP13, GP14 and GP15 are not
5 V tolerant.

The three signals plus a ground land on **four consecutive header pins, 17 to
20**, in the EC11's own terminal order: A, common, B, SW. The ground the
connector needs is already inside the block, and the switch returns to that same
ground, so one flat four-way connector does the whole part with nothing to
enjamb.

### Decoding

An EC11 emits one full Gray-code cycle per detent, so
`QuadratureDecoder::kCountsPerDetent` is 4. Sampling happens in a pin-change
interrupt on both lines, so no rotation is lost while the display is being
redrawn, and impossible transitions — both lines appearing to move at once,
which is what bounce looks like — are dropped rather than counted.

### Bring-up

`pio run -e encoder_test -t upload -t monitor` reports every edge and every
detent, using the production decoder, plus the raw edge counts the firmware
hides. Four checks, in order:

1. **At rest, silent.** A detent reported with nobody touching the knob means a
   line is floating — the common is not actually on GND.
2. **One detent clockwise → `CW`, position +1, 4 edges.** `CCW` means A and B
   are swapped; fix it at the connector rather than in `config.h`, so the pin
   map keeps matching the silkscreen.
3. **Ten detents each way → position back to 0.** A drift means detents are
   being dropped or doubled; the summary's *edges per detent* says which. Well
   above 4.00 with a rising bounce count is a noisy encoder — 10 nF from A to
   GND and from B to GND fixes it.
4. **One press → exactly one `CLICK`.** Two per press is switch bounce; the
   30 ms debounce in `RotaryEncoder` covers a normal EC11.

## Panel buttons and status LEDs

Four signals on one contiguous block of header pins, 1 to 5, with the ground in
the middle — one flat connector for the whole panel, the same reason the encoder
takes header pins 17 to 20:

| Header | GPIO | Signal |
|---|---|---|
| 1 | GP0 | STOP |
| 2 | GP1 | START |
| 3 | GND | both button commons **and** both LED cathodes |
| 4 | GP2 | green LED anode, 330 Ω to ground |
| 5 | GP3 | red LED anode, 330 Ω to ground |

GP0 and GP1 are UART0's *default* pins, but nothing claims them there: the logs
go out over USB CDC, the "ext" RS485 bus is on UART1, and the "probe" bus,
though it is on UART0, has its TX/RX set explicitly to GP16/GP17 rather than
left at the default. Putting the buttons on GP0/GP1 does spend the one place a
rescue serial console could have been landed — the price of having the panel
connector at the end of the header the panel loom arrives at.

A connector fitted one row out therefore puts all four signals on the wrong pin
at once, which is loud rather than subtle: `panel_test` prints the resting level
of each pin under its own name.

### Two buttons, not one

v4 shipped with a single button toggling the session. A toggle answers the wrong
question in front of the machine: the operator reaching for it wants to *stop*,
and has to know what the dryer is currently doing to predict what the press will
do. Two dedicated buttons remove that inference, and neither can be the other by
mistake.

STOP takes GP0 and START GP1 — the other way round from v4 — because that is the
order the panel loom arrives in. Which end of the loom carries which button is a
property of the panel, not a wiring fault, so it is fixed in
[include/config.h](include/config.h).

Both are dry contacts to ground with no external resistor: the RP2040's internal
pull-up is the only pull there is, as on the encoder. Both are debounced 50 ms
by `PushButton` and fire once per press, however long they are held.

STOP is read first every pass, so pressing both within one debounce window ends
with the dryer stopped. `Dryer::Start()` and `Dryer::Stop()` each return
immediately when the dryer is already in the state being asked for, so a press
that changes nothing costs nothing — and the start precondition stays where it
was, inside `Dryer::Start()`, whatever route asks for a session: no session
begins while `Dryer::FaultReason()` reports anything wrong, which is the same
condition the red LED beside the button is blinking for.

### Two LEDs, not one RGB

Two discrete LEDs say more than an RGB part would on one GPIO less, because they
are two independent lines and can therefore carry **two axes at once** — green
for the session, red for the fault:

| | Green | Red |
|---|---|---|
| running | steady | — |
| cooling down (`FAN_COOLDOWN_DURATION_S`) | blinking | — |
| stopped | out | steady |
| *and* a fault | *unchanged* | **blinking** |

A running dryer with something wrong therefore shows **steady green and blinking
red**, which no other condition does. Red means two things and the blink is what
separates them: steady is a machine at rest, blinking is a machine in fault. The
operator reads the green for motion and the red for trouble, and never has to
work out which of the two the panel decided to show — the earlier design folded
both into one lamp, and a fault silently swallowed the running indication.

No state leaves both LEDs dark, so an unpowered board or a dead LED does not
look like a dryer sitting quietly at rest. The blink is 500 ms on, 500 ms off —
the same half period as the screen's own blink, so the panel LED and the
cooldown icon beat together.

Four conditions blink the red, ranked worst first by `Dryer::FaultReason()`:
**no airflow** (both registers shut), **an unusable register feedback**, **a
silent inlet probe** (`SENSOR_TIMEOUT_MS`), and **a hydraulic module that
stopped answering** — the last only when the hydraulic source is enabled in the
menu, since a dryer fitted without one would otherwise blink red for ever.

Every one of them also refuses a start, and the screen's alarm band names the
same ranked reason, so a blinking red LED, a button that will not take and a
line of text on screen are always the same fault seen three ways. The two polled
faults are held back for `STATUS_FAULT_GRACE_MS` after boot, where neither the
probe nor the module has been asked yet; the two air-path faults read off the
ADC from the first loop and are not.

All but the hydraulic one also **stop a session already running** — the module
is optional at runtime and losing it degrades to electric-only, whereas a shut
air path, a dead register recopy and a silent inlet probe each leave the dryer
unable to vouch for what it is doing. A dead recopy in particular does not cost
one indicator: it disarms the airflow interlock, which reads no signal as
not-shut.

The probe is the only one given a hold-off before the batch ends: 5 missed polls
cut the heat, 30 end the session, and the 50 s between them is spent purging —
heat off, extraction held open, phase transitions suspended. With the probe dead
the `safety_max` cutoff is blind on the same wire, so venting is the only heat
removal left that does not depend on knowing the temperature. See
[Status LEDs](DOCUMENTATION.md#status-leds) for the whole table.

Nothing is reported for the first 15 s after boot (`STATUS_FAULT_GRACE_MS`).
The probe and the hydraulic module are both silent until Core 1 has completed
its first RS485 cycle, so every freshly booted dryer is in fault by the letter
of the test — that is a boot, not a fault.

### Sizing the series resistors

Straight off the GPIO, one resistor per LED, cathode to ground. At 3.3 V:

| LED | Vf | R | I |
|---|---|---|---|
| red | ~1.9 V | 330 Ω | ~4.2 mA |
| green (standard) | ~2.1 V | 330 Ω | ~3.6 mA |

One value for both keeps the bill of materials short; drop the green to 220 Ω
(~5.5 mA) if it looks pale in daylight. That is inside the RP2040 pad's default
4 mA drive strength and well under its 12 mA absolute maximum.

**Use a standard green** — GaP or AlInGaP, 2.0–2.2 V. A high-brightness InGaN
green drops 3.0–3.2 V, which leaves 0.1–0.3 V across the resistor: the current
would then be set by the part's forward-voltage tolerance rather than by
anything on the board, and two LEDs from the same reel would not match.

Panel-mount 12 V or 24 V indicators are a different fitting altogether: they
cannot be driven from a pad and would need a BC337 stage like the three command
outputs. Decide that before buying the indicators, not after.

### Bring-up

`pio run -e panel_test -t upload -t monitor` reads all four pins before it
drives anything, then names each button as it is pressed and walks the LEDs
through the four states, 4 s each. Four checks:

1. **Both LEDs dark at boot**, before `pinMode` runs. A LED lit here is wired to
   3.3 V rather than to its GPIO, and is on for the whole boot window of every
   reset.
2. **Both buttons read released** with nobody touching them. One reading LOW is
   a stuck contact — or that signal shorted to the ground at header 3, which is
   what a connector one row out does.
3. **One press, one line, under the right name.** Two lines per press is bounce;
   the wrong name is two swapped signal wires.
4. **Press `b` and look at the panel in daylight.** Both LEDs on: if the green
   is the one that disappears, it is the wrong green or the wrong resistor.

## Power supply

| Rail | Use |
|---|---|
| 24 V | contactors, Belimo actuator, RS485 modules |
| 5 V | Pico (VSYS), TFT |
| 3.3 V | MAX3485, RTC |

The 3.3 V rail now carries two small, steady loads and no transmitter: the
120 mA emission peak that used to set its size went with the radio.

## Bring-up order

1. Pico alone, USB serial: check the boot log.
2. TFT: mire, fonts, icons. Confirm orientation and colours.
3. Encoder: detents and click, no phantom steps — `encoder_test` first, then
   the menu itself.
   Panel: `panel_test` — both LEDs dark before `pinMode`, both buttons released
   at rest, one line per press under the right name, and the four LED states
   legible in daylight.
4. Outputs: `output_test`, one at a time, **measuring at the connector before
   wiring the loads** — this is where a polarity mistake is caught, and where
   the resting level of the three command pins is read back before anything
   drives them.
5. Registers: `damper_test` first — it checks the resting level, the relay and
   both feedbacks in one pass, and says whether the two openings mirror each
   other — then a full travel on the `l` cycle to record each register's two
   end-stop values, and calibrate both from the menu.
6. RS485: `rs485_test` first, then the probes in the firmware, then the
   hydraulic module, then `extension_test` on a second Pico. This is the last
   step — everything the dryer talks to now lands on this bus.
