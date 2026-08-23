#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== PINS CONFIGURATION ==========

// ----- SPI1, the display alone -----
// Write-only, so no MISO is wired at all. Nothing else is allowed onto this bus:
// TFT_eSPI on the RP2040 may drive the panel through the PIO rather than the
// hardware SPI block, and a second master on the same pins would be fighting it
// with no amount of transaction bracketing to help.
//
// SPI1 rather than SPI0 since the PCB layout: the whole right-hand side of the
// header (GP16 upwards) is now field wiring — contactors, registers, RTC — and
// ten pins there do not stretch to the display as well. The bus moved rather
// than the process I/O, because the display is the one peripheral whose pins are
// a free choice within a block. SCK and MOSI are not free even so: SPI1 offers
// SCK on GP10/GP14/GP26 and MOSI on GP11/GP15/GP27, and the GP14/GP15 pair is
// where the encoder now sits.
#define SPI1_SCK_PIN 10
#define SPI1_MOSI_PIN 11

// TFT display — GMT020-02-7P v1.3 (ST7789, 240x320).
// The pins are mirrored into TFT_eSPI via build flags in platformio.ini —
// including TFT_SPI_PORT=1, which is what selects spi1 — and both must be
// changed together.
//
// The six signals land on consecutive header pins 11 to 16 with GND at 13, so
// the panel takes one flat connector. Better still, pins 14-15-16 carry SCL,
// SDA and RES in the module's *own* silkscreen order, which the old GP16-GP20
// block did not: three of the five wires now run straight across.
//
// RST is on GP12, which is also SPI1's RX pin, and CS on GP8, which is the
// other one. Both are deliberate and harmless: the panel never drives data
// back, so RX is never enabled — TFT_eSPI passes MISO as -1 to its SPI object —
// and the two pins stay plain outputs.
#define TFT_CS_PIN 8
#define TFT_DC_PIN 9
#define TFT_RST_PIN 12

// Rotary encoder (EC11) — quadrature + push switch, all active LOW with pullups.
//
// A, GND, B, SW on header pins 17 to 20 — four consecutive pins carrying the
// EC11's own terminal order, with the ground the connector needs already in the
// middle of the block. The switch returns to that same ground, so one flat
// four-way connector does the whole part with nothing to enjamb.
#define ENCODER_A_PIN 13
#define ENCODER_B_PIN 14
#define ENCODER_SW_PIN 15

// Turning the knob clockwise must count up. Which of the two quadrature pads a
// maker calls "A" is not standardised, so a reversed knob is a property of the
// part, not a wiring fault — hence a flag rather than a swap of the two pins
// above, which would leave config.h disagreeing with the silkscreen and with
// HARDWARE.md. Flip it if encoder_test reports CCW while you turn right.
#define ENCODER_REVERSED false

// Panel controls — two dedicated buttons and two status LEDs, on one contiguous
// block of header pins, 1 to 5, with the ground in the middle:
//
//   1   GP0   STOP
//   2   GP1   START
//   3   GND   both button commons and both LED cathodes
//   4   GP2   green LED anode
//   5   GP3   red LED anode
//
// One flat connector for the whole panel, nothing to enjamb — the same reason
// the encoder takes header pins 17 to 20 above.
//
// GP0 and GP1 are UART0's default pins, which nothing here uses: the logs go out
// over USB CDC and the RS485 bus has UART1. Putting the buttons there does spend
// the one place a rescue serial console could have been landed, which is the
// price of having the panel connector at the end of the header the panel loom
// arrives at.

// START and STOP, both active LOW with internal pullups. Dry contacts to
// ground: the internal pullup is the only pull there is, as on the encoder.
//
// v4 shipped with a single button toggling the session. A toggle answers the
// wrong question in front of the machine: the operator reaching for it wants to
// *stop*, and has to know what the dryer is currently doing to predict what the
// press will do. Two dedicated buttons remove that inference. A press on STOP
// stops, whatever the state, and neither button can be the other by mistake.
//
// STOP takes GP0 and START GP1 — the other way round from v4 — because that is
// the order the panel loom arrives in. Which end of the loom carries which
// button is a property of the panel, not a wiring fault, so it is fixed here.
#define BTN_START_PIN 1
#define BTN_STOP_PIN 0

