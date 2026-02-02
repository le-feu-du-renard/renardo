#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== PINS CONFIGURATION ==========

// I2C Bus 1 (OLED + Outlet Air Sensor)
#define I2C_BUS_1_SDA_PIN 12
#define I2C_BUS_1_SCL_PIN 13

// I2C Bus 2 (DAC + Inlet Air Sensor + RTC)
#define I2C_BUS_2_SDA_PIN 10
#define I2C_BUS_2_SCL_PIN 11

// Rotary Encoder
#define ROTARY_ENCODER_SW_PIN 22
#define ROTARY_ENCODER_CLK_PIN 20
#define ROTARY_ENCODER_DT_PIN 21

// Relays
#define ELECTRIC_HEATER_RELAY_PIN 28
#define FAN_RELAY_PIN 27

// Water circulator
#define WATER_CIRCULATOR_PWM_PIN 7

// Water Temperature // OneWire
#define WATER_SENSOR_PIN 6

// SD Card SPI
#define SD_CARD_CS_PIN 5   // Chip Select
#define SD_CARD_MISO_PIN 4 // MISO (SPI0 RX)
#define SD_CARD_MOSI_PIN 3 // MOSI (SPI0 TX)
#define SD_CARD_SCK_PIN 2  // SCK (SPI0 SCK)

// Screen
#define SCREEN_SSD1306_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_OLED_RESET -1

// ========== I2C ADDRESSES ==========
#define DAC_GP8403_ADDR 0x5F
#define CHT8305_INLET_ADDR 0x40
#define CHT8305_OUTLET_ADDR 0x40
#define RTC_DS1307_ADDR 0x68

// ========== TIMING CONSTANTS ==========
#define SENSOR_UPDATE_INTERVAL 2000   // ms
#define DISPLAY_UPDATE_INTERVAL 100   // ms
#define CONTROL_LOOP_INTERVAL 1000    // ms
#define SETTINGS_SAVE_INTERVAL 300000 // ms (300 seconds = 5 minutes)
#define DATA_LOG_INTERVAL 60000       // ms (60 seconds = 1 minute)

// ========== DRYER DEFAULT PARAMETERS ==========

// Heating parameters
#define DEFAULT_TEMPERATURE_TARGET 50.0 // °C (20-45)
#define DEFAULT_HYDRAULIC_ENABLED true  // Enable hydraulic heater
#define DEFAULT_ELECTRIC_ENABLED true   // Enable electric heater

// ===== PID Hydraulic Heater Parameters =====
// The hydraulic heater uses proportional control (0-100% power)
// It has high thermal inertia (slow response)

// Kp: Proportional gain - Immediate response to temperature error
//     Higher Kp = stronger immediate reaction
//     - INCREASE if: system is too slow to reach target, undershoots
//     - DECREASE if: system oscillates, overshoots target
//     Range: 0.1-20, Default: 5.0 (moderate response)
#define DEFAULT_HYDRAULIC_KP 5.0

// Ki: Integral gain - Eliminates steady-state error over time
//     Accumulates error and provides correction
//     - INCREASE if: system never reaches exact target (offset remains)
//     - DECREASE if: system is sluggish, overshoots after long time
//     Range: 0.0-2, Default: 0.1 (slow integral action)
#define DEFAULT_HYDRAULIC_KI 0.1

// Kd: Derivative gain - Anticipates future error based on rate of change
//     Dampens oscillations and improves stability
//     - INCREASE if: system oscillates, overshoots frequently
//     - DECREASE if: system is too cautious, reacts to noise
//     Range: 0.0-5, Default: 2.0 (strong damping for high inertia)
#define DEFAULT_HYDRAULIC_KD 2.0

// ===== PID Electric Heater Parameters =====
// The electric heater uses binary control (ON/OFF with 0.5 threshold)
// It is undersized and has slow response

// Kp: Proportional gain - Immediate response to temperature error
//     Higher value needed for binary control to trigger quickly
//     - INCREASE if: electric heater rarely turns on, too conservative
//     - DECREASE if: electric heater switches too frequently
//     Range: 0.1-20, Default: 10.0 (strong response for binary control)
#define DEFAULT_ELECTRIC_KP 10.0

// Ki: Integral gain - Accumulates error for persistent heating needs
//     Important for maintaining heat when hydraulic is insufficient
//     - INCREASE if: temperature drops slowly over time
//     - DECREASE if: system becomes unstable after long periods
//     Range: 0.0-2, Default: 0.2 (moderate integral for backup heating)
#define DEFAULT_ELECTRIC_KI 0.2

// Kd: Derivative gain - Reduces overshoot from binary switching
//     Helps prevent excessive heating when temperature rises quickly
//     - INCREASE if: electric causes temperature spikes
//     - DECREASE if: electric turns off too early
//     Range: 0.0-5, Default: 1.0 (moderate damping)
#define DEFAULT_ELECTRIC_KD 1.0

// ===== PID Advanced Parameters =====
// These parameters affect both PIDs and should rarely need adjustment

// Integral max: Anti-windup limit - Prevents integral from growing unbounded
//     Limits the accumulated error to prevent overshoot when constraint is removed
//     - INCREASE if: system is slow to respond after constraint removal
//     - DECREASE if: system overshoots significantly after constraint removal
//     Range: 10-100, Default: 50.0
//     Note: Only adjust if you observe windup issues (large overshoots after
//           hydraulic heater is blocked by water temperature constraint)
#define DEFAULT_PID_INTEGRAL_MAX 50.0

// Derivative filter: Low-pass filter coefficient for derivative term
//     Reduces noise sensitivity in derivative calculation
//     0.0 = heavy filtering (smooth but slow), 1.0 = no filtering (fast but noisy)
//     - INCREASE if: control output is jittery/unstable
//     - DECREASE if: derivative term is too slow to react
//     Range: 0.01-0.5, Default: 0.1 (heavy filtering for noisy sensor)
//     Note: Only adjust if temperature sensor is very noisy or very stable
#define DEFAULT_PID_DERIVATIVE_FILTER 0.1

// Water temperature constraint for hydraulic heater
#define DEFAULT_WATER_TEMP_MARGIN 2.0 // °C - Minimum margin between water temp and air setpoint

// Phase parameters
#define DEFAULT_INIT_PHASE_DURATION 3600       // seconds (5-7200) = 1 hour
#define DEFAULT_EXTRACTION_PHASE_DURATION 120  // seconds (5-7200) = 2 minutes
#define DEFAULT_CIRCULATION_PHASE_DURATION 300 // seconds (5-3600) = 5 minutes

// Session parameters
#define DEFAULT_DRYING_SESSION_DURATION 172800 // seconds (3600-604800) = 48 hours

// Air recycling
#define DEFAULT_RECYCLING_RATE 50.0 // % (0-100)

#endif // CONFIG_H
