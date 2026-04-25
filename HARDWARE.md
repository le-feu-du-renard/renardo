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
| 0  | SD Card MISO (SPI0 RX) | IN |
| 1  | SD Card CS (SPI0) | OUT |
| 2  | SD Card SCK (SPI0) | OUT |
| 3  | SD Card MOSI (SPI0 TX) | OUT |
| 4  | TM1637 CLK — session duration display | OUT |
| 5  | TM1637 DIO — session duration display | OUT |
| 9  | Air damper relay — Belimo LM24A-SR | OUT |
| 10 | I2C Bus SDA — MCP23017 + RTC DS1307 | I2C |
| 11 | I2C Bus SCL — MCP23017 + RTC DS1307 | I2C |
| 12 | RS485 TX (UART1 → MAX3485 DI) | OUT |
| 13 | RS485 RX (UART1 ← MAX3485 RO) | IN |
| 14 | RS485 DE/RE (HIGH = transmit, LOW = receive) | OUT |
| 15 | Circulator PWM (0–100%, boosted to 10 V via BC337) | OUT (PWM) |
| 16 | Button START (active LOW, internal pullup) | IN |
| 17 | Button STOP (active LOW, internal pullup) | IN |
| 18 | Voltmeter CH1 — Inlet humidity | OUT (PWM) |
| 19 | Voltmeter CH2 — Inlet temperature | OUT (PWM) |
| 20 | Voltmeter CH3 — Outlet temperature | OUT (PWM) |
| 21 | Voltmeter CH4 — Outlet humidity | OUT (PWM) |
| 22 | Mode selector ECO/PERF (LOW = ECO) | IN |
| 26 | ADC0 — Temperature potentiometer | IN (ADC) |
| 27 | ADC1 — Humidity potentiometer | IN (ADC) |

---

## MCP23017 LED Expander

I2C address: `0x20` — shared bus with RTC (GPIO 10/11)

Each MCP23017 output drives a BC337 transistor that switches a 24 V indicator LED or relay.

### Port A — Indicator LEDs

| Pin | Indicator | config.h constant |
|-----|-----------|-------------------|
| GPA0 | Electric heater active | `MCP_LED_HEATER` |
| GPA1 | Hydraulic heater active | `MCP_LED_HYDRO_HEATER` |
| GPA2 | Fan active | `MCP_LED_FAN` |
| GPA3 | Air renewal active | `MCP_LED_AIR_RENEWAL` |
| GPA4 | Phase Extraction | `MCP_LED_PHASE_EXTRACTION` |
| GPA5 | Phase Brassage | `MCP_LED_PHASE_BRASSAGE` |
| GPA6 | Phase Init | `MCP_LED_PHASE_INIT` |
| GPA7 | Mode ECO active | `MCP_LED_ECO_MODE` |

### Port B — Relays and button LEDs

| Pin | Function | config.h constant |
|-----|----------|-------------------|
| GPB1 | Electric heater relay (Fotek SSR) | `MCP_HEATER_RELAY` |
| GPB2 | Fan relay (Ruck 150) | `MCP_FAN_RELAY` |
| GPB6 | LED — STOP button backlight | `MCP_BTN_STOP_LED` |
| GPB7 | LED — START button backlight | `MCP_BTN_START_LED` |

---

## Sensors

### Temperature & Humidity — SHT30 RS485 (×2)

- **Protocol:** Modbus RTU, FC03 (Read Holding Registers)
- **Register 0x0000:** Humidity — raw / 10.0 = %RH
- **Register 0x0001:** Temperature — raw / 10.0 = °C
- **Baud rate:** 9600
- **Modbus addresses:** 1 = inlet, 2 = outlet
- **Bus adapter:** MAX3485 TTL-to-RS485 module
- **Bus wiring:** UART1 (GPIO 12/13), direction control on GPIO 14 (DE/RE)
- **Placement:** one at air inlet, one at air outlet

### RTC — DS1307 + AT24C32 module

- **Interface:** I2C (GPIO 10/11), address `0x68`
- **Purpose:** Session file timestamping
- **Backup:** CR2032 coin cell (timekeeping during power loss)
- Note: The AT24C32 EEPROM on the same module is not used — session state is persisted via the Pico's internal EEPROM emulation.