// Status LEDs — one green, one red, active HIGH, anode on the GPIO and cathode
// to ground through a series resistor. Two discrete LEDs rather than one RGB
// part: it says the same four states on one GPIO less.
//
//   running       green steady
//   cooling down  green blinking
//   stopped       red steady
//   fault         red blinking, green off
//
// No state has both LEDs dark, so a dead LED or an unpowered board does not
// look like a dryer sitting quietly at rest.
//
// 330 ohm in series with each, which puts ~4.2 mA through the red (Vf 1.9V) and
// ~3.6 mA through the green (Vf 2.1V), inside the RP2040 pad's default 4 mA
// drive. Use a *standard* green: a high-brightness InGaN one drops 3.0-3.2V and
// would leave the resistor 0.1V to work with. See HARDWARE.md.
#define LED_RUN_PIN 2
#define LED_FAULT_PIN 3

// RS485 — single Modbus bus carrying both probes and the hydraulic module
// (UART1 / Serial2 → MAX3485)
//
// TX and RX have not moved and cannot: UART1 exists on GP4/GP8/GP12/GP20 for TX
// and GP5/GP9/GP13/GP21 for RX, and of those only GP4/GP5 are still free once
// the display has GP8-GP12 and the encoder GP13-GP15. DE moved from GP3 to GP6
// to let the panel have GP0-GP3, which puts the transceiver on header pins 6 to
// 9 — TX, RX, GND, DE — one contiguous block again, ground included.
#define RS485_TX_PIN 4 // UART1 TX → MAX3485 DI
#define RS485_RX_PIN 5 // UART1 RX ← MAX3485 RO
#define RS485_DE_PIN 6 // DE/RE direction enable (HIGH = transmit, LOW = receive)

// Command outputs — one BC337 per output, NPN in common emitter, low side.
//
// Polarity is per output because it depends on how each load is wired, and all
// three happen to land on active HIGH by two different routes:
//   collector in series with a contactor coil to +24V → GPIO HIGH saturates the
//     transistor and energises it (active HIGH). The stage does not invert the
//     command here: the coil is the load;
//   collector pulling down an input the receiver already pulls up and that is
//     itself active LOW → the stage inverts, the receiver inverts again, and the
//     command comes back to active HIGH. This is the damper module.
// What every case must share is the *floating* GPIO leaving the load off: until
// pinMode() runs, roughly two seconds after power-up, the pads are inputs and
// sit low through their default pull-down. See HARDWARE.md — the fan and the
// electric heater must stay off for that whole window, and output_test reads the
// resting level of all three pins back before anything drives them.
//
// The three moved from GP0-GP2 to the far side of the header for the PCB, and
// that argument survives the move intact: GP16, GP17 and GP22 wake up exactly as
// GP0-GP2 did, as inputs with the pull-down enabled. The RP2040 pins that do
// *not* — GP23, GP24, GP25, GP29 — carry board functions on a Pico and never
// reach the header, so there is no way to land a command on one by accident.
//
// One new trap comes with the move, and it is silent: **nothing may ever call
// SPI.begin()**. That is arduino-pico's default SPI0 object, whose default pins
// are GP16, GP17, GP18 and GP19 — it would take the fan and the electric heater
// away from us and hand them to a shift register. Nothing does today; the
// display owns its own SPIClassRP2040 on spi1 and never touches the default one.
#define OUT_FAN_PIN 16
#define OUT_FAN_ACTIVE_LOW false
// The damper module drives its relay through a BC337, an NPN in common
// emitter: the stage inverts, so GPIO HIGH now commands extraction and a
// floating GPIO (the RP2040 pads idle as inputs with a pull-down) leaves the
// relay released — recirculation, the safe state, during the whole boot window.
// Checked end to end with the damper_test environment.
//
// It sits on GP22 rather than beside the other two commands so that the whole
// register loom is one block: command on header pin 29, the two feedbacks on 31
// and 32, and AGND at 33 for their return. GP22 is also the least capable pin on
// that side — no SPI, no I2C, no UART, no ADC — which makes it the right one to
// spend on a plain digital output.
#define OUT_DAMPER_PIN 22
#define OUT_DAMPER_ACTIVE_LOW false
#define OUT_ELECTRIC_PIN 17
#define OUT_ELECTRIC_ACTIVE_LOW false

