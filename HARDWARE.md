# Hardware — renard'o Dryer Controller

## Microcontroller

**Raspberry Pi Pico W (RP2040)**
- Dual-core ARM Cortex-M0+ @ 133 MHz
- 264 KB SRAM, 2 MB Flash
- WiFi 802.11 b/g/n (reserved for future use)
- 3.3 V logic, 5 V tolerant USB input

---

## GPIO Pin Assignment

| GPIO | Function | Direction |
|------|----------|-----------|
| 2  | SD Card SCK (SPI0) | OUT |
| 3  | SD Card MOSI (SPI0) | OUT |
| 4  | SD Card MISO (SPI0) | IN |
| 5  | SD Card CS (SPI0) | OUT |
| 6  | RS485 DE/RE (direction enable) | OUT |
| 7  | Circulator PWM (0-100%, boosted to 10 V via BC337) | OUT |
| 8  | RS485 TX (UART1) | OUT |
| 9  | RS485 RX (UART1) | IN |
| 10 | I2C Bus 2 SDA — RTC DS1307 | I2C |
| 11 | I2C Bus 2 SCL — RTC DS1307 | I2C |
| 12 | I2C Bus 1 SDA — MCP23017 | I2C |
| 13 | I2C Bus 1 SCL — MCP23017 | I2C |
| 14 | Relay — Electric heater (Fotek SSR) | OUT |
| 15 | Relay — Fan (Ruck 150) | OUT |
| 16 | Voltmeter CH1 — Inlet temperature | OUT (PWM) |
| 17 | Voltmeter CH2 — Inlet humidity | OUT (PWM) |
| 18 | Voltmeter CH3 — Total session duration | OUT (PWM) |
| 19 | Voltmeter CH4 — Current phase duration | OUT (PWM) |
| 20 | Button START (active LOW, internal pullup) | IN |
| 21 | Button STOP (active LOW, internal pullup) | IN |
| 22 | Mode selector ECO/PERF (LOW = ECO) | IN |
| 23 | LED — START button backlight (24 V via BC337) | OUT |
| 24 | LED — STOP button backlight (24 V via BC337) | OUT |
| 26 | ADC0 — Temperature potentiometer | IN (ADC) |
| 27 | ADC1 — Humidity potentiometer | IN (ADC) |
| 28 | Air damper — Belimo LM24A-SR (0/10 V via BC337) | OUT |

---

## MCP23017 LED Expander — Port A

I2C address: `0x20` — Bus 1 (GPIO 12/13)

