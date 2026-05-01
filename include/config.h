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

#ifdef SENSOR_I2C
// Dedicated i2c0 bus for inlet probe
#define I2C_SENSOR_1_SDA_PIN 0
#define I2C_SENSOR_1_SCL_PIN 1
// I2C address for SEN0546 (ADDR pin to GND)
#define I2C_SENSOR_ADDRESS 0x40
#endif // SENSOR_I2C

#ifndef SENSOR_I2C
// RS485 Modbus RTU (UART1 → MAX3485)
#define RS485_TX_PIN 4 // UART1 TX → MAX3485 DI
#define RS485_RX_PIN 5 // UART1 RX ← MAX3485 RO
#define RS485_DE_PIN 3 // DE/RE direction enable (HIGH = transmit, LOW = receive)
#endif                 // SENSOR_I2C

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

#ifndef SENSOR_I2C
// ========== RS485 / MODBUS ==========
#define MODBUS_BAUDRATE 9600
#define MODBUS_INLET_ADDRESS 1
#define MODBUS_OUTLET_ADDRESS 2

// SHT30 RS485 sensor register map (function code FC03)
#define MODBUS_REG_HUMIDITY 0x0000    // raw / MODBUS_RAW_SCALE = %RH
#define MODBUS_REG_TEMPERATURE 0x0001 // raw / MODBUS_RAW_SCALE = C
#define MODBUS_RAW_SCALE 10.0f        // sensor raw value divisor
#endif                                // SENSOR_I2C

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
#define HYDRAULIC_AVAILABLE true  // overridable via build flag: -D HYDRAULIC_AVAILABLE=false
#endif
#ifdef ELECTRIC_HEATING
#define ELECTRIC_ENABLED true
#else
#define ELECTRIC_ENABLED false
#endif

// ===== PID Parameters (single split-range PID) =====
//
// This is the only PID in the system. Its output u ∈ [0, 100%] represents
// the total heating demand. The split-range logic then decides how to distribute
// this demand between the hydraulic (manual) and electric (ON/OFF) sources.
//
// The system has significant inertia (large dryer volume, slow electric heater)
// so all gains should remain moderate — avoid aggressive tuning.
//
// Kp — Proportional gain
//   Effect : immediate reaction to the temperature error (setpoint − measured).
//   u_p = Kp × error
//   Increase if the response is too slow (temperature takes too long to climb).
//   Decrease if the output oscillates or the electric relay switches on/off rapidly.
//   Typical sign of over-tuning: temperature overshoots the setpoint and hunts.
//   Starting point: 5.0  →  try range [3.0 – 10.0]
#define HYDRAULIC_KP 5.0

// Ki — Integral gain
//   Effect : eliminates the steady-state offset that Kp alone cannot correct.
//   u_i accumulates error × dt over time, so it builds up slowly.
//   Keep low: the electric heater has high inertia and the anti-windup only
//   partially compensates — a large Ki will still cause overshoot after a cold start.
//   Increase only if the temperature stabilises 1–2°C below setpoint permanently.
//   Decrease (or set to 0) if you observe slow oscillations after reaching setpoint.
//   Starting point: 0.1  →  try range [0.05 – 0.3]
#define HYDRAULIC_KI 0.1

// Kd — Derivative gain
//   Effect : anticipates the error trend (rate of change). Helps brake the response
//   before overshooting. Useful here because the dryer has long thermal lag.
//   Increase if the temperature regularly overshoots the setpoint after a cold start.
//   Decrease (or set to 0) if the output is noisy or the relay chatters.
//   Note: the derivative is filtered (PID_DERIVATIVE_FILTER) to reduce sensor noise.
//   Starting point: 2.0  →  try range [0.5 – 5.0]
#define HYDRAULIC_KD 2.0

// ===== PID Advanced Parameters =====

// PID_INTEGRAL_MAX — Anti-windup clamp on the integral accumulator (°C·s)
//   Limits how much the integral can build up regardless of how long the error persists.
//   The effective output contribution is Ki × integral, so max integral output = Ki × MAX.
//   With Ki=0.1 and MAX=200 → max integral contribution = 20% of u.
//   This allows the integral to compensate a small persistent error (e.g. 0.84°C) in
//   PRIMARY_ELEC mode, where u_max = Kp×0.84 + 20% = 24.2% > 8% threshold.
//   If the system overshoots after a long cold start, reduce this value toward 100.
#define PID_INTEGRAL_MAX 200.0

// PID_DERIVATIVE_FILTER — Low-pass filter coefficient for the derivative term (0–1)
//   filtered_d = α × raw_d + (1−α) × previous_filtered_d
//   Lower value → more filtering (smoother but slower derivative response).
//   Higher value → less filtering (faster but noisier).
//   0.1 is conservative and appropriate for 1 Hz sensor data with Modbus noise.
//   Increase toward 0.3 only if sensors are high-quality and low-noise.
#define PID_DERIVATIVE_FILTER 0.1

// ===== Split-Range Thresholds =====
//
// The PID output u ∈ [0, 100%] is distributed as follows:
//
//   Normal mode (hydraulic available):
//     u ∈ [0, 70%]  → hydraulic alone (electric stays OFF)
//     u ∈ [70, 80%] → hysteresis dead-band (electric stays in its current state)
//     u > 80%       → electric supplement requested (timer starts, see below)
//
//   Degraded mode (no hydraulic):
//     u < 10%       → electric OFF
//     u ∈ [10, 30%] → hysteresis dead-band
//     u > 30%       → electric ON

