# renard'o - Professional Hybrid Food Dehydrator Controller

A high-performance dehydration control system built for the Raspberry Pi Pico. This project implements a sophisticated hybrid heating strategy and intelligent airflow management to achieve precise drying cycles for various food types.

<!-- TODO: Add project image/photo here -->

## Project Philosophy

This project follows a **low-tech** approach: sustainable, repairable, and accessible technology. The controller is designed to be:

- **Affordable** - Based on standard, low-cost components (Raspberry Pi Pico, common sensors)
- **Energy Frugal** - Designed to minimize energy consumption through intelligent operating modes and hydraulic heating priority
- **Modular** - Flexible architecture allowing easy adaptation or replacement of components according to your needs
- **Versatile** - Capable of drying a wide variety of products (fruits, vegetables, meats, herbs, mushrooms...)
- **Easy to Use** - Intuitive interface with display and navigation menu for all users

The goal is to create a professional-grade dehydration system accessible to individuals, small farms, and artisans, while maintaining industrial equipment performance and consuming as little energy as possible.

## Overview

This controller manages a hybrid heating environment using both hydraulic and electric heating elements. It features a custom hybrid PID controller to maintain stable temperatures while optimizing energy consumption between the water-based radiator and electric backup heater.

The system is designed for professional-grade food dehydration with precise temperature control, adaptive airflow management, and comprehensive data logging capabilities.

<!-- TODO: Add system architecture diagram here -->

## Key Features

- **Hybrid PID Temperature Control** - Advanced control logic to balance a hydraulic radiator (circulator pump) and an electric heater for optimal energy efficiency
- **Two Operating Modes (ECO & PERFORMANCE)** - ECO mode implements night mode (20h-10h by default) with reduced target temperature (85% by default) for extended, energy-efficient drying using hydraulic heating only. PERFORMANCE mode maintains full target temperature 24/7 using all available heaters for maximum drying speed
- **Airflow & Ventilation Management** - Dynamic control of ventilation and motorized air dampers to manage air recycling vs. fresh air intake
- **Multi-Phase Drying Programs** - Support for complex drying profiles with different temperature and humidity setpoints over time
- **Smart Presets** - Built-in programs tailored for different food categories
- **Data Logging** - Full session logging to SD card for process analysis and quality control
- **Real-Time Monitoring** - On-screen display of live sensor data, active phase, and system status
- **Persistent Settings** - Integrated RTC for precise timing and interactive menu to adjust parameters on the fly

## Hardware

For detailed information about the hardware components, wiring, and assembly, see [HARDWARE.md](HARDWARE.md).

## Applications

This dehydration system is suitable for:

- **Herbs** - Gentle drying preserving aromas
- **Fruits and vegetables** - Harvest preservation, healthy snacks
- **Mushrooms** - Drying for long-term storage
- **Flowers and plants** - Herbalism, herbal teas
- **Artisan preparations** - Fruit leathers, raw crackers, etc.

## For Developers

If you want to contribute to the project, modify the code, or compile the firmware, see the [DEVELOPMENT.md](DEVELOPMENT.md) guide.

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE-GPL-3.0) file for details.

---

*Open-source project - Contributions welcome*
