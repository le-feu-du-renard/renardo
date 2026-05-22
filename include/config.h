#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== PINS CONFIGURATION ==========

// I2C Bus 1 (MCP23017 GPIO expander + RTC DS1307)
#define I2C_BUS_1_SDA_PIN 14
#define I2C_BUS_1_SCL_PIN 15

// SD Card SPI (SPI0)
#define SD_CARD_MISO_PIN 16 // SPI0 RX
#define SD_CARD_CS_PIN 17   // Chip Select
#define SD_CARD_SCK_PIN 18  // SCK
#define SD_CARD_MOSI_PIN 19 // SPI0 TX

// Hydraulic circulator PWM (0-10V)
#define WATER_CIRCULATOR_PWM_PIN 13

// RS485 Modbus RTU (UART1 → MAX3485)
#define RS485_TX_PIN 4 // UART1 TX → MAX3485 DI
#define RS485_RX_PIN 5 // UART1 RX ← MAX3485 RO
#define RS485_DE_PIN 3 // DE/RE direction enable (HIGH = transmit, LOW = receive)

// Physical buttons (active LOW, internal pullup)
#define BTN_START_PIN 20
#define BTN_STOP_PIN 21

// Voltmeter outputs (PWM, 0-3V)
#define VOLTMETER_INLET_HUMIDITY_PIN 6
#define VOLTMETER_INLET_TEMPERATURE_PIN 7
#define VOLTMETER_OUTLET_HUMIDITY_PIN 8
#define VOLTMETER_OUTLET_TEMPERATURE_PIN 9

// TM1637 4-digit LED display (total session duration)
#ifdef DURATION_DISPLAY
#define TM1637_CLK_PIN 11
#define TM1637_DIO_PIN 12
#endif // DURATION_DISPLAY

// Mode selector (LOW = ECO, HIGH = PERFORMANCE)
#define MODE_SELECTOR_PIN 22

// Potentiometers (ADC)
#define POT_TEMPERATURE_PIN 26 // ADC0
#define POT_HUMIDITY_PIN 27    // ADC1

// ========== I2C ADDRESSES ==========
#define MCP_EXPANDER_ADDRESS 0x20 // MCP23017 (on I2C Bus 1)
#define RTC_DS1307_ADDR 0x68      // DS1307 (on I2C Bus 1)

// ========== MCP23017 PIN MAPPING ==========

// Port A – Indicator LEDs (GPA bit index 0-7)
#define MCP_LED_ECO_MODE 7         // GPA7 - ECO mode indicator LED
#define MCP_LED_PHASE_INIT 6       // GPA6 - init phase indicator LED
#define MCP_LED_PHASE_BRASSAGE 5   // GPA5 - mixing phase indicator LED
#define MCP_LED_PHASE_EXTRACTION 4 // GPA4 - extraction phase indicator LED
#define MCP_LED_HEATER 3           // GPA3 - electric heater indicator LED
#define MCP_LED_HYDRO_HEATER 2     // GPA2 - hydraulic heater indicator LED
#define MCP_LED_FAN 1              // GPA1 - fan indicator LED
#define MCP_LED_AIR_RENEWAL 0      // GPA0 - air renewal indicator LED

// Port B – Digital outputs (Adafruit library: pin = 8 + GPB bit index)
#define MCP_BTN_START_LED 8 // GPB0 - START button indicator LED
#define MCP_BTN_STOP_LED 9  // GPB1 - STOP button indicator LED
#define MCP_HEATER_RELAY 10 // GPB2 - electric heater relay
#define MCP_FAN_RELAY 11    // GPB3 - fan relay
#define MCP_BELIMO_RELAY 12 // GPB4 - belimo damper actuator relay

// ========== RS485 / MODBUS ==========
#define MODBUS_BAUDRATE 9600
#define MODBUS_INLET_ADDRESS 1
#define MODBUS_OUTLET_ADDRESS 2

// SHT30 RS485 sensor register map (function code FC03)
#define MODBUS_REG_HUMIDITY 0x0000    // raw / MODBUS_RAW_SCALE = %RH
#define MODBUS_REG_TEMPERATURE 0x0001 // raw / MODBUS_RAW_SCALE = C
#define MODBUS_RAW_SCALE 10.0f        // sensor raw value divisor

// ========== TIMING CONSTANTS ==========
#define SENSOR_UPDATE_INTERVAL 2000  // ms
#define SENSOR_TIMEOUT_MS 10000      // ms — heating disabled if inlet sensor silent for this long
#define CONTROL_LOOP_INTERVAL 1000   // ms
#define SETTINGS_SAVE_INTERVAL 60000 // ms (1 minute)
#define DATA_LOG_INTERVAL 60000      // ms (1 minute)
#define INPUT_UPDATE_INTERVAL 50     // ms (button debounce)

// ========== DRYER DEFAULT PARAMETERS ==========

// Temperature target (potentiometer overrides at runtime)
#define TEMPERATURE_TARGET 40.0f // °C

// Potentiometer ADC mapping ranges
#define POT_TEMP_MIN 20.0f // °C
#define POT_TEMP_MAX 45.0f // °C
#define POT_HUM_MIN 0.0f   // %RH
#define POT_HUM_MAX 100.0f // %RH

// Voltmeter display ranges
#define VOLTMETER_TEMPERATURE_MAX 60.0f // °C (inlet and outlet)
#define VOLTMETER_HUMIDITY_MAX 100.0f   // %RH (inlet and outlet)

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