// SPLIT_ELECTRIC_ON — demand threshold above which the electric timer starts (normal mode)
//   Because the electric heater has ~30s of thermal lag after relay ON, triggering at
//   80% means the temperature has already been falling for a while before any heat arrives.
//   Lowering this threshold gives the heater a head start — it activates with more
//   temperature margin, so the heat arrives before the dip becomes too large.
//   Lower toward 60% if the temperature consistently dips too far below setpoint.
//   Raise toward 90% if the hydraulic alone can sustain the setpoint and the electric
//   is triggering unnecessarily.
#define SPLIT_ELECTRIC_ON      70.0f

// SPLIT_ELECTRIC_OFF — demand below which the electric heater is forced OFF (normal mode)
//   Must be strictly lower than SPLIT_ELECTRIC_ON to create a hysteresis dead-band.
//   This prevents the contactor from cycling rapidly around the threshold.
//   Recommended gap: at least 10%.
#define SPLIT_ELECTRIC_OFF     55.0f

// SPLIT_ELECTRIC_ON_DEG / SPLIT_ELECTRIC_OFF_DEG — thresholds for PRIMARY_ELEC mode
//   Electric is the primary (and only) source — it must activate even with small errors.
//   With Kp=5, Ki=0.1, PID_INTEGRAL_MAX=200:
//     u_max with 0.84 error = 5x0.84 + 0.1x200 = 24.2% -> above 8% threshold.
//   Gap between ON and OFF: at least 5% to avoid relay chattering.
//   Increase ON toward 15-20% if the relay cycles too rapidly.
#define SPLIT_ELECTRIC_ON_DEG   4.0f
#define SPLIT_ELECTRIC_OFF_DEG  2.0f

// ===== Electric Heater Timing =====

// ELECTRIC_ON_DELAY_S — how long (seconds) the demand must stay above SPLIT_ELECTRIC_ON
//   before the electric heater actually switches ON (normal mode only).
//   Purpose: short debounce to avoid triggering on a transient demand spike.
//
//   *** Important — thermal lag trade-off ***
//   The system is reactive, not predictive. By the time u > SPLIT_ELECTRIC_ON for
//   ELECTRIC_ON_DELAY_S seconds, the temperature has already been dropping. Adding
//   ELECTRIC_SETTLE_S on top (thermal lag before heat reaches the sensor) means the
//   total lag from "demand spike" to "effective heat" is:
//       ELECTRIC_ON_DELAY_S + ELECTRIC_SETTLE_S
//   During that entire window the temperature continues to fall.
//   → Keep this value SHORT (just enough to debounce transients, not a long guard).
//   → To reduce undershoot: lower SPLIT_ELECTRIC_ON so the heater triggers earlier.
//   Increase only if you observe the relay cycling ON/OFF rapidly (chattering).
//   Starting point: 10s  →  try range [5 – 20s]
#define ELECTRIC_ON_DELAY_S    10.0f

// ELECTRIC_SETTLE_S — how long (seconds) after the heater turns ON to keep the integral
//   frozen before the PID resumes normal integration.
//   Purpose: the electric heater has ~30s of thermal inertia before its heat reaches the
//   sensor. Without this freeze, the PID over-integrates during that blind window and
//   causes overshoot once the heat finally arrives.
//   Set this to roughly the heater's thermal lag (time from relay ON to measurable °C rise).
//   Increase if temperature still overshoots after the heater activates.
//   Decrease if the system is slow to react once the heater is running.
//   Starting point: 30s  →  try range [20 – 60s]
#define ELECTRIC_SETTLE_S      30.0f

// ELECTRIC_DT_ON — temperature must be at least this far below setpoint (°C) for
//   the electric timer to increment. Prevents activating the heater when temperature
//   is already very close to the setpoint (avoids overshoot from the electric's inertia).
//   Increase if the electric heater tends to push temperature above the setpoint.
//   Decrease toward 0.5 if the system consistently stabilises just below the setpoint.
//   Starting point: 2.0°C  →  try range [0.5 – 5.0°C]
#define ELECTRIC_DT_ON          2.0f

// ===== Safety =====
// Hard cutoff: if the measured temperature exceeds this value, the electric heater is
// forced OFF immediately and the PID is reset. The hydraulic is manual and unaffected.
// Set this to ~5–10°C above the maximum expected operating setpoint.
#define TEMPERATURE_SAFETY_MAX 50.0f  // °C

// ===== ECO Mode Parameters =====
#define ECO_START_HOUR 18
#define ECO_END_HOUR 9
#define ECO_NIGHT_TARGET_PERCENTAGE 85.0f

// ===== Phase Parameters =====
#define FAN_COOLDOWN_DURATION_S 60  // seconds — fan runs after stop to cool electric heater
#define INIT_PHASE_DURATION 3600    // seconds
#define BRASSAGE_PHASE_DURATION 600 // seconds
// 150s to open the air dumper (2.5min)
// 60s to extract the air (1min)
// note: it takes 150s to close also (in brassage phase)
#define EXTRACTION_PHASE_DURATION 210 // seconds

// ===== Extraction Parameters =====
#define EXTRACTION_DAMPER_OPEN_DURATION 120 // seconds
#define DRYING_SESSION_DURATION 172800      // seconds (48 hours)

#endif // CONFIG_H