// Air damper position feedback — one ADC channel per register.
//
// A dryer carries one or two registers, extraction and recycling, and when it
// carries two they are **asymmetric**: different vane geometry, so different
// travel, so each needs its own two-point calibration. They share the single
// command above because they are complementary — air is either extracted or
// recycled, never both — so one relay drives both actuators, one of them wired
// to travel the other way.
//
// Both feedbacks go through the same divider: R1 = 10k to the Belimo U output,
// R2 = 3.3k to ground, ratio 3.3/13.3 = 0.2481, which puts the actuator's 10.10V
// end at 2.51V and keeps 800mV clear of the 3.3V rail. That headroom is the
// reason for 3.3k over the 4.7k first considered: at 0.3197 the same 10.10V
// would land at 3.23V, 89 counts from clipping, and a clipped reading is
// indistinguishable from a register genuinely sitting at its stop. damper_test
// warns when a cycle's highest sample comes within 400 counts of full scale. A
// 100nF from each tap to ground feeds the ADC's sample-and-hold and keeps mains
// hum off a field wire.

//
// These two pins are not a free choice. The Pico exposes exactly three ADC
// channels — GP26, GP27, GP28 — because ADC3 is wired to the VSYS divider on
// the board and never reaches the header. Two feedbacks plus the RTC's I2C do
// not fit in three pins, which is why the RTC moved off GP26/GP27 and onto
// I2C0 below. Nothing else can be moved here instead: the ADC channels are tied
// to these GPIOs in the silicon.
#define DAMPER_EXTRACTION_FEEDBACK_PIN 26 // ADC0
#define DAMPER_RECYCLING_FEEDBACK_PIN 27  // ADC1

// How many registers this dryer actually has — a menu setting, not a #define.
//
// It used to be a compile-time DAMPER_RECYCLING_FITTED, which meant a rebuild to
// declare the second register's wire landed. It is now `damper_count` in the
// settings record, and it does three things: it decides whether GP27 is sampled
// at all, it greys out the recycling entries in the menu, and it arms the
// airflow interlock — which only exists with two registers, since one register
// alone cannot shut the air path on its own.
//
// One is the factory value, so an uncalibrated dryer behaves exactly as the
// firmware did before the setting existed. A channel that is not declared is
// never sampled: a floating input is not a harmless zero, it reads wandering
// noise that the screen would present as a live opening. Unsampled,
// DamperFeedback simply has no sample and reports no position — dashes on the
// screen, the sentinel over the air, and nothing anywhere invents a number.
#define DAMPER_COUNT_DEFAULT 1
#define DAMPER_COUNT_MAX 2

// Which end of the feedback signal means "open" — one setting for every
// register, because it is a property of the actuator model and its linkage, not
// of an individual register.
//
// On this dryer the signal runs backwards: the Belimo puts out 10.10V with the
// register shut and 2.00V with it open, so the *low* end is the open one. That
// used to be implicit in the order of the calibration pair, where entering the
// two values the wrong way round reported every opening inside out while looking
// entirely plausible. The pair is now two ordered marks, min and max, and this
// flag alone says what they mean.
#define DAMPER_FEEDBACK_LOW_IS_OPEN_DEFAULT true

// Which way each actuator travels under the single command — one flag per
// register, because each Belimo carries its own mechanical direction switch.
//
// The firmware cannot read that switch, but it has to know where it is set: it
// is what tells travel detection which end a register is heading for, and what
// tells the airflow interlock which reading means shut. The factory values are
// the complementary pair the dryer has always run: the extraction register opens
// on the extraction command, the recycling one closes.
#define DAMPER_EXTRACTION_INVERTED_DEFAULT false
#define DAMPER_RECYCLING_INVERTED_DEFAULT true

// Below this raw value, that channel is carrying no signal at all.
//
// This works because the divider's R2 = 3.3k sits between the tap and ground: a
// feedback wire that is absent, cut or dead leaves the ADC pin pulled down to
// roughly zero, not floating. A live signal never goes below the actuator's own
// 2.00V floor, which is ~616 counts through the divider. 250 counts — about
// 0.81V at the actuator — sits well clear of both, so the test never confuses a
// register genuinely at its low end with a wire that is not there.
//
// Confirmed over a few consecutive samples so a single noisy conversion cannot
// declare a working feedback dead.
#define DAMPER_SIGNAL_MIN_RAW 250
#define DAMPER_SIGNAL_CONFIRM_SAMPLES 3

