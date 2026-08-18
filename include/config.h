#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== PINS CONFIGURATION ==========

// ----- SPI0, the display alone -----
// Write-only, so no MISO is wired at all.
#define SPI0_SCK_PIN 18
#define SPI0_MOSI_PIN 19

// TFT display — GMT020-02-7P v1.3 (ST7789, 240x320).
// The pins are mirrored into TFT_eSPI via build flags in platformio.ini;
// both must be changed together.
//
// CS sits on GP16, which is also SPI0's RX pin. That is deliberate and
// supported: the panel never drives data back, so RX is idle, and TFT_eSPI
// re-asserts the pin as an output after spi.begin() precisely for this case.
// It puts the five signals on consecutive header pins 21-26 with GND at 23.
#define TFT_CS_PIN 16
#define TFT_DC_PIN 17
#define TFT_RST_PIN 20

// ----- SPI1, the LoRa radio alone -----
// The radio deliberately does not share the display's bus. TFT_eSPI may drive
// the panel through the RP2040's PIO rather than the hardware SPI block, in
// which case two different masters would be fighting over the same pins and no
// amount of transaction bracketing would help. Separate buses remove the
// question, and let each run at its own clock — the panel is happy at 40 MHz,
// the SX1262 tops out around 16.
// SPI1 pin choices are fixed by the RP2040: SCK {10,14,26}, MOSI {11,15,27},
// MISO {8,12,24,28}.
#define SPI1_SCK_PIN 10
#define SPI1_MOSI_PIN 11
#define SPI1_MISO_PIN 12
#define LORA_SPI_FREQUENCY 8000000 // Hz, conservative against the SX1262 limit

// LoRa radio — DX-LR30 (SX1262, 868 MHz). NSS is driven in software, so it is
// not tied to the SPI1 hardware chip-select pins.
#define LORA_NSS_PIN 13
#define LORA_BUSY_PIN 9
#define LORA_DIO1_PIN 15
#define LORA_RST_PIN 22

// Rotary encoder (EC11) — quadrature + push switch, all active LOW with pullups.
//
// SW is on GP8 rather than GP9 so the three signals plus a ground land on four
// consecutive header pins, 8 to 11: one flat connector, nothing to enjamb. BUSY
// took GP9 in exchange, which costs the radio nothing — RadioLib only reads it
// as a plain input, unlike SPI1 MISO whose pin choice is fixed by the RP2040.
#define ENCODER_A_PIN 6
#define ENCODER_B_PIN 7
#define ENCODER_SW_PIN 8

// Turning the knob clockwise must count up. Which of the two quadrature pads a
// maker calls "A" is not standardised, so a reversed knob is a property of the
// part, not a wiring fault — hence a flag rather than a swap of the two pins
// above, which would leave config.h disagreeing with the silkscreen and with
// HARDWARE.md. Flip it if encoder_test reports CCW while you turn right.
#define ENCODER_REVERSED true

// Single START/STOP button (active LOW, internal pullup).
// One press starts a stopped dryer, the next stops a running one.
#define BTN_START_PIN 14

// RS485 — single Modbus bus carrying both probes and the hydraulic module
// (UART1 / Serial2 → MAX3485)
#define RS485_TX_PIN 4 // UART1 TX → MAX3485 DI
#define RS485_RX_PIN 5 // UART1 RX ← MAX3485 RO
#define RS485_DE_PIN 3 // DE/RE direction enable (HIGH = transmit, LOW = receive)

// Command outputs — 2N2222 open collector.
//
// Polarity is per output because it depends on how each load is wired:
//   collector in series with a contactor coil to +24V → GPIO HIGH energises it
//     (active HIGH), and a floating GPIO leaves the load OFF, which is what we
//     want during the boot window before pinMode() runs;
//   collector pulling down an input that is pulled up by the receiver → GPIO
//     HIGH forces the line to 0V (active LOW).
// See HARDWARE.md: the fan and the electric heater must be wired active HIGH so
// they stay off while the MCU boots.
#define OUT_FAN_PIN 0
#define OUT_FAN_ACTIVE_LOW false
// The damper module drives its relay through a BC337, an NPN in common
// emitter: the stage inverts, so GPIO HIGH now commands extraction and a
// floating GPIO (the RP2040 pads idle as inputs with a pull-down) leaves the
// relay released — recirculation, the safe state, during the whole boot window.
// Checked end to end with the damper_test environment.
#define OUT_DAMPER_PIN 1
#define OUT_DAMPER_ACTIVE_LOW false
#define OUT_ELECTRIC_PIN 2
#define OUT_ELECTRIC_ACTIVE_LOW false

