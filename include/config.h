#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== PINS CONFIGURATION ==========

// I2C Bus 1 (MCP23017 GPIO expander + RTC DS1307)
#define I2C_BUS_1_SDA_PIN 10
#define I2C_BUS_1_SCL_PIN 11

// RS485 Modbus RTU (UART1 → MAX3485)
#define RS485_TX_PIN 12 // UART1 TX → MAX3485 DI
#define RS485_RX_PIN 13 // UART1 RX ← MAX3485 RO
#define RS485_DE_PIN 14 // DE/RE direction enable (HIGH = transmit, LOW = receive)

// Hydraulic circulator PWM (0-10V)
#define WATER_CIRCULATOR_PWM_PIN 15

// SD Card SPI (SPI0)
#define SD_CARD_MISO_PIN 0 // SPI0 RX
#define SD_CARD_CS_PIN 1   // Chip Select
#define SD_CARD_SCK_PIN 2  // SCK
#define SD_CARD_MOSI_PIN 3 // SPI0 TX

// Physical buttons (active LOW, internal pullup)
#define BTN_START_PIN 16
#define BTN_STOP_PIN 17

// Voltmeter outputs (PWM, 0-3V via RC filter)
// Channel assignment: change the pin to rewire a voltmeter to a different GPIO.
#define VOLTMETER_CH1_TEMPERATURE_PIN 19    // CH1 - inlet temperature
#define VOLTMETER_CH2_HUMIDITY_PIN 18       // CH2 - inlet humidity
#define VOLTMETER_CH3_TOTAL_DURATION_PIN 21 // CH3 - total session duration
#define VOLTMETER_CH4_PHASE_DURATION_PIN 20 // CH4 - current phase duration

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
#define MCP_LED_HEATER 0           // GPA0 - electric heater indicator LED
#define MCP_LED_HYDRO_HEATER 1     // GPA1 - hydraulic heater indicator LED
#define MCP_LED_FAN 2              // GPA2 - fan indicator LED
#define MCP_LED_AIR_RENEWAL 3      // GPA3 - air renewal indicator LED

// Port B – Digital outputs (Adafruit library: pin = 8 + GPB bit index)
#define MCP_AIR_DAMPER 8     // GPB0 - air damper
#define MCP_HEATER_RELAY 9   // GPB1 - electric heater relay
#define MCP_FAN_RELAY 10     // GPB2 - fan relay
#define MCP_BTN_START_LED 15 // GPB7 - START button indicator LED
#define MCP_BTN_STOP_LED 14  // GPB6 - STOP button indicator LED

// ========== RS485 / MODBUS ==========
#define MODBUS_BAUDRATE 9600
#define MODBUS_INLET_ADDRESS 1
#define MODBUS_OUTLET_ADDRESS 2

// SHT30 RS485 sensor register map (function code FC03)
#define MODBUS_REG_HUMIDITY 0x0000    // raw / MODBUS_RAW_SCALE = %RH
#define MODBUS_REG_TEMPERATURE 0x0001 // raw / MODBUS_RAW_SCALE = C
#define MODBUS_RAW_SCALE 10.0f        // sensor raw value divisor

// ========== TIMING CONSTANTS ==========
#define SENSOR_UPDATE_INTERVAL 2000   // ms
#define CONTROL_LOOP_INTERVAL 1000    // ms
#define SETTINGS_SAVE_INTERVAL 300000 // ms (5 minutes)
#define DATA_LOG_INTERVAL 60000       // ms (1 minute)
#define INPUT_UPDATE_INTERVAL 50      // ms (button debounce)

// ========== DRYER DEFAULT PARAMETERS ==========

// Temperature target (potentiometer overrides at runtime)
#define DEFAULT_TEMPERATURE_TARGET 40.0f // °C

// Potentiometer ADC mapping ranges
#define POT_TEMP_MIN 20.0f // °C - potentiometer minimum
#define POT_TEMP_MAX 45.0f // °C - potentiometer maximum
#define POT_HUM_MIN 0.0f   // %RH - potentiometer minimum
#define POT_HUM_MAX 100.0f // %RH - potentiometer maximum

// Voltmeter display ranges
#define VOLTMETER_TEMP_MAX 50.0f      // °C
#define VOLTMETER_HUM_MAX 100.0f      // %RH
#define VOLTMETER_TOTAL_DUR_H 48.0f   // hours
#define VOLTMETER_PHASE_DUR_MIN 60.0f // minutes

// Heater enable defaults
#define DEFAULT_HYDRAULIC_ENABLED true
#define DEFAULT_ELECTRIC_ENABLED true

// ===== PID Hydraulic Heater Parameters =====
// The hydraulic heater uses proportional control (0-100% power)
// It has high thermal inertia (slow response)

// Kp: Proportional gain - higher = stronger immediate reaction
#define DEFAULT_HYDRAULIC_KP 5.0

// Ki: Integral gain - eliminates steady-state offset
#define DEFAULT_HYDRAULIC_KI 0.1

// Kd: Derivative gain - damps oscillations
#define DEFAULT_HYDRAULIC_KD 2.0

// ===== PID Electric Heater Parameters =====
// The electric heater uses binary control (ON/OFF, threshold 0.5)

// Kp: High value needed to trigger binary control quickly
#define DEFAULT_ELECTRIC_KP 10.0

// Ki: Accumulates error for persistent heating needs
#define DEFAULT_ELECTRIC_KI 0.2

// Kd: Reduces overshoot from binary switching
#define DEFAULT_ELECTRIC_KD 1.0

// ===== PID Advanced Parameters =====
// Anti-windup integral clamp
#define DEFAULT_PID_INTEGRAL_MAX 50.0

// Derivative low-pass filter (0.0 = heavy filtering, 1.0 = none)
#define DEFAULT_PID_DERIVATIVE_FILTER 0.1

// ===== ECO Mode Parameters =====
// ECO mode is selected via physical MODE_SELECTOR_PIN (no time-based scheduling)
// In ECO mode: electric heater disabled, target reduced by this percentage
#define DEFAULT_ECO_NIGHT_TARGET_PERCENTAGE 85.0f

// ===== Phase Parameters =====
#define DEFAULT_INIT_PHASE_DURATION 3600       // seconds (1 hour max)
#define DEFAULT_BRASSAGE_PHASE_DURATION 120    // seconds (2 minutes)
#define DEFAULT_EXTRACTION_PHASE_DURATION 300  // seconds (5 minutes)
#define DEFAULT_EXTRACTION_HUM_THRESHOLD 70.0f // %RH - exit extraction when below

// Session duration limit
#define DEFAULT_DRYING_SESSION_DURATION 172800 // seconds (48 hours)

#endif // CONFIG_H
