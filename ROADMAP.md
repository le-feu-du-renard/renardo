# Roadmap

Planned improvements for the renard'o dryer controller.

---

## In Progress

- [ ] Hardware validation — first full power-on test with all peripherals

---

## Short Term

### Sensors & Control

- [ ] Validate SHT30 Modbus readings against reference thermometer/hygrometer
- [ ] Fine-tune PID gains (Kp/Ki/Kd) based on real thermal behaviour of the chamber
- [ ] Validate ECO mode temperature reduction in practice
- [ ] Validate Belimo damper binary control (0 V / 10 V switching)

### Data Logging

- [ ] Validate CSV output format and SD card directory structure on real hardware
- [ ] Test SD card hot-plug recovery (RetryInitialization path)

### Panel Interface

- [ ] Validate voltmeter calibration (duty cycle → voltage at 3.3 V rail)
- [ ] Validate potentiometer ADC mapping (12-bit range → °C / %RH)
- [ ] Validate all 8 indicator LEDs and button backlights (24 V / BC337 circuit)

---

## Medium Term

### Reliability

- [ ] Add sensor failure detection: if a Modbus read fails for N consecutive cycles, flag it in the logs and stop the heaters
- [ ] Add thermal runaway protection: if temperature exceeds target + safety margin, cut all heaters regardless of PID output
- [ ] Evaluate adding a fuse on the 24 V rail for LED/button protection

### Logging & Analysis

- [ ] Add a post-session summary line at the end of each CSV file (total duration, average temperature, average humidity)
- [ ] Build a simple Python script to plot CSV session data

### Configuration

- [ ] Consider exposing phase durations as potentiometer-adjustable at startup (hold START during boot)

---

## Future

### Connectivity (WiFi — Pico W)

- [ ] WiFi access point mode for log file retrieval without SD card removal
- [ ] Simple web interface showing live sensor data and current phase
- [ ] OTA firmware update

### Hardware Evolution

- [ ] Evaluate adding a second fan for forced air circulation (independent of extraction fan)
- [ ] Evaluate adding a dehumidifier module on a spare relay

---

## Completed

- [x] Migrate all sensors to Modbus RS485 (SHT30) — removed DHT22 / DS18B20
- [x] Replace OLED + menu system with physical panel interface
- [x] Replace proportional air damper (DAC 0-10V) with binary damper (BC337 transistor)
- [x] Add MCP23017 I2C expander for 8× 24 V indicator LEDs
- [x] Add 4× panel voltmeters (PWM + RC, 0-3 V)
- [x] Add START/STOP buttons with 24 V integrated LED
- [x] Add temperature + humidity potentiometers (ADC)
- [x] Add ECO/PERFORMANCE physical mode selector
- [x] Simplify session to fixed 3-phase cycle (Init → Brassage → Extraction)
- [x] Remove program/cycle/preset system
- [x] EEPROM persistence for session state (PersistentStateManager)
- [x] All configuration as compile-time constants in config.h
- [x] Migrate all Serial.print to Logger (ArduinoLog wrapper)
- [x] Establish coding guidelines (2-space indent, Allman braces, English comments)
- [x] Hardware watchdog (8 s timeout)