// Air damper position feedback — one ADC channel per register.
//
// This version has two registers, extraction and recycling, and they are
// **asymmetric**: different vane geometry, so different travel, so each needs
// its own two-point calibration. They share the single command above because
// they are complementary — air is either extracted or recycled, never both — so
// one relay drives both actuators, one of them wired to travel the other way.
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

// Set to 1 once the recycling register's feedback is actually wired.
//
// Until then GP27 is a floating input, and floating inputs are not merely
// useless: they read wandering noise that the screen would show as a live
// opening, and they present a high impedance to a multiplexed ADC. So the pin
// is simply never sampled. That leaves DamperFeedback with no sample at all,
// which it already reports as "no position" — the screen shows dashes, the LoRa
// uplink sends its no-feedback sentinel, and nothing anywhere invents a number.
// Preprocessor-valued rather than bool: it guards #if blocks.
#define DAMPER_RECYCLING_FITTED 0

// Raw 12-bit ADC values at each end stop, the starting point for both
// registers. Measured on the extraction register and confirmed against a meter
// at the same node.
//
// **The feedback runs backwards**: this actuator puts out 10.10V with the
// register shut and 2.00V with it open, so the closed value is the *higher*
// one. That is a property of the linkage, not a fault, and needs no inverting
// flag — GetPositionPercent() derives its span as open minus closed, which is
// simply negative here, and the guard against a degenerate span is written
// signed for exactly this case. Enter the pair the wrong way round, though, and
// the screen reports every opening inside out.
//
// Through the divider those two voltages predict 616 and 3110; the bench reads
// 630 and 3104, and the meter reads 0.49V at the tap where the ADC reports
// 0.508V. Both ends now agree on one ratio — 0.2438 open, 0.2477 shut, against
// 0.2481 designed — which is precisely what was missing while this was broken.
// The travel, 2474 counts, is 0.7% off theory, and 991 counts remain before the
// ADC clips.
//
// Getting here took two false starts, both worth recording because both are
// easy to repeat. The first bench run gave 1392 and 3845, from a fixed 150s hold
// rather than from the reading going quiet: one end had arrived, the other was
// still creeping, and comparing a mid-travel sample against an end-stop voltage
// manufactures an offset that does not exist. damper_test now waits for a
// settled reading and will not call anything an end stop until it stops moving.
// The second was a genuine wiring fault on the prototype, which made the divider
// measure 0.280 instead of 0.248 with both resistors confirmed correct.
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
#define DAMPER_RAW_CLOSED_DEFAULT 3104
#define DAMPER_RAW_OPEN_DEFAULT 630
// Below this span the calibration is treated as invalid (feedback wire absent).
#define DAMPER_CALIBRATION_MIN_SPAN 200
// Distance from the commanded end stop (%) under which travel is complete.
#define DAMPER_POSITION_TOLERANCE 5.0f

// RTC DS1307 on I2C0 — optional, an absent RTC disables ECO mode.
//
// On I2C0 rather than I2C1, and on these pins rather than GP26/GP27, because the
// two damper feedbacks need the ADC channels those carry. GP28 is I2C0 SDA and
// GP21 is I2C0 SCL: RP2040 datasheet Table 2 "GPIO Functions", column F3, whose
// I2C pattern is periodic modulo 4 across GP0-GP29 (0 -> I2C0 SDA, 1 -> I2C0
// SCL, 2 -> I2C1 SDA, 3 -> I2C1 SCL). The Wire library validates both against
// that same table and would refuse the bus outright if either were wrong.
#define RTC_I2C_SDA_PIN 28
#define RTC_I2C_SCL_PIN 21

// No GPIO left for expansion: GP21, the last spare, went to I2C0 SCL when the
// second register arrived. Freeing one more means giving up a function — the
// cheapest is TFT_RST (TFT_eSPI accepts -1 and resets the panel in software),
// which would return GP20.

// ========== I2C ADDRESSES ==========
#define RTC_DS1307_ADDR 0x68 // DS1307 (on I2C Bus 1)

// ========== RS485 / MODBUS ==========
#define MODBUS_BAUDRATE 9600

// Slave addresses on the single RS485 bus.
// v4 carries one probe: the outlet one was polled and transmitted but never
// fed a control decision, so it was dropped rather than kept warm.
#define MODBUS_INLET_ADDRESS 1
#define MODBUS_HYDRAULIC_ADDRESS 10