// Raw 12-bit ADC values at the two ends of travel, the starting point for both
// registers. Measured on the extraction register and confirmed against a meter
// at the same node.
//
// These are **ordered marks, not named ends**: min is simply the smaller of the
// two readings and max the larger, and which of them is the open one is said
// once by DAMPER_FEEDBACK_LOW_IS_OPEN_DEFAULT above. That split is deliberate.
// Naming them "closed" and "open" put the signal's direction inside the pair,
// where entering the two values the wrong way round produced a screen that was
// inside out and entirely believable. Ordered marks cannot be entered the wrong
// way round — a descending pair is now rejected as an unusable calibration.
//
// Through the divider those two voltages predict 616 and 3110; the bench reads
// 641 and 3179, once the wiring fault below was found and the readings were
// allowed to settle. Both ends agree on one ratio — 0.2583 open, 0.2537 shut,
// against 0.2481 designed — a 1.8% disagreement, which is what a pair of 1%
// resistors and an unregulated 3V3 used as the ADC reference are entitled to.
// Fitted as a line, that is 313 counts per volt with a 14 count offset. The
// travel, 2538 counts, and 916 counts remain before the ADC clips.
//
// Getting here took three false starts, all worth recording because all are
// easy to repeat. The first bench run gave 1392 and 3845, from a fixed 150s hold
// rather than from the reading going quiet: one end had arrived, the other was
// still creeping, and comparing a mid-travel sample against an end-stop voltage
// manufactures an offset that does not exist. damper_test now waits for a
// settled reading and will not call anything an end stop until it stops moving.
// The second was a genuine wiring fault on the prototype, which made the divider
// measure 0.280 instead of 0.248 with both resistors confirmed correct. The
// third read 51..4095 on a full sweep, which is not a 2-10V signal at all: below
// the actuator's own floor at one end and clipping at the other, the signature
// of a divider not dividing. Values 630 and 3104 date from before that one was
// fixed, which is why they are gone.
//
// Both were caught by the same test, which is the one worth keeping: a divider
// has exactly one ratio. Whenever two calibration points disagree about it, at
// least one of them is not where it claims to be — and no amount of software
// compensation is the answer.
//
// The two registers being asymmetric, these only describe both of them until
// the first calibration; each register then keeps its own pair, captured from
// the menu by driving it to each end stop. The menu values are persisted and
// are what the firmware actually runs on — these defaults only cover a dryer
// that has never been calibrated.
#define DAMPER_RAW_MIN_DEFAULT 641   // 2.00V through the divider — open, here
#define DAMPER_RAW_MAX_DEFAULT 3179  // 10.10V — shut, here
// Below this span the calibration is treated as invalid (feedback wire absent,
// or the pair entered in descending order).
#define DAMPER_CALIBRATION_MIN_SPAN 200
// Distance from the commanded end stop (%) under which travel is complete.
#define DAMPER_POSITION_TOLERANCE 5.0f

// --- Airflow interlock ------------------------------------------------------
//
// With two registers the air path can be shut completely, and a fan pushing
// against two closed vanes moves nothing: the session has to be refused, and
// stopped if it is already running. One register cannot do it — hence the
// interlock only exists at DAMPER_COUNT_MAX.
//
// The confirmation delay costs nothing legitimate. Complementary registers pass
// each other mid-travel, one climbing while the other falls, and are never both
// under the closed threshold at the same time. Two actuators whose direction
// switches are set the same way do settle there, and that is precisely the fault
// this catches — but only after they have had time to arrive, so the delay is
// generous rather than tight.
#define DAMPER_CLOSED_THRESHOLD 10.0f    // % at or below which a register is shut
#define DAMPER_BLOCKED_CONFIRM_MS 30000  // both shut this long = no airflow

