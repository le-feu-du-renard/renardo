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

## Project Structure

```
dryer/
├── src/                 # Main source code
│   ├── main.cpp        # Application entry point
│   ├── pid/            # PID controller implementation
│   ├── sensors/        # Sensor drivers
│   └── actuators/      # Actuator control
├── include/            # Header files
├── test/               # Unit tests
│   ├── test_pid/       # PID controller tests
│   └── test_sensors/   # Sensor tests
├── data/               # Filesystem data
│   ├── fonts/          # Display fonts
│   └── config/         # Configuration files
├── lib/                # Project libraries
├── platformio.ini      # PlatformIO configuration
└── README.md           # Project overview
```

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
5. **Place in data directory** - Copy to `data/fonts/`
6. **Upload filesystem** - Run `pio run -t uploadfs -e pico_w`

### Font Organization

```
data/fonts/
├── display_small.h      # Small font for detailed info
├── display_medium.h     # Medium font for regular text
└── display_large.h      # Large font for main values
```

## Usage and Testing

### First-Time Setup

1. **Power on the system** - Connect the Raspberry Pi Pico to power
2. **Set date/time** - Use the menu interface to configure the RTC
3. **Calibrate sensors** - Verify temperature and humidity readings
4. **Configure programs** - Select or customize drying programs for your needs

### Starting a Drying Cycle

1. Load your food into the dehydrator
2. Select a preset program or create a custom profile
3. Start the cycle from the menu
4. Monitor progress on the display

### Data Logs

Session logs are automatically saved to the SD card in CSV format for later analysis. Each log includes:
- Timestamp
- Temperature readings (chamber and water)
- Humidity levels
- Active phase information
- Actuator states

This data allows you to optimize your drying recipes and ensure complete process traceability.

**Log file location:** SD card root directory, named with timestamp (e.g., `log_2024-01-15_14-30-00.csv`)

## Code Style Guidelines

- **C++ Standard:** C++17
- **Indentation:** 2 spaces (no tabs)
- **Naming:**
  - Classes/Structs: `PascalCase`
  - Functions/Methods: `camelCase`
  - Variables: `snake_case`
  - Constants: `UPPER_CASE`
- **Comments:** Use `//` for single line, `/* */` for blocks
- **Include guards:** Use `#pragma once`

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

### Build Errors

- Run `pio run -t clean` and rebuild
- Update PlatformIO: `pio upgrade`
- Check `platformio.ini` for correct library versions

## Contributing

### Workflow

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes with clear commit messages
4. Add tests for new functionality
5. Ensure all tests pass: `pio test -e native`
6. Submit a pull request

### Pull Request Checklist

- [ ] Code follows style guidelines
- [ ] All tests pass
- [ ] New features include tests
- [ ] Documentation updated (if needed)
- [ ] Commit messages are clear and descriptive

## Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Arduino-Pico Core](https://github.com/earlephilhower/arduino-pico)
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)

## License

[Add your license here]
