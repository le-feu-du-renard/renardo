WORK IN PROGRESS

# Development Guide

This guide contains technical information for developers who want to build, modify, or contribute to the dehydrator controller project.

## Prerequisites

### Required Software

- [PlatformIO](https://platformio.org/) - Development platform for embedded systems
  - Can be installed as a VSCode extension or standalone CLI
- USB cable for connecting the Raspberry Pi Pico
- Serial terminal for monitoring (included with PlatformIO)

### Hardware Setup

Before development, ensure you have:
- Raspberry Pi Pico properly connected
- All sensors and actuators wired according to the hardware schematic
- Power supply capable of handling the heating elements

## Building the Project

### Standard Build

```bash
# Build all targets
pio run

# Build specific environment
pio run -e pico_w
```

### Clean Build

```bash
# Clean build artifacts
pio run -t clean

# Clean and rebuild
pio run -t clean && pio run
```

## Uploading to the Pico

### Initial Setup

1. **Connect the Pico** - Hold the BOOTSEL button while connecting USB
2. **Upload filesystem first** - This contains fonts, configs, etc.

```bash
# Upload filesystem (data/ directory)
pio run -t uploadfs -e pico_w
```

3. **Upload firmware** - Main application code

```bash
# Upload firmware and start serial monitor
pio run -t upload -t monitor -e pico_w
```

### Development Workflow

For faster iteration during development:

```bash
# Upload and monitor in one command
pio run -t upload -t monitor -e pico_w

# Exit monitor: Ctrl+C
```

### Hydraulic System Testing

A special test environment is available for testing the hydraulic control system:

```bash
# Upload and run hydraulic tests
pio run -t upload -t monitor -e test_hydraulic
```

## Testing

### Unit Tests

Unit tests run on your local machine (native environment) for rapid testing without hardware:

```bash
# Run all tests with verbose output
pio test -vvv -e native

# Run specific test
pio test -e native -f test_pid

# Run tests with coverage (if configured)
pio test -e native --coverage
```

### Hardware Tests

Some tests require actual hardware and run on the Pico:

```bash
# Upload and run hardware tests
pio test -e pico_w
```

### Test Structure

- `test/` directory contains all test files
- Tests use the Unity testing framework
- Each module should have corresponding tests
- Aim for >80% code coverage on critical components (PID, safety logic)

## Building for Different Targets

The project supports multiple build environments:

### Pico W (Main Target)

```bash
# Standard Pico W build with WiFi support
pio run -e pico_w
pio run -t upload -e pico_w
```

### Hydraulic Test Build

```bash
# Specialized build for testing hydraulic system
pio run -e test_hydraulic
pio run -t upload -t monitor -e test_hydraulic
```

### Native (x86/x64)

```bash
# Build for local machine (for unit tests)
pio run -e native

# Run the native executable
.pio/build/native/program
```

## Generating Custom Fonts

The display uses bitmap fonts generated from TrueType fonts.

### Steps

1. **Select a TrueType font** - Choose a .ttf file suitable for embedded displays
2. **Use the converter tool** - https://rop.nl/truetype2gfx/
3. **Configure settings:**
   - Font size: Based on your display resolution
   - Character set: Include only needed characters to save memory
   - Output format: Adafruit GFX format
4. **Download generated files** - Usually `.h` files
5. **Place in data directory** - Copy to `assets/fonts/`
6. **Upload filesystem** - Run `pio run -t uploadfs -e pico_w`

## Debugging

### Serial Output

```bash
# Monitor serial output
pio device monitor

# Monitor with specific baud rate
pio device monitor -b 115200
```

### Debug Logging

The project includes debug macros:

```cpp
DEBUG_PRINT("Temperature: ");
DEBUG_PRINTLN(temperature);
```

Enable/disable in `platformio.ini`:

```ini
build_flags =
    -DDEBUG_ENABLED=1
```

## Common Issues

### Upload Fails

- Ensure Pico is in BOOTSEL mode (hold button while connecting)
- Check USB cable supports data (not just power)
- Try a different USB port

### Serial Monitor Shows Garbage

- Check baud rate matches (default: 115200)
- Ensure correct port is selected