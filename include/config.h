#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== PINS CONFIGURATION ==========

// TFT display — GMT020-02-7P v1.3 (ST7789, 240x320) on SPI0, write-only (no MISO).
// These values are mirrored into TFT_eSPI via build flags in platformio.ini;
// both must be changed together.
#define TFT_SCK_PIN 18
#define TFT_MOSI_PIN 19
#define TFT_CS_PIN 17
#define TFT_DC_PIN 16
#define TFT_RST_PIN 22

// Rotary encoder (EC11) — quadrature + push switch, all active LOW with pullups
#define ENCODER_A_PIN 10
#define ENCODER_B_PIN 11
#define ENCODER_SW_PIN 12

// Physical buttons (active LOW, internal pullup)
#define BTN_START_PIN 20
#define BTN_STOP_PIN 21

// RS485 bus A — sensors + hydraulic module (UART1 / Serial2 → MAX3485)
#define RS485_A_TX_PIN 4 // UART1 TX → MAX3485 DI
#define RS485_A_RX_PIN 5 // UART1 RX ← MAX3485 RO
#define RS485_A_DE_PIN 3 // DE/RE direction enable (HIGH = transmit, LOW = receive)

// RS485 bus B — LoRa module (UART0 / Serial1 → MAX3485)
#define RS485_B_TX_PIN 0
#define RS485_B_RX_PIN 1
#define RS485_B_DE_PIN 2

// Command outputs — 2N2222 open collector, pulled up to the load side.
// The transistor inverts: GPIO HIGH pulls the output line to 0V.
#define OUT_FAN_PIN 6      // ventilation, 24V command signal
#define OUT_DAMPER_PIN 7   // air damper (Belimo LM24A-SR), 0-10V command signal
#define OUT_ELECTRIC_PIN 8 // electric heating, 24V command signal
#define OUTPUTS_ACTIVE_LOW true

// Air damper position feedback — Belimo 2-10V output through a divider (ADC0)
#define DAMPER_FEEDBACK_PIN 26

// I2C Bus 1 — optional RTC DS1307. Absent RTC disables ECO mode.
#define I2C_BUS_1_SDA_PIN 14
#define I2C_BUS_1_SCL_PIN 15

// ========== I2C ADDRESSES ==========
#define RTC_DS1307_ADDR 0x68 // DS1307 (on I2C Bus 1)

// ========== RS485 / MODBUS ==========
#define MODBUS_BAUDRATE 9600

// Bus A slave addresses
#define MODBUS_INLET_ADDRESS 1
#define MODBUS_OUTLET_ADDRESS 2
#define MODBUS_HYDRAULIC_ADDRESS 10

// Bus B slave address
#define MODBUS_LORA_ADDRESS 20

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

// LoRa module register map
#define LORA_REG_TELEMETRY 0x0000 // write: telemetry block base address
#define LORA_REG_COMMAND 0x0100   // read: pending command block base address
#define LORA_REG_ACK 0x0110       // write: last consumed command sequence number

// ========== TIMING CONSTANTS ==========
#define SENSOR_UPDATE_INTERVAL 2000  // ms
#define SENSOR_TIMEOUT_MS 10000      // ms — heating disabled if inlet sensor silent for this long
#define CONTROL_LOOP_INTERVAL 1000   // ms
#define SETTINGS_SAVE_INTERVAL 60000 // ms (1 minute)
#define DATA_LOG_INTERVAL 60000      // ms (1 minute)
#define INPUT_UPDATE_INTERVAL 50     // ms (button debounce)

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

// Heater enable defaults
#ifndef HYDRAULIC_AVAILABLE
#define HYDRAULIC_AVAILABLE true // overridable via build flag: -D HYDRAULIC_AVAILABLE=false
#endif
#ifdef ELECTRIC_HEATING
#define ELECTRIC_ENABLED true
#else
#define ELECTRIC_ENABLED false
#endif

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
// 150s to open the air dumper (2.5min)
// 60s to extract the air (1min)
// note: it takes 150s to close also (in brassage phase)
#define EXTRACTION_PHASE_DURATION 210 // seconds

// ===== Extraction Parameters =====
#define EXTRACTION_DAMPER_OPEN_DURATION 120 // seconds
#define DRYING_SESSION_DURATION 172800      // seconds (48 hours)

#endif // CONFIG_H
