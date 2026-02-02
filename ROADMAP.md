# Roadmap

This document outlines planned improvements, features, and bug fixes for the renard'o dehydrator controller.

## Current Issues

### High Priority

- [ ] Review manual program shortcut on display
- [ ] Implement proper shutdown procedure - ensure electric heating element cools down before power off
- [ ] Fix state management and EEPROM persistence
- [ ] Migrate loggers + document how to launch with different debug levels

## User Interface Improvements

### Display

- [ ] Replace current font with more legible alternative
- [ ] Add proper degree symbol (°) character
- [ ] Improve menu system:
  - Add units to parameter displays
  - Convert certain parameters to hour-based format
- [ ] Add splash screen on startup
- [ ] Add startup UI status indicators (SD card, RTC, sensors) for visual system health check
- [ ] Display max temperature and max humidity based on active program and overrides
- [ ] Add IN/OUT icons to differentiate sensor values (intake vs. outlet)

### System Display

- [ ] Show current operating mode on screen (Eco/Hybrid/Performance)
- [ ] Allow mode switching through configuration menu

## Core Features

### Heating Management

- [ ] Add air renewal control in heater manager when temperature exceeds thresholds
- [ ] Make cycle duration configurable via parameters
- [ ] Implement three operating modes:
  - **Eco Mode** - Hydraulic heating only; reduces targets if insufficient energy to preserve hydraulic system
  - **Hybrid Mode** - Uses solar/hydraulic first, then electric with reduced targets
  - **Performance Mode** - Maintains target setpoints regardless of energy source

### Hardware Control

- [ ] Add circulator pump configuration options:
  - Minimum/maximum speed
  - Cycle frequency
  - PWM parameters
- [ ] Enable configurable heating strategy:
  - Support multiple heater configurations
  - Consider impact on display representation
- [ ] Add relay control for optional dehumidifier module

## Configuration & Settings

- [ ] Add system menu for RTC time/date adjustment
- [ ] Implement dual-core support for:
  - SD card logging on second core
  - EEPROM save operations
  - Message queue system for inter-core communication

## Future Features

### Connectivity

- [ ] WiFi access point mode for data retrieval
- [ ] Historical data mode - browse and download past drying sessions

### Advanced Features

- [ ] Multi-chamber support with independent control
- [ ] Recipe library and sharing
- [ ] Automatic program recommendations based on food type

## Documentation

- [ ] Create comprehensive user documentation
- [ ] Add Bill of Materials (BOM) with part numbers and sources
- [ ] Create wiring schematics and diagrams
- [ ] Add assembly instructions with photos

## Testing & Quality

### Code Quality

- [ ] Establish and enforce coding style guidelines
- [ ] Increase test coverage (target: >80% for critical components)
- [ ] Add integration test scenarios:
  - Complete drying cycle simulation
  - Error condition handling
  - Mode switching behavior
  - Sensor failure scenarios

### Hardware Testing

- [ ] Create device test suite to validate all functionalities:
  - All sensors reading correctly
  - All actuators responding
  - Display rendering properly
  - RTC keeping accurate time
  - SD card writing successfully