// RTC DS1307 on I2C0 — optional, an absent RTC disables ECO mode.
//
// On I2C0 rather than I2C1, and not on GP26/GP27, because the two damper
// feedbacks need the ADC channels those carry. GP20 is I2C0 SDA and GP21 is I2C0
// SCL: RP2040 datasheet Table 2 "GPIO Functions", column F3, whose I2C pattern
// is periodic modulo 4 across GP0-GP29 (0 -> I2C0 SDA, 1 -> I2C0 SCL, 2 -> I2C1
// SDA, 3 -> I2C1 SCL). The Wire library validates both against that same table
// and would refuse the bus outright if either were wrong.
//
// GP20/GP21 rather than the GP28/GP21 pair used before the PCB, because that
// pattern leaves exactly one *adjacent* I2C0 pair on this side of the header,
// and this is it: SDA on pin 26, SCL on 27, GND on 28. Three consecutive pins
// for a module that has four, the fourth being 3V3. It also gives GP28 back, so
// ADC2 is a spare channel again rather than a sacrificed one.
#define RTC_I2C_SDA_PIN 20
#define RTC_I2C_SCL_PIN 21

// Free for expansion: GP7, GP18, GP19 and GP28.
//
// GP18 and GP19 are the useful pair — together they are a whole I2C1 bus, or,
// with GP16/GP17 borrowed back, a whole SPI0. GP28 is ADC2, the third and last
// analog channel, free again since the RTC moved to GP20/GP21. GP7 is a bare
// GPIO on the panel side of the header.
//
// The extension port needed none of them: it is a slave address on the existing
// RS485 segment, so it costs no transceiver, no UART and no GPIO. They stay free
// for whatever comes next — and should a future extension ever want a segment of
// its own, Rs485Bus::kMaxBuses is already 2 and the pins are there.

// ========== I2C ADDRESSES ==========
#define RTC_DS1307_ADDR 0x68 // DS1307 (on i2c0, see RTC_I2C_*_PIN above)

// ========== RS485 / MODBUS ==========
#define MODBUS_BAUDRATE 9600

// Slave addresses on the single RS485 bus.
// v4 carries one probe: the outlet one was polled and transmitted but never
// fed a control decision, so it was dropped rather than kept warm. Address 2
// fell vacant with it and now carries the extension port.
//
// Modbus RTU allows one master per segment and the dryer is it — both the probe
// and the hydraulic module depend on that. Everything else here is a slave.
#define MODBUS_INLET_ADDRESS 1
#define MODBUS_EXTENSION_ADDRESS 2
#define MODBUS_HYDRAULIC_ADDRESS 10

// SHT30 RS485 sensor register map (function code FC03)
#define MODBUS_REG_HUMIDITY 0x0000    // raw / MODBUS_RAW_SCALE = %RH
#define MODBUS_REG_TEMPERATURE 0x0001 // raw / MODBUS_RAW_SCALE = C
#define MODBUS_RAW_SCALE 10.0f        // sensor raw value divisor

// Hydraulic module register map.
//
// Two blocks, both driven by the dryer because a slave never speaks unprompted:
// one FC16 out with the permission and the setpoint, one FC03 back with the
// measurements. The wire format is in HydraulicProtocol.h, which the module
// firmware compiles too — that header and these addresses are mirrored in the
// dryer-extension repository and the two copies must stay identical.
//
// Readings are **signed tenths** in an unsigned register: cast to int16_t
// before dividing, because the water loop legitimately reads below zero. A
// reading the module does not have is INT16_MIN, never a zero.
#define HYDRO_REG_STATE 0x0000          // write: 0 = stand down, 1 = cleared to run
#define HYDRO_REG_WATER_TARGET 0x0001   // write: water setpoint x10 (C)
#define HYDRO_REG_DRYER_AIR_TEMP 0x0002 // write: inlet air temperature x10, signed
#define HYDRO_COMMAND_COUNT 3

#define HYDRO_REG_WATER_TEMP 0x0010      // read: circulating water temperature x10
#define HYDRO_REG_TANK_TEMP 0x0011       // read: storage tank temperature x10
#define HYDRO_REG_STATUS 0x0012          // read: status bits
#define HYDRO_REG_FAKE_WATER_TEMP 0x0013 // read: what the valve is being told, x10
#define HYDRO_REG_PUMP_SPEED 0x0014      // read: commanded circulator speed, percent
#define HYDRO_TELEMETRY_COUNT 5

