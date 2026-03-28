# Development Guide

Technical reference for building, uploading, and debugging the renard'o dryer controller.

## Prerequisites

### Software

- [PlatformIO](https://platformio.org/) — as a VSCode extension or standalone CLI
- USB cable (data-capable, not charge-only)

### Hardware

- Raspberry Pi Pico W connected via USB
- Serial terminal for log output (included with PlatformIO)

---

## Build

```bash
# Standard build
pio run

# Clean build
pio run -t clean && pio run
```

Environments defined in `platformio.ini`:

| Environment | Target | Use |
|-------------|--------|-----|
| `pico_w` | Raspberry Pi Pico W | Main firmware |
| `native` | Host machine (x86/x64) | Unit tests only |

---

## Upload

### First upload (or after hardware change)

1. Hold **BOOTSEL** while connecting USB — Pico appears as a mass storage device
2. Upload firmware:

```bash
pio run -t upload -e pico_w
```

### Normal upload

```bash
# Upload and open serial monitor
pio run -t upload -t monitor -e pico_w
```

> There is no filesystem to upload — the SD card is used for session logs only, and all configuration is in `config.h`.

---

## Serial Monitor

```bash
# Open monitor (115200 baud)
pio device monitor

# Or with explicit baud rate
pio device monitor -b 115200
```

Log output uses [ArduinoLog](https://github.com/thijse/Arduino-Log) via the `Logger` wrapper:

```
[INFO ] SessionMonitor: SD card initialized successfully
[WARN ] Inlet sensor read failed (errors=1)
[ERROR] PersistentStateManager: EEPROM commit failed
```

### Log levels

Controlled in `setup()` via `Logger::Init(level)`:

| Level | Constant | Output |
|-------|----------|--------|
| 0 | `LOG_LEVEL_SILENT` | Nothing |
| 1 | `LOG_LEVEL_FATAL` | Fatal errors only |
| 2 | `LOG_LEVEL_ERROR` | Errors |
| 3 | `LOG_LEVEL_WARNING` | Warnings + errors |
| 4 | `LOG_LEVEL_NOTICE` | `Logger::Info()` calls |
| 5 | `LOG_LEVEL_TRACE` | — |
| 6 | `LOG_LEVEL_VERBOSE` | `Logger::Debug()` calls |

Default: `LOG_LEVEL_VERBOSE` (all messages visible).

---

## Unit Tests

Tests run on the host machine without hardware:

```bash
# Run all tests
pio test -vvv -e native

# Run a specific test file
pio test -e native -f test_pid
```

Test files live in `test/`. The Unity framework is used.

---

## Configuration

All hardware parameters are in `include/config.h`. Change a value and rebuild — no reflashing of a filesystem needed.

Key sections:

| Section | What it controls |
|---------|-----------------|
| Pin assignments | GPIO numbers for every peripheral |
| I2C addresses | MCP23017, RTC |
| LED pin mapping | Which GPA bit drives which indicator |
| Voltmeter channel mapping | Which GPIO drives which voltmeter |
| Modbus register map | SHT30 register addresses and scale |
| Phase durations | Init / Brassage / Extraction timings |
| PID gains | Hydraulic and electric PID parameters |
| Potentiometer ranges | ADC → physical unit mapping |
| Voltmeter ranges | Full-scale value per channel |

---

## Common Issues

### Upload fails

- Hold BOOTSEL while plugging USB
- Check the USB cable supports data transfer
- Try a different USB port

### Serial monitor shows garbage

- Verify baud rate: 115200
- Re-open monitor after upload completes

### SD card not detected

- Verify FAT32 format
- Check SPI wiring (GPIO 2/3/4/5)
- Inspect log: `SessionMonitor: SD card initialization failed`

### Sensors not responding

- Check RS485 wiring (GPIO 8/9) and DE pin (GPIO 6)
- Verify Modbus addresses match `MODBUS_INLET_ADDRESS` / `MODBUS_OUTLET_ADDRESS` in `config.h`
- Check baud rate: 9600

### Watchdog resets

- A reset is logged as `!!! Recovered from watchdog reset !!!` on next boot
- Timeout is 8 seconds — if the main loop blocks longer than that, the Pico reboots
- Check for blocking I2C or SPI calls