// SHT30 RS485 sensor register map (function code FC03)
#define MODBUS_REG_HUMIDITY 0x0000    // raw / MODBUS_RAW_SCALE = %RH
#define MODBUS_REG_TEMPERATURE 0x0001 // raw / MODBUS_RAW_SCALE = C
#define MODBUS_RAW_SCALE 10.0f        // sensor raw value divisor

// Hydraulic module register map (FC03 read / FC06 write)
#define HYDRO_REG_STATE 0x0000       // write: 0 = off, 1 = on
#define HYDRO_REG_WATER_TARGET 0x0001 // write: water setpoint x10 (C)
#define HYDRO_REG_WATER_TEMP 0x0010  // read: circulating water temperature x10
#define HYDRO_REG_TANK_TEMP 0x0011   // read: storage tank temperature x10
#define HYDRO_REG_STATUS 0x0012      // read: status bits

// ========== LORA ==========
// DX-LR30 (SX1262) on SPI0, driven by RadioLib. EU 868 MHz band.
#define LORA_FREQUENCY 868.0f     // MHz
#define LORA_BANDWIDTH 125.0f     // kHz
#define LORA_SPREADING_FACTOR 9
#define LORA_CODING_RATE 7
#define LORA_SYNC_WORD 0x34
#define LORA_TX_POWER 14          // dBm, EU868 limit without duty-cycle tricks
#define LORA_PREAMBLE_LENGTH 8
#define LORA_TELEMETRY_INTERVAL_MS 60000
// Identifies this dryer on a shared band; frames addressed elsewhere are dropped.
#define LORA_DEVICE_ID 1

// ========== TIMING CONSTANTS ==========
#define SENSOR_UPDATE_INTERVAL 2000  // ms
#define SENSOR_TIMEOUT_MS 10000      // ms — heating disabled if inlet sensor silent for this long
#define CONTROL_LOOP_INTERVAL 1000   // ms
#define SETTINGS_SAVE_INTERVAL 60000 // ms (1 minute)
#define DATA_LOG_INTERVAL 60000      // ms (1 minute)
#define INPUT_UPDATE_INTERVAL 50     // ms (button debounce)
#define DAMPER_SAMPLE_INTERVAL 500   // ms (position feedback, display only)
#define DISPLAY_UPDATE_INTERVAL 100  // ms (values throttle themselves further)

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
// Two independent on/off sources share the same measured air temperature:
//
//   Hydraulic — base heat. The remote module holds a fixed water setpoint and
//     is commanded on/off. Its three-way valve is far too slow to modulate, so
//     it runs on a wide hysteresis band with long minimum on/off times.
//   Electric — fine trim. Narrow hysteresis with predictive shutoff, closing
//     the last degree that the hydraulic cannot resolve.
//
// CTRL_BANDE_HYDRO must stay well above CTRL_BANDE_ELEC so a large error
// engages both sources while a small one is trimmed by the electric alone.

// CTRL_BANDE_HYDRO — error (°C) above which the hydraulic source is requested.
//   Raise if the hydraulic engages for gaps the electric could close alone.
//   Starting point: 1.5°C
#define CTRL_BANDE_HYDRO 1.5f

// CTRL_HYDRO_T_ON_MIN / CTRL_HYDRO_T_OFF_MIN — minimum time (seconds) the
//   hydraulic must stay on, respectively off, per cycle. These protect the
//   three-way valve and the circulator, and must exceed the time the valve
//   needs to travel and the loop to reach temperature.
//   Starting point: 300s (5 minutes) each
#define CTRL_HYDRO_T_ON_MIN 300.0f
#define CTRL_HYDRO_T_OFF_MIN 300.0f

// CTRL_HYDRO_HORIZON — prediction window (seconds) for hydraulic shutoff.
//   Longer than the electric horizon: the water loop keeps giving off heat well
//   after the circulator stops.
//   Starting point: 180s
#define CTRL_HYDRO_HORIZON 180.0f

// ===== Electric Trim Parameters =====

// CTRL_BANDE_ELEC — hysteresis band (°C) for the electric heater.
//   Electric turns ON when error > CTRL_BANDE_ELEC, OFF when error ≤ 0 or an
//   overshoot is predicted. Must stay well below CTRL_BANDE_HYDRO.
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

// ===== Derivative filter =====
// DERIVATIVE_FILTER — low-pass coefficient (0–1) applied to dT_dt, which feeds
//   both predictive shutoffs. Lower → more smoothing.
#define DERIVATIVE_FILTER 0.3f

// ===== Safety =====
// Hard cutoff: if the measured temperature exceeds this value, both heat sources
// are forced OFF immediately.
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