// Extension port register map.
//
// Two blocks, both driven by the dryer because a slave never speaks unprompted:
// the dryer pushes its telemetry with one FC16, then reads the module's command
// mailbox with one FC03. A command therefore waits at most one poll cycle.
//
// The acknowledgement rides inside the telemetry block, so no separate write is
// needed to answer a command.
#define EXT_PROTOCOL_VERSION 1

#define EXT_REG_TELEMETRY 0x0000 // write: block base
#define EXT_TELEMETRY_COUNT 17   // registers in the block

#define EXT_REG_COMMAND 0x0040 // read: mailbox base
#define EXT_COMMAND_COUNT 4    // registers in the mailbox

// ========== TIMING CONSTANTS ==========
#define SENSOR_UPDATE_INTERVAL 2000  // ms

// Two thresholds on the same silence, because the two decisions cost very
// different things. Core 1 polls the inlet probe every SENSOR_UPDATE_INTERVAL,
// so both are really counted in missed polls: 5 to cut the heat, 30 to end the
// batch.
//
// Cutting the heat is instantly reversible — the reading comes back and the
// heaters resume where they left off. Ending a batch is not, and under the
// fault rules the dryer cannot even be restarted until the probe answers again.
// One number for both would have to be short enough to be safe, which would put
// a ten-second bus hiccup in a position to destroy a night's drying.
//
// The gap between them is not idle waiting: it is the purge window in
// Dryer::UpdateFaultResponse() — heat off, extraction open, phases frozen — so
// the machine spends it shedding heat rather than hoping the probe returns.
#define SENSOR_TIMEOUT_MS 10000         // ms — 5 missed polls: heating blocked
#define SENSOR_SESSION_TIMEOUT_MS 60000 // ms — 30 missed polls: session stopped

#define CONTROL_LOOP_INTERVAL 1000   // ms
#define SETTINGS_SAVE_INTERVAL 60000 // ms (1 minute)
#define DATA_LOG_INTERVAL 60000      // ms (1 minute)
#define INPUT_UPDATE_INTERVAL 50     // ms (button debounce)
#define DAMPER_SAMPLE_INTERVAL 500   // ms (position feedback, display only)
#define DISPLAY_UPDATE_INTERVAL 100  // ms (values throttle themselves further)

// Half period of the blinking LED states. Same value as TftDisplay's own blink,
// so the panel LED and the cooldown icon on screen beat together.
#define STATUS_BLINK_INTERVAL 500 // ms

// How long after boot a fault is held back from the LED.
//
// The inlet probe and the hydraulic module are both silent until Core 1 has
// completed its first RS485 cycle, so every freshly booted dryer is briefly in
// "fault" by the letter of the test. Long enough to cover SENSOR_UPDATE_INTERVAL
// several times over, short enough that a genuine fault present at power-up is
// still announced while the operator is standing there.
#define STATUS_FAULT_GRACE_MS 15000 // ms

// ========== DRYER DEFAULT PARAMETERS ==========

// Temperature target (menu overrides at runtime)
#define TEMPERATURE_TARGET 40.0f // °C

// Setpoint adjustment ranges (menu limits)
#define TARGET_TEMP_MIN 20.0f // °C
#define TARGET_TEMP_MAX 45.0f // °C
#define TARGET_HUM_MIN 0.0f   // %RH
#define TARGET_HUM_MAX 100.0f // %RH

// Hydraulic module water setpoint (fixed value pushed to the remote module)
#define WATER_TARGET_DEFAULT 55.0f // °C
#define WATER_TARGET_MIN 30.0f     // °C
#define WATER_TARGET_MAX 70.0f     // °C

// Heat source enable defaults — these are the factory values of the two menu
// toggles, not a statement about whether the hardware is present. Actual
// availability is decided at runtime: the hydraulic module has to answer on
// RS485, and the inlet probe has to be fresh.
#define HYDRAULIC_ENABLED_DEFAULT true
#define ELECTRIC_ENABLED_DEFAULT true