Each MCP23017 output drives a BC337 transistor that switches a 24 V indicator LED (see [BC337 section](#bc337--npn-transistor-switch) below).

| Pin | Indicator | config.h constant |
|-----|-----------|-------------------|
| GPA0 | Mode PERFORMANCE active | `LED_PIN_MODE_PERF` |
| GPA1 | Electric heater active | `LED_PIN_ELECTRIC_HEATER` |
| GPA2 | Fan active | `LED_PIN_FAN` |
| GPA3 | Hydraulic heater active | `LED_PIN_HYDRO_HEATER` |
| GPA4 | Mode ECO active | `LED_PIN_ECO_MODE` |
| GPA5 | Phase Init | `LED_PIN_PHASE_INIT` |
| GPA6 | Phase Brassage | `LED_PIN_PHASE_BRASSAGE` |
| GPA7 | Phase Extraction | `LED_PIN_PHASE_EXTRACTION` |

---

## Sensors

### Temperature & Humidity — SHT30 RS485 (×2)

- **Protocol:** Modbus RTU, FC04 (Read Input Registers)
- **Register 0x0001:** Temperature — raw / 10.0 = °C
- **Register 0x0002:** Humidity — raw / 10.0 = %RH
- **Baud rate:** 9600
- **Modbus addresses:** Configurable via software (default: 1 = inlet, 2 = outlet)
- **Bus adapter:** MAX3485 TTL-to-RS485 module (×2, one per sensor)
- **Bus wiring:** UART1 (GPIO 8/9), direction control on GPIO 6 (DE/RE)
- **Placement:** one at air inlet, one at air outlet

### RTC — DS1307 + AT24C32 module

- **Interface:** I2C Bus 2 (GPIO 10/11), address `0x68`
- **Purpose:** Session file timestamping
- **Backup:** CR2032 coin cell (timekeeping during power loss)
- Note: The AT24C32 EEPROM on the same module is not used — session state is persisted via the Pico's internal EEPROM emulation.

---

## Actuators

### Hydraulic Circulator — OEG

- **Power:** 60 W
- **Flow rate:** 3.4 m³/h
- **Control:** 0-10 V analog signal
- **Interface:** Pico GPIO 7 outputs 3.3 V PWM → BC337 boosts to 10 V
- **Role:** Primary heater — proportional 0-100% power via PID

### Electric Heater — 600 W

- **Power:** 600 W
- **Control:** Binary ON/OFF via Fotek 40 A SSR
- **Interface:** GPIO 14 drives SSR control input directly (3.3 V compatible)
- **Role:** Supplement heater — binary ON/OFF via PID threshold (> 0.5 = ON)
- **Mode:** Disabled in ECO mode

### Fan — Ruck 150

- **Power:** 105 W
- **Airflow:** 760 m³/h
- **Control:** Binary ON/OFF via relay
- **Interface:** GPIO 15 drives relay coil
- **Behaviour:** ON whenever a session is active, OFF otherwise

### Air Damper — Belimo LM24A-SR

- **Supply:** 24 V AC/DC
- **Control input:** 0-10 V proportional (used here as binary: 0 V = closed, 10 V = open)
- **Spring return:** damper closes on power loss
- **Interface:** GPIO 28 → BC337 → switches between 0 V and 10 V
- **Logic:** humidity above threshold → open (evacuate moisture); below target → close

---

## Panel Interface

### Indicator LEDs — 24 V, ⌀16 mm (×8)

- **Voltage:** 24 V
- **Driver:** BC337 transistor per LED, controlled by MCP23017 Port A
- **Resistors:** R 2.2 kΩ on BC337 base, R 1.2 kΩ in series with LED
- **Mounting:** 16 mm panel cutout

### START / STOP Buttons — 24 V with integrated LED, ⌀16 mm (×2)

| Button | Color | Contact | GPIO contact | GPIO LED |
|--------|-------|---------|--------------|----------|
| START  | Green Light, 1NO | active LOW | 20 | 23 |
| STOP   | Red Light, 1NO   | active LOW | 21 | 24 |

- **Contact:** internal Pico pullup, debounced in software (50 ms)
- **LED:** 24 V via BC337 (R 2.2 kΩ base, R 1.2 kΩ series)
- **Behaviour:** START LED = session running; STOP LED = session idle

### Potentiometers — 10 kΩ (×2)

- **GPIO 26 (ADC0):** Target temperature — mapped to 20–45 °C
- **GPIO 27 (ADC1):** Humidity threshold — mapped to 0–100 %RH
- **Resolution:** 12-bit ADC (0–4095)

### Mode Selector — XB2-BD21

- **Type:** 2-position rotary selector
- **GPIO 22:** ECO (LOW) / PERFORMANCE (HIGH) — internal pullup
- **Position 1 (LOW):** ECO mode — hydraulic only, reduced target
- **Position 2 (HIGH):** PERFORMANCE mode — all heaters, full target

### Panel Voltmeters — 0–3 V, ⌀45 mm (×4)

Each GPIO drives an RC low-pass filter:
- **R:** 10 kΩ
- **C:** 100 µF 16 V electrolytic
- **fc ≈ 0.16 Hz** (τ ≈ 1 s) — fully suppresses 50 kHz PWM ripple, slow needle response for smooth reading
- **PWM:** 50 kHz, 12-bit — duty 0–91 % → 0–3.0 V on 3.3 V rail

| Channel | GPIO | Measurement | Full-scale |
|---------|------|-------------|------------|
| CH1 | 16 | Inlet temperature | `VOLTMETER_TEMP_MAX` °C |
| CH2 | 17 | Inlet humidity | `VOLTMETER_HUM_MAX` %RH |
| CH3 | 18 | Total session duration | `VOLTMETER_TOTAL_DUR_H` hours |
| CH4 | 19 | Current phase duration | `VOLTMETER_PHASE_DUR_MIN` minutes |

---

## SD Card

- **Interface:** SPI0 (GPIO 2/3/4/5)
- **Format:** FAT32
- **Usage:** Session CSV logs only — no configuration files
- **Directory structure:** `/sessions/YYYY/MM/YYMMXXXX.csv`

---

## Power Supply

| Rail | Consumers | Source |
|------|-----------|--------|
| 5 V | Pico W, RTC, MCP23017, SHT30, MAX3485 | Mean Well DIN rail PSU |
| 24 V | Belimo actuator, indicator LEDs (×8), button LEDs (×2) | Separate 24 V supply |
| 230 V AC | Electric heater (SSR), fan, circulator | Mains |

**Protection diodes — 1N4007** on 5 V and 24 V rails to prevent reverse current between supplies.

---

## BC337 — NPN Transistor Switch

The BC337 (NPN, TO-92, 45 V / 800 mA) is used throughout the design as a low-side switch to interface 3.3 V logic with higher-voltage loads. The Pico W and MCP23017 outputs are limited to 3.3 V — insufficient to directly drive 24 V LEDs or 10 V control signals.

**Wiring — indicator LED (common-emitter, low-side switch):**
```
24 V ──── R 1.2 kΩ ──── [LED] ────┐
                               Collector
                                 BC337
                               Base ──── R 2.2 kΩ ──── GPIO / MCP23017 (3.3 V)
                               Emitter
                                   |
                                  GND
```

**Wiring — PWM / binary signal boost (10 V):**
```
10 V ──── [Load: circulator or damper] ────┐
                                       Collector
                                         BC337
                                       Base ──── R 2.2 kΩ ──── GPIO (3.3 V)
                                       Emitter
                                           |
                                          GND
```

**Behaviour:**
- GPIO HIGH (3.3 V) → base current flows → transistor saturates → load ON
- GPIO LOW (0 V) → transistor off → load OFF

**Used for:**

| Application | Supply | Control source |
|-------------|--------|----------------|
| 8× indicator LEDs | 24 V | MCP23017 GPA0–GPA7 |
| START button backlight | 24 V | GPIO 23 |
| STOP button backlight | 24 V | GPIO 24 |
| Circulator PWM boost (3.3 V → 10 V) | 10 V | GPIO 7 |
| Air damper binary control (0 / 10 V) | 10 V | GPIO 28 |

> The common-emitter configuration inverts the signal (HIGH in → load ON, but collector pulls low). For the circulator PWM, the software compensates by inverting the duty cycle (`1.0 - duty`) before writing to the GPIO.

---

## Bill of Materials

| # | Component | Reference | Qty |
|---|-----------|-----------|-----|
| 1 | Raspberry Pi Pico W | RP2040 | 1 |
| 2 | SHT30 RS485 Modbus sensor | — | 2 |
| 3 | MAX3485 TTL-to-RS485 module | MAX3485 | 2 |
| 4 | MCP23017 I2C GPIO expander module | MCP23017 | 1 |
| 5 | RTC module | DS1307 + AT24C32 | 1 |
| 6 | MicroSD card module | SPI | 1 |
| 7 | Hydraulic circulator pump | OEG 60 W | 1 |
| 8 | Electric heater | 600 W | 1 |
| 9 | Solid-state relay | Fotek 40 A | 1 |
| 10 | Centrifugal fan | Ruck 150, 105 W | 1 |
| 11 | Air damper actuator | Belimo LM24A-SR | 1 |
| 12 | Panel voltmeter 0–3 V ⌀45 mm | — | 4 |
| 13 | Indicator LED 24 V ⌀16 mm | — | 8 |
| 14 | Button Green Light 1NO 24 V ⌀16 mm | — | 1 (START) |
| 15 | Button Red Light 1NO 24 V ⌀16 mm | — | 1 (STOP) |
| 16 | Rotary selector 2 positions | XB2-BD21 | 1 |
| 17 | Potentiometer 10 kΩ | — | 2 |
| 18 | NPN transistor | BC337 TO-92 | 11 |
| 19 | Resistor 2.2 kΩ (BC337 base) | — | 11 |
| 20 | Resistor 1.2 kΩ (LED series) | — | 10 |
| 21 | Resistor 10 kΩ (voltmeter RC filter) | — | 4 |
| 22 | Capacitor 100 µF 16 V electrolytic (RC filter) | — | 4 |
| 23 | Diode 1N4007 (power supply protection) | 1N4007 | 2 |
| 24 | Power supply 5 V DIN rail | Mean Well | 1 |
| 25 | Power supply 24 V | — | 1 |
