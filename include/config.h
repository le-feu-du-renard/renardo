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

// ===== PID Parameters (hydraulic controller) =====
//
// One PID drives the hydraulic source (0–100%). The electric source is
// controlled separately by a state machine (see CTRL_ parameters below).
//
// The system has significant inertia (large dryer volume, slow thermal response)
// so all gains should remain moderate — avoid aggressive tuning.
//
// Kp — Proportional gain
//   Effect: immediate reaction to the temperature error (setpoint − measured).
//   u_p = Kp × error.
//   Increase if the response is too slow.
//   Decrease if the output oscillates.
//   Starting point: 15.0  →  try range [5.0 – 25.0]
#define HYDRAULIC_KP 15.0f

// Ki — Integral gain
//   Effect: eliminates the steady-state offset that Kp alone cannot correct.
//   Keep low — the integral is frozen during BOOST and electric-only mode.
//   Increase only if temperature stabilises below setpoint permanently.
//   Starting point: 0.1  →  try range [0.05 – 0.3]
#define HYDRAULIC_KI 0.1f

// Kd — Derivative gain
//   Effect: anticipates the error trend (rate of change), helps brake before overshoot.
//   Filtered by PID_DERIVATIVE_FILTER to reduce sensor noise.
//   Starting point: 2.0  →  try range [0.5 – 5.0]
#define HYDRAULIC_KD 2.0f

// PID_INTEGRAL_MAX — Anti-windup clamp on the integral accumulator (°C·s).
//   Max integral output contribution = Ki × PID_INTEGRAL_MAX = 0.1 × 200 = 20% of u.
//   Reduce toward 100 if overshoot occurs after a long cold start.
#define PID_INTEGRAL_MAX 200.0f

// PID_DERIVATIVE_FILTER — Low-pass filter coefficient for the derivative (0–1).
//   Also used to filter the temperature derivative for ETA and prediction.
//   Lower → more smoothing; 0.1 is conservative for 1 Hz Modbus sensor data.
#define PID_DERIVATIVE_FILTER 0.3f

// ===== Electric Boost State Machine Parameters =====
//
// The electric heater (ON/OFF) is governed by a state machine with two modes:
//
//   REGULATION  — hydraulic runs via PID; electric is OFF.
//   BOOST       — electric ON, hydraulic forced to 100%.
//
// BOOST is triggered when the PID alone cannot close the gap fast enough;
// it exits once the setpoint is nearly reached. An anti-short-cycle guard
// (CTRL_T_ON_MIN / CTRL_T_OFF_MIN) protects the electric relay.
//
// When hydraulic is disabled by the user, the system enters ELECTRIC_ONLY mode:
// the electric heater follows a simple ON/OFF hysteresis (CTRL_BANDE_ELEC).

// CTRL_E_HAUT — error threshold (°C) that triggers BOOST immediately.
//   Raise if the electric activates too eagerly during warm-up.
//   Lower if the system is too slow to supplement the hydraulic when needed.
//   Starting point: 5.0°C
#define CTRL_E_HAUT 5.0f

// CTRL_E_BAS — error threshold (°C) at which BOOST exits (hysteresis low end).
//   Must be < CTRL_E_HAUT. Represents "close enough to setpoint" for regulation.
//   Starting point: 0.4°C
#define CTRL_E_BAS 0.4f

// CTRL_BANDE_ELEC — hysteresis band (°C) for electric-only mode (no hydraulic).
//   Electric turns ON when error > CTRL_BANDE_ELEC, OFF when error ≤ 0 or overshoot predicted.
//   Starting point: 0.5°C
#define CTRL_BANDE_ELEC 0.5f

// CTRL_T_SAT — how long (seconds) hydraulic must be saturated at ≥99% with
//   error > CTRL_E_BAS before BOOST is triggered via the saturation condition.
//   Increase if BOOST triggers too often during partial-load heating.
//   Starting point: 120s
#define CTRL_T_SAT 75.0f

// CTRL_ETA_MAX — maximum acceptable estimated time-to-setpoint (seconds).
//   If eta = error / dT_dt > CTRL_ETA_MAX and error > CTRL_E_BAS, BOOST is triggered.
//   Starting point: 900s (15 minutes)
#define CTRL_ETA_MAX 900.0f

// CTRL_DT_PREDICT_MIN — minimum dT_dt (°C/s) required to activate predictive shutoff
//   in ELEC_ONLY mode. Below this threshold the derivative is sensor noise
//   (0.1°C resolution at 1 Hz gives single-tick spikes of ~0.03°C/s filtered).
//   Starting point: 0.05°C/s
#define CTRL_DT_PREDICT_MIN 0.05f

// CTRL_DT_FALLING — temperature fall rate (°C/s) below which cond3 treats ETA as infinite.
//   If dT_dt < -CTRL_DT_FALLING (temperature dropping noticeably) AND error > CTRL_E_BAS,
//   BOOST triggers — even though dT_dt is not positive.
//   Covers the case where cold hydraulic water cools the air: the PID raises power
//   but can't overcome the heat loss without electric boost.
//   Raise if BOOST triggers too often on minor temperature dips.
//   Starting point: 0.01°C/s (= 0.6°C/min)
#define CTRL_DT_FALLING 0.01f

// CTRL_HORIZON — prediction window (seconds) for predictive electric shutoff.
//   If T_measured + dT_dt × CTRL_HORIZON ≥ setpoint, the electric is cut off early
//   to avoid overshoot due to thermal inertia.
//   Set to the approximate time the temperature keeps rising after the relay opens.
//   Starting point: 300s
#define CTRL_HORIZON 60.0f

// CTRL_T_ON_MIN — minimum time (seconds) the electric must stay ON per cycle.
//   Anti-short-cycle: BOOST cannot exit before this duration.
//   Starting point: 300s (5 minutes)
#define CTRL_T_ON_MIN 15.0f

// CTRL_T_OFF_MIN — minimum time (seconds) the electric must stay OFF between cycles.
//   Anti-short-cycle: BOOST cannot be triggered before this duration has elapsed
//   since the electric last turned OFF.
//   75s is a conservative protection for a contactor while still allowing the
//   system to react within 2 minutes when the temperature continues to drop.
//   Starting point: 75s  →  try range [60 – 300s]
#define CTRL_T_OFF_MIN 60.0f

// ===== Safety =====
// Hard cutoff: if the measured temperature exceeds this value, the electric heater is
// forced OFF immediately and the PID is reset. The hydraulic is manual and unaffected.
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