---

## Actuators

### Hydraulic Circulator — OEG

- **Power:** 60 W
- **Flow rate:** 3.4 m³/h
- **Control:** 0–10 V analog signal
- **Interface:** Pico GPIO 15 outputs 3.3 V PWM → BC337 boosts to 10 V
- **Role:** Primary heater — proportional 0–100% power via PID

### Electric Heater — 600 W

- **Power:** 600 W
- **Control:** Binary ON/OFF via Fotek 40 A SSR
- **Interface:** MCP23017 GPB1 drives SSR control input via BC337
- **Role:** Supplement heater — binary ON/OFF via PID threshold (> 0.5 = ON)
- **Mode:** Disabled in ECO mode

### Fan — Ruck 150

- **Power:** 105 W
- **Airflow:** 760 m³/h
- **Control:** Binary ON/OFF via relay
- **Interface:** MCP23017 GPB2 drives relay coil via BC337
- **Behaviour:** ON whenever a session is active, OFF otherwise

### Air Damper — Belimo LM24A-SR

- **Supply:** 24 V AC/DC
- **Control input:** 0–10 V proportional (used here as binary: 0 V = closed, 10 V = open)
- **Spring return:** damper closes on power loss
- **Interface:** GPIO 9 controls a relay — NC contact open = 0 V (closed), NC contact closed = 10 V (open)
- **Logic:** humidity above threshold → open (evacuate moisture); below target → close

---

## Panel Interface

### Indicator LEDs — 24 V, ⌀16 mm (×8)

- **Voltage:** 24 V
- **Driver:** BC337 transistor per LED, controlled by MCP23017 Port A
- **Resistors:** R 2.2 kΩ on BC337 base, R 1.2 kΩ in series with LED
- **Mounting:** 16 mm panel cutout

### START / STOP Buttons — 24 V with integrated LED, ⌀16 mm (×2)

| Button | Color | Contact GPIO | LED (MCP23017) |
|--------|-------|-------------|-----------------|
| START  | Green Light, 1NO | GPIO 16 | GPB7 (`MCP_BTN_START_LED`) |
| STOP   | Red Light, 1NO   | GPIO 17 | GPB6 (`MCP_BTN_STOP_LED`) |

- **Contact:** internal Pico pullup, debounced in software (50 ms)
- **LED:** 24 V via BC337 driven by MCP23017 GPB6/GPB7
- **Behaviour:** START LED = session running; STOP LED = session idle

### Potentiometers — 10 kΩ (×2)

- **GPIO 26 (ADC0):** Target temperature — mapped to 20–45 °C
- **GPIO 27 (ADC1):** Humidity threshold — mapped to 0–100 %RH
- **Resolution:** 12-bit ADC (0–4095)

### Mode Selector — XB2-BD21

- **Type:** 2-position rotary selector
- **GPIO 22:** ECO (LOW) / PERFORMANCE (HIGH) — internal pullup
- **Position 1 (LOW):** ECO mode — reduced night target, electric heater disabled
- **Position 2 (HIGH):** PERFORMANCE mode — all heaters, full target

### Panel Voltmeters — 0–3 V, ⌀45 mm (×4)

Each GPIO drives an RC low-pass filter:
- **R:** 10 kΩ
- **C:** 100 µF 16 V electrolytic
- **fc ≈ 0.16 Hz** (τ ≈ 1 s) — fully suppresses 50 kHz PWM ripple, slow needle response for smooth reading
- **PWM:** 50 kHz, 12-bit — duty 0–91 % → 0–3.0 V on 3.3 V rail

| Channel | GPIO | Measurement | Full-scale |
|---------|------|-------------|------------|
| CH1 | 18 | Inlet humidity | 100 %RH |
| CH2 | 19 | Inlet temperature | 50 °C |
| CH3 | 20 | Outlet temperature | 50 °C |
| CH4 | 21 | Outlet humidity | 100 %RH |

### TM1637 4-digit LED Display — Session Duration

- **GPIO 4:** CLK
- **GPIO 5:** DIO
- **Format:**
  - `< 60 min` → `MM:SS` (e.g. `05:23`)
  - `≥ 60 min` → `HH:MM` (e.g. `01:30`), clamped to `99:59`
