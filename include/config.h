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
#define HYDRAULIC_ENABLED true
#ifdef ELECTRIC_HEATING
#define ELECTRIC_ENABLED true
#else
#define ELECTRIC_ENABLED false
#endif

// ===== PID Hydraulic Heater Parameters =====
#define HYDRAULIC_KP 5.0
#define HYDRAULIC_KI 0.1
#define HYDRAULIC_KD 2.0

// ===== PID Electric Heater Parameters =====
#define ELECTRIC_KP 10.0
#define ELECTRIC_KI 0.2
#define ELECTRIC_KD 1.0

// ===== PID Advanced Parameters =====
#define PID_INTEGRAL_MAX 50.0
#define PID_DERIVATIVE_FILTER 0.1

// ===== ECO Mode Parameters =====
#define ECO_START_HOUR 18
#define ECO_END_HOUR 9
#define ECO_NIGHT_TARGET_PERCENTAGE 85.0f

// ===== Phase Parameters =====
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
