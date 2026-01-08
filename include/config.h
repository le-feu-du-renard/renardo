#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== PINS CONFIGURATION ==========

// I2C Bus 1 (OLED + Inlet Air Sensor)
#define I2C_BUS_1_SDA_PIN 12
#define I2C_BUS_1_SCL_PIN 13

// I2C Bus 2 (DAC + Outlet Air Sensor)
#define I2C_BUS_2_SDA_PIN 10
#define I2C_BUS_2_SCL_PIN 11

// Buttons
#define BUTTON_START_PIN 21

// Rotary Encoder
#define ROTARY_ENCODER_SW_PIN 7
#define ROTARY_ENCODER_CLK_PIN 8
#define ROTARY_ENCODER_DT_PIN 9

// Outputs
#define ELECTRIC_HEATER_RELAY_PIN 14
#define FAN_RELAY_PIN 15
#define WATER_CIRCULATOR_PWM_PIN 5

// OneWire (Water Temperature)
#define WATER_SENSOR_PIN 6

// Screen
#define SCREEN_SSD1306_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_OLED_RESET -1

// DAC
#define DAC_GP8403_ADDR 0x58

// ========== I2C ADDRESSES ==========
#define CHT8305_INLET_ADDR 0x40
#define CHT8305_OUTLET_ADDR 0x40

// ========== TIMING CONSTANTS ==========
#define SENSOR_UPDATE_INTERVAL 2000  // ms
#define DISPLAY_UPDATE_INTERVAL 100  // ms
#define CONTROL_LOOP_INTERVAL 1000   // ms
#define SETTINGS_SAVE_INTERVAL 60000 // ms (60 seconds)

// ========== DRYER DEFAULT PARAMETERS ==========

// Heating parameters
#define DEFAULT_TEMPERATURE_TARGET 50.0      // °C (20-80)
#define DEFAULT_TEMPERATURE_DEADBAND 2.0     // °C (0.5-10)
#define DEFAULT_HEATING_ACTION_MIN_WAIT 10.0 // seconds (5-120)
#define DEFAULT_HEATER_STEP_MIN 0.05         // ratio (0.01-0.5)
#define DEFAULT_HEATER_STEP_MAX 0.2          // ratio (0.05-1.0)
#define DEFAULT_HEATER_FULL_SCALE_DELTA 10.0 // °C (5-30)

// Phase parameters
#define DEFAULT_INIT_PHASE_DURATION 3600       // seconds (5-7200) = 1 hour
#define DEFAULT_EXTRACTION_PHASE_DURATION 120  // seconds (5-7200) = 2 minutes
#define DEFAULT_CIRCULATION_PHASE_DURATION 300 // seconds (5-3600) = 5 minutes

// Session parameters
#define DEFAULT_DRYING_SESSION_DURATION 172800 // seconds (3600-604800) = 48 hours

// Air recycling
#define DEFAULT_RECYCLING_RATE 50.0 // % (0-100)

#endif // CONFIG_H