- **Brightness:** maximum (level 7)
- **Self-test:** displays `59:59` for 2 s on startup

---

## SD Card

- **Interface:** SPI0 (GPIO 0/1/2/3)
- **Format:** FAT32
- **Usage:** Session CSV logs only — no configuration files
- **Directory structure:** `/sessions/YYYY/MM/YYMMXXXX.csv`

---

## Power Supply

| Rail | Consumers | Source |
|------|-----------|--------|
| 5 V | Pico W, RTC, MCP23017, SHT30, MAX3485, TM1637 | Mean Well DIN rail PSU |
| 24 V | Belimo actuator, indicator LEDs (×8), button LEDs (×2) | Separate 24 V supply |
| 230 V AC | Electric heater (SSR), fan, circulator | Mains |

**Protection diodes — 1N4007** on 5 V and 24 V rails to prevent reverse current between supplies.

---

## BC337 — NPN Transistor Switch

The BC337 (NPN, TO-92, 45 V / 800 mA) is used throughout the design as a low-side switch to interface 3.3 V logic with higher-voltage loads.

**Wiring — indicator LED (common-emitter, low-side switch):**
```
24 V ──── R 1.2 kΩ ──── [LED] ────┐
                               Collector
                                 BC337
                               Base ──── R 2.2 kΩ ──── MCP23017 output (3.3 V)
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
- GPIO / MCP23017 HIGH (3.3 V) → base current flows → transistor saturates → load ON
- GPIO / MCP23017 LOW (0 V) → transistor off → load OFF

**Used for:**

| Application | Supply | Control source |
|-------------|--------|----------------|
| 8× indicator LEDs | 24 V | MCP23017 GPA0–GPA7 |
| START button backlight | 24 V | MCP23017 GPB7 |
| STOP button backlight | 24 V | MCP23017 GPB6 |
| Circulator PWM boost (3.3 V → 10 V) | 10 V | GPIO 15 |

> The common-emitter configuration inverts the signal (HIGH in → load ON, but collector pulls low). For the circulator PWM, the software compensates by inverting the duty cycle (`1.0 - duty`) before writing to the GPIO.

---

## Bill of Materials

| # | Component | Reference | Qty |
|---|-----------|-----------|-----|
| 1 | Raspberry Pi Pico W | RP2040 | 1 |
| 2 | SHT30 RS485 Modbus sensor | — | 2 |
| 3 | MAX3485 TTL-to-RS485 module | MAX3485 | 1 |
| 4 | MCP23017 I2C GPIO expander module | MCP23017 | 1 |
| 5 | RTC module | DS1307 + AT24C32 | 1 |
| 6 | MicroSD card module | SPI | 1 |
| 7 | TM1637 4-digit 7-segment display module | — | 1 |
| 8 | Hydraulic circulator pump | OEG 60 W | 1 |
| 9 | Electric heater | 600 W | 1 |
| 10 | Solid-state relay | Fotek 40 A | 1 |
| 11 | Centrifugal fan | Ruck 150, 105 W | 1 |
| 12 | Air damper actuator | Belimo LM24A-SR | 1 |
| 13 | Panel voltmeter 0–3 V ⌀45 mm | — | 4 |
| 14 | Indicator LED 24 V ⌀16 mm | — | 8 |
| 15 | Button Green Light 1NO 24 V ⌀16 mm | — | 1 (START) |
| 16 | Button Red Light 1NO 24 V ⌀16 mm | — | 1 (STOP) |
| 17 | Rotary selector 2 positions | XB2-BD21 | 1 |
| 18 | Potentiometer 10 kΩ | — | 2 |
| 19 | NPN transistor | BC337 TO-92 | 11 |
| 20 | Resistor 2.2 kΩ (BC337 base) | — | 11 |
| 21 | Resistor 1.2 kΩ (LED series) | — | 10 |
| 22 | Resistor 10 kΩ (voltmeter RC filter) | — | 4 |
| 23 | Capacitor 100 µF 16 V electrolytic (RC filter) | — | 4 |
| 24 | Diode 1N4007 (power supply protection) | 1N4007 | 2 |
| 25 | Power supply 5 V DIN rail | Mean Well | 1 |
| 26 | Power supply 24 V | — | 1 |
