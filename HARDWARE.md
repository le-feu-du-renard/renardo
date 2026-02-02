WORK IN PROGRESS

# Hardware Components

This document details the hardware components used in the renard'o dehydrator controller.

## Microcontroller

- **Raspberry Pi Pico (RP2040)**
  - Dual-core ARM Cortex-M0+ processor
  - 264KB SRAM
  - 2MB Flash storage
  - Low cost and widely available
  - Excellent community support

## Sensors

### Temperature & Humidity Sensors (x2)
- **Purpose:** Chamber monitoring
- **Type:** DHT22 or similar I2C/digital sensors
- **Placement:**
  - One near the air intake
  - One near the air outlet
- **Specifications:**
  - Temperature range: -40°C to 80°C
  - Humidity range: 0-100% RH
  - Accuracy: ±0.5°C, ±2% RH

### Water Temperature Probe
- **Purpose:** Hydraulic loop monitoring
- **Type:** DS18B20 or similar 1-Wire waterproof sensor
- **Placement:** Attached to hydraulic radiator pipe
- **Specifications:**
  - Temperature range: -55°C to 125°C
  - Accuracy: ±0.5°C
  - Waterproof stainless steel probe

## Actuators

### Controllable Heating Circulator Pump
- **Purpose:** Hydraulic radiator circulation
- **Type:** 230V AC heating circulator pump with control input
- **Control:** PWM (0-100% duty cycle)
- **Interface:** Relay or PWM-compatible control module controlled by Pico
- **Flow rate:** Adjustable based on heating demand

### Electric Heating Element
- **Purpose:** Backup heating and rapid temperature adjustment
- **Type:** 230V AC resistive heating element (power rating depends on chamber size)
- **Control:** On/Off via solid-state relay (SSR)
- **Interface:** SSR controlled by Pico digital output
- **Safety:** Thermal fuse and overtemperature protection recommended

### Ventilation Fan
- **Purpose:** Air circulation and moisture extraction
- **Type:** 230V AC centrifugal fan
- **Capacity:** 400-800 m³/h per chamber
- **Control:** On/Off or variable speed (via relay or triac)
- **Interface:** Relay or speed controller controlled by Pico

### Motorized Air Dampers
- **Purpose:** Air recycling vs. fresh air intake control
- **Type:** 24V DC HVAC motorized damper actuators
- **Control:** 0-10V analog signal
- **Interface:** DAC or PWM-to-analog converter (0-10V output)
- **Positions:**
  - Full recycling (minimal fresh air)
  - Full renewal (maximum fresh air)
  - Intermediate positions for fine control

## Peripherals

### I2C Display
- **Purpose:** User interface and real-time monitoring
- **Type:** OLED or LCD display (128x64 or larger)
- **Interface:** I2C (SDA, SCL)
- **Recommended models:**
  - SSD1306 OLED (128x64)
  - SH1106 OLED (128x64)
  - LCD2004 with I2C adapter

### DS3231 RTC Module
- **Purpose:** Real-time clock for accurate timing and logging
- **Interface:** I2C
- **Features:**
  - Battery backup for timekeeping during power loss
  - Temperature-compensated crystal oscillator
  - High accuracy (±2ppm)
  - Alarm functions

### SPI MicroSD Card Module
- **Purpose:** Data logging and configuration storage
- **Interface:** SPI (MISO, MOSI, SCK, CS)
- **Card support:** MicroSD and MicroSDHC (up to 32GB)
- **File system:** FAT32
- **Uses:**
  - Session logs (CSV format)
  - Configuration files
  - Drying program presets

## Power Supply

### Main Power
- **Input:** 230V AC (or 110V depending on region)
- **Recommended:** Isolated power supplies for different voltage rails

### DC Rails

**5V Rail:**
- Raspberry Pi Pico (via USB or dedicated regulator)
- Logic level sensors
- Relays (control circuits)
- PWM control modules
- Current requirement: ~1-2A

**24V Rail:**
- Motorized air dampers only
- Current requirement: Depends on damper actuator specifications

### AC Rails

**230V AC:**
- Heating circulator pump
- Centrifugal ventilation fan
- Electric heating element
- **Note:** All high-voltage components must be properly isolated and controlled via relays or SSRs

<!-- TODO: Add wiring diagram -->
<!-- TODO: Add PCB design (if applicable) -->
<!-- TODO: Add assembly photos -->
