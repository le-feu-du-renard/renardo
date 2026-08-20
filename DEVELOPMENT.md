# Development Guide

Technical reference for building, uploading, and debugging the renard'o dryer controller.

## Prerequisites

### Software

- [PlatformIO](https://platformio.org/) — as a VSCode extension or standalone CLI
- USB cable (data-capable, not charge-only)

### Hardware

- Raspberry Pi Pico H connected via USB
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
| `pico` | Raspberry Pi Pico H | Main firmware |
| `native` | Host machine (x86/x64) | Unit tests only |
| `pin_test` | Raspberry Pi Pico H | Display wiring, one signal at a time |
| `tft_test` | Raspberry Pi Pico H | Panel identification and test patterns |
| `encoder_test` | Raspberry Pi Pico H | Encoder edges, detents and click |
| `rs485_test` | Raspberry Pi Pico H | Modbus probes, address and baud sweeps |
| `extension_test` | A **second** Raspberry Pi Pico H | Plays both remote modules, extension and hydraulic |
| `damper_test` | Raspberry Pi Pico H | Damper command, relay and position feedback |
| `output_test` | Raspberry Pi Pico H | The three command outputs, one at a time |
| `panel_test` | Raspberry Pi Pico H | START/STOP buttons and the two status LEDs |
| `adc_test` | Raspberry Pi Pico H | One ADC pin as a raw count and as volts |

The bring-up sketches live in [bringup/](bringup/), outside `src/`, so the
firmware build needs no exclusion list and nothing that only exists to be
uploaded by hand sits among the production sources. Each one has its own `main`
and pulls in the handful of production files it exercises, named in its
`build_src_filter`. What each proves, and how to read what it prints, is in
[HARDWARE.md](HARDWARE.md) beside the part it tests.

---

## Upload

### First upload (or after hardware change)

1. Hold **BOOTSEL** while connecting USB — Pico appears as a mass storage device
2. Upload firmware:

```bash
pio run -t upload -e pico
```

### Normal upload

```bash
# Upload and open serial monitor
pio run -t upload -t monitor -e pico
```

> Settings live in a LittleFS partition on internal flash, not in the firmware
> image, so they survive a re-flash. `config.h` only supplies factory defaults.
> To wipe them, use Système → Réinit. usine in the menu, or erase the flash
> entirely with a `flash_nuke.uf2`.

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
[INFO ] SettingsStore: LittleFS mounted
[WARN ] ModbusSensors: read failed @1 (error 0xE2, count 1)
[ERROR] Inlet probe silent for 10000 ms — heating disabled
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

# Run a specific suite
pio test -e native -f "unit/test_heating_controller"
```

Suites live in `test/unit/`, one directory per binary, using Unity.

Tests compile the **real** production sources rather than a copy of the logic:
each suite has a `production_impl.cpp` that includes the `.cpp` files under
test. `test/unit/support/Arduino.h` supplies a shim with a clock the tests drive
explicitly, and `Logger` compiles to no-ops when `ARDUINO` is undefined.

That matters: the v3 suite re-implemented the control logic in the test file,
and its copy had already drifted from `config.h` — it was passing against logic
the firmware no longer ran. Anything hardware-free belongs in a class the tests
can compile, which is why the display model and the menu are both separated from
their hardware.

---

## Screen fonts

The interface's typefaces live in `include/fonts/` as generated `GFXfont`
headers. They are committed, so **the firmware builds without Python** and the
tool below is only needed to change the type.

```bash
python3 tools/make_gfx_font.py           # regenerate every cut
python3 tools/make_gfx_font.py --proof   # and write tools/font-proof.png
```

The script rasterises `~/Library/Fonts/HackNerdFontMono-*.ttf` with Pillow. It
needs the font installed and `python3 -m pip install Pillow`. Check the proof
sheet after changing a size: below about 10 px the glyphs break up, and the
failure is a mangled `N`, not an error.

`-D LOAD_GFXFF=1` in `platformio.ini` is what makes TFT_eSPI accept them. The
built-in fonts stay loaded — the splash screen draws with them, because it has
to be able to report that everything loaded after it failed.

---

## Configuration

`include/config.h` holds the pin map and the **factory defaults**. Anything
reachable from the menu is persisted in LittleFS and overrides the default at
boot, so changing a constant only affects a board whose settings were never
saved, or one reset from Système → Réinit. usine.

| Section | What it controls |
|---------|-----------------|
| Pin assignments | GPIO numbers for every peripheral |
| Modbus register map | probe and hydraulic-module addresses and registers |
| Output polarity | `OUT_*_ACTIVE_LOW`, one per output |
| Damper calibration | end-stop ADC defaults and tolerance |
| Phase durations | Init / Brassage / Extraction defaults |
| Control parameters | hysteresis bands, horizons, anti-short-cycle timers |

> The TFT pins are duplicated in the `TFT_eSPI` build flags in
> `platformio.ini`. Change both together.

---

## Common Issues

### Upload fails

- Hold BOOTSEL while plugging USB
- Check the USB cable supports data transfer
- Try a different USB port

### Serial monitor shows garbage

- Verify baud rate: 115200
- Re-open monitor after upload completes

### Display blank or corrupted

- Check the TFT pins in `platformio.ini` match `config.h`
- Inverted colours or an offset image is the usual ST7789 variant question —
  try `TFT_INVERSION_ON` or a column/row offset
- The panel owns SPI0 alone and nothing else is allowed on it, so a redraw
  cannot be colliding with another master; corruption that coincides with RS485
  traffic points at the power rail rather than at the bus

### Sensors not responding

- Check RS485 wiring (GPIO 4/5) and the DE pin (GPIO 3)
- Verify the Modbus address matches `MODBUS_INLET_ADDRESS`
- Check baud rate: 9600
- A silent inlet probe blocks all heating after `SENSOR_TIMEOUT_MS` and shows
  `SONDE` in the status bar — that is the interlock working, not a bug

### Heating never starts

Work through the interlocks in order: the fan has to be running, the inlet
probe fresh, the source enabled in the menu, and for the hydraulic, the module
answering on RS485. The periodic `TempMgr:` log line reports which sources are
in play.

### An output is on at boot

Polarity. The fan and the electric heating must be wired active HIGH so a
floating GPIO leaves them off during the boot window — see HARDWARE.md.

### Watchdog resets

- A reset is logged as `!!! Recovered from watchdog reset !!!` on next boot
- Timeout is 8 seconds — if the main loop blocks longer than that, the Pico reboots
- Check for blocking I2C or SPI calls
