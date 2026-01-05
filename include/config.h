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

// LEDs
#define STOP_LED_PIN 20
#define START_LED_PIN 19
#define ELECTRIC_HEATER_LED_PIN 18

// ========== I2C ADDRESSES ==========
#define CHT8305_INLET_ADDR 0x40
#define CHT8305_OUTLET_ADDR 0x40  // Sur bus I2C différent
#define SSD1306_ADDR 0x3C
#define GP8403_DAC_ADDR 0x58

// ========== TIMING CONSTANTS ==========
#define SENSOR_UPDATE_INTERVAL 2000  // ms
#define DISPLAY_UPDATE_INTERVAL 100  // ms
#define CONTROL_LOOP_INTERVAL 1000   // ms

// ========== DRYER PARAMETERS ==========
#define TARGET_TEMPERATURE_DEFAULT 50.0  // °C
#define MAX_TEMPERATURE 80.0             // °C
#define MIN_TEMPERATURE 20.0             // °C

#endif // CONFIG_H