// ===== Heating Control Parameters =====
//
// The dryer regulates one source. The two are not symmetrical, and the reason
// is authority rather than speed:
//
//   Hydraulic — the remote module owns its start, its circulator and its water
//     regulation. The dryer publishes a run permission and a fixed water
//     setpoint over RS485 and reads telemetry back; it never cycles the module
//     on air temperature. Nothing here parameterises it.
//   Electric — the one source the dryer commands. Narrow hysteresis with
//     predictive shutoff, on the measured inlet temperature.

// ===== Electric Trim Parameters =====

// CTRL_BANDE_ELEC — hysteresis band (°C) for the electric heater.
//   Electric turns ON when error > CTRL_BANDE_ELEC, OFF when error ≤ 0 or an
//   overshoot is predicted.
//   Starting point: 0.5°C
#define CTRL_BANDE_ELEC 0.5f

// CTRL_HORIZON — prediction window (seconds) for predictive electric shutoff.
//   If T_measured + dT_dt × CTRL_HORIZON ≥ setpoint, the electric is cut off early
//   to avoid overshoot due to thermal inertia.
//   Set to the approximate time the temperature keeps rising after the relay opens.
//   Starting point: 60s
#define CTRL_HORIZON 60.0f

// CTRL_DT_PREDICT_MIN — minimum dT_dt (°C/s) required to activate predictive shutoff.
//   Below this threshold the derivative is sensor noise (0.1°C resolution at
//   1 Hz gives single-tick spikes of ~0.03°C/s once filtered).
//   Starting point: 0.05°C/s
#define CTRL_DT_PREDICT_MIN 0.05f

// CTRL_T_ON_MIN — minimum time (seconds) the electric must stay ON per cycle.
//   Starting point: 15s
#define CTRL_T_ON_MIN 15.0f

// CTRL_T_OFF_MIN — minimum time (seconds) the electric must stay OFF between cycles.
//   Conservative protection for the contactor while still allowing the system to
//   react within a couple of minutes when the temperature keeps dropping.
//   Starting point: 60s  →  try range [60 – 300s]
#define CTRL_T_OFF_MIN 60.0f

// ===== Air renewal =====
// CTRL_AIR_RENEWAL_S — how long (seconds) the loop stops second-guessing itself
//   after the damper moves. Opening the extraction injects outside air and the
//   inlet temperature falls; closing it again, the temperature climbs back at a
//   rate the predictive shutoff would read as an impending overshoot and cut the
//   electric well below setpoint, where CTRL_T_OFF_MIN would then hold it.
//   Inside this window the prediction is suspended and the minimum off-time is
//   waived, so the electric is free to work through the transient. The band and
//   the error ≤ 0 cutoff still apply, and so does the safety maximum: the window
//   relaxes when the heater may restart, never how hot it is allowed to get.
//   Sized on the register itself — the Belimo takes ~150 s to travel and the
//   extraction window is 120 s — and on two minutes off-setpoint being an
//   accepted cost of renewing the air.
//   Starting point: 120s
#define CTRL_AIR_RENEWAL_S 120.0f

// ===== Derivative filter =====
// DERIVATIVE_FILTER — low-pass coefficient (0–1) applied to dT_dt, which feeds
//   the predictive shutoff. Lower → more smoothing.
#define DERIVATIVE_FILTER 0.3f

// ===== Safety =====
// Hard cutoff: if the measured temperature exceeds this value, the electric is
// forced OFF and the hydraulic run permission is withdrawn immediately.
// Set this to ~5–10°C above the maximum expected operating setpoint.
#define TEMPERATURE_SAFETY_MAX 50.0f // °C

// ===== ECO Mode Parameters =====
#define ECO_START_HOUR 18
#define ECO_END_HOUR 9
#define ECO_NIGHT_TARGET_PERCENTAGE 85.0f

// ===== Phase Parameters =====
#define FAN_COOLDOWN_DURATION_S 60  // seconds — fan runs after stop to cool electric heater
#define INIT_PHASE_DURATION 3600    // seconds
#define BRASSAGE_PHASE_DURATION 900 // seconds
// 150s to open the air damper (2.5min)
// 60s to extract the air (1min)
// note: it takes 150s to close also (in brassage phase)
#define EXTRACTION_PHASE_DURATION 210 // seconds

// ===== Extraction Parameters =====
#define EXTRACTION_DAMPER_OPEN_DURATION 120 // seconds

#endif // CONFIG_H
