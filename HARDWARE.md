# Hardware — v4

Controller for the renard'o dryer, built on a **Raspberry Pi Pico H** (RP2040, no
WiFi). Connectivity is provided solely by the LoRa radio.

> The v3 board is gone: no more panel voltmeters, MCP23017 expander, indicator
> LEDs, potentiometers, mode selector, TM1637 display or SD card. Everything the
> operator sees is on the TFT, and everything they change goes through the
> rotary encoder.

## GPIO map

25 of the 26 available GPIOs are used. **GP21 is free.**

| Function | GPIO | Notes |
|---|---|---|
| SPI0 SCK | 18 | display only |
| SPI0 MOSI | 19 | display only; MISO not wired |
| TFT CS | 16 | SPI0 RX pin, reused as an output |
| TFT DC | 17 | |
| TFT RST | 20 | |
| SPI1 SCK | 10 | radio only |
| SPI1 MOSI | 11 | radio only |
| SPI1 MISO | 12 | radio only |
| LoRa NSS | 13 | driven in software |
| LoRa BUSY | 8 | mandatory on SX126x |
| LoRa DIO1 | 15 | RX interrupt |
| LoRa RST | 22 | |
| Encoder A | 6 | EC11, internal pull-up |
| Encoder B | 7 | EC11, internal pull-up |
| Encoder SW | 9 | EC11, internal pull-up |
| START/STOP button | 14 | active LOW, internal pull-up, toggles the session |
| RS485 DE/RE | 3 | HIGH = transmit |
| RS485 TX | 4 | UART1 → MAX3485 DI |
| RS485 RX | 5 | UART1 ← MAX3485 RO |
| Fan command | 0 | 2N2222, **active HIGH** |
| Damper command | 1 | 2N2222, **active LOW** |
| Electric heating command | 2 | 2N2222, **active HIGH** |
| Damper position feedback | 28 | ADC2 |
| I2C1 SDA | 26 | optional RTC |
| I2C1 SCL | 27 | optional RTC |

Pin assignments live in [include/config.h](include/config.h). The TFT pins are
**duplicated** into the `TFT_eSPI` build flags in
[platformio.ini](platformio.ini) — change both together or the display will not
initialise.

## Separate SPI buses

The display owns **SPI0**, the radio owns **SPI1**. They share nothing.

This costs two GPIOs over a shared bus and is worth it. `TFT_eSPI` on the
RP2040 may drive the panel through the **PIO** rather than the hardware SPI
block; on a shared bus that would put two different masters on the same pins,
and no amount of transaction bracketing would fix it. Separate buses remove the
question entirely, and let each device run at its own clock — the panel is happy
at 40 MHz, the SX1262 tops out around 16 and is driven at 8.

The display is write-only, so SPI0 MISO is not wired at all (`TFT_MISO=-1`).

SPI1 pin choices are fixed by the RP2040 and cannot be moved freely:
SCK ∈ {10, 14, 26}, MOSI ∈ {11, 15, 27}, MISO ∈ {8, 12, 24, 28}. NSS is driven
in software by RadioLib, so it is free of the hardware chip-select constraint.

## Command outputs

The three outputs are **2N2222 NPN transistors in open collector** — note this
is a small-signal bipolar (800 mA max), not a MOSFET. They switch a control
signal, never a load directly.

- Base fed through ~1 kΩ from the GPIO, emitter to the common ground.
- A free-wheeling diode is required if a coil is driven directly.
- **A common ground between the 24 V supply and the Pico is mandatory.**

### Polarity, and why it differs per output

Until `pinMode()` runs — roughly two seconds after power-up — the GPIOs are
high-impedance inputs. The safe state has to be the one a **floating** GPIO
produces.

| Output | Wiring | Polarity | Floating GPIO |
|---|---|---|---|
| Fan | contactor coil between +24 V and the collector | active HIGH | no current → **off** |
| Electric heating | contactor coil between +24 V and the collector | active HIGH | no current → **off** |
| Damper | collector pulls down the Belimo command input, which the actuator pulls up | active LOW | line high → recirculation |

Wiring the fan or the electric heating active LOW would energise them during the
whole boot window. Polarity is declared per output in `config.h`
(`OUT_*_ACTIVE_LOW`), not globally.

## Air damper

Belimo **LM24A-SR**, driven purely on/off: recirculation or extraction, never a
percentage. Travel takes about 150 s each way.

Its 2-10 V position feedback goes through a divider to 0-3.3 V on ADC2. **It
feeds the display only** — no control logic depends on it. Since the feedback
starts at 2 V rather than 0 V, calibration is two-point, captured from the menu
by driving the damper to each end stop (Système → Registre fermé / ouvert).

A disconnected feedback wire yields a degenerate calibration span, which the
firmware detects and reports as "no position" rather than as 0 %.

## RS485 bus

A single MAX3485 carries every Modbus RTU slave, 9600 8N1. Core 1 owns the bus
exclusively.

| Address | Device | Registers |
|---|---|---|
| 1 | SHT30 probe, injection | FC03 `0x0000` %RH ×10, `0x0001` °C ×10 |
| 2 | SHT30 probe, outlet | idem |
| 10 | Hydraulic module | see below |

120 Ω termination at both ends of the segment.

### Hydraulic module (to be built)

Deported over RS485. It owns the three-way valve and the circulator; the dryer
only tells it to run and at what water temperature, because the valve is far too
slow to be modulated from here.

| Register | Direction | Contents |
|---|---|---|
| `0x0000` | write | requested state, 0 = off, 1 = on |
| `0x0001` | write | water setpoint ×10 (°C) |
| `0x0010` | read | circulating water temperature ×10, signed |
| `0x0011` | read | storage tank temperature ×10, signed |
| `0x0012` | read | status bits |

State and setpoint are written in **one FC16 transaction**, so the module never
sees a state change paired with a stale setpoint.

**The module must implement its own watchdog** and shut down if it receives no
frame for 60 s. The dryer marks it unavailable after 30 s of silence and falls
back to electric-only.

## LoRa radio

**DX-LR30 (SX1262), 868 MHz**, EU band. Settings in `config.h`: SF9, BW 125 kHz,
CR 4/7, 14 dBm, sync word 0x34.

Telemetry every 60 s, continuous reception in between. At SF9 a 43-byte frame
lasts about 100 ms, so one transmission per minute stays far below the 1 % duty
cycle limit on g1 — recheck this if the interval is ever shortened.

`LORA_DEVICE_ID` identifies this dryer: frames addressed elsewhere are dropped,
which matters as soon as a second dryer shares the band.

## Optional RTC

DS1307 on I2C1, address 0x68. Probed at startup.

**With no RTC there is no ECO mode**: the night window cannot be evaluated
without a wall clock, so the whole ECO submenu is greyed out and the mode is
forced to PERFORMANCE whatever is stored in the settings.

## Display

**GMT020-02-7P v1.3**, ST7789, 240×320, used in **landscape (320×240)** via
`setRotation(1)`.

The 7-pin connector carries no backlight control; the backlight is permanently
on — so a lit backlight proves the supply, and nothing else.

### Wiring

The module's `SCL` and `SDA` are **SPI**, not I2C, despite the silkscreen.

Listed in the order the pins appear on the module, which is **not** the order
they appear on the Pico header — wiring positionally rather than by name is the
easy mistake here.

| Module | Pico GP | Header pin |
|---|---|---|
| GND | GND | 23 |
| VCC | 3V3(OUT) | 36 |
| SCL (clock) | GP18 | 24 |
| SDA (data) | GP19 | 25 |
| RES | GP20 | 26 |
| DC | GP17 | 22 |
| CS | GP16 | 21 |

3.3 V only. The five signals land on header pins 21–26, with GND at 23.

CS sits on GP16, which is also SPI0's RX pin. The panel never drives data back,
so RX is idle, and TFT_eSPI re-asserts the pin as an output after `spi.begin()`
for exactly this case.

TFT_eSPI on the RP2040 does not call `spi_init()` or `gpio_set_function()`: it
calls plain `spi.begin()` and inherits arduino-pico's default SPI0 pins, which
are MISO 16, CS 17, SCK 18, MOSI 19. SCK and MOSI above match those defaults on
purpose — moving them would need `SPI.setSCK()`/`setTX()` before `tft.init()`.

### Clock polarity

**This panel needs `TFT_SPI_MODE=SPI_MODE0`** — the clock idling low.

TFT_eSPI defaults ST7789 to `SPI_MODE3`, with the comment "some ST7789 boards do
not work with Mode 0". This module is the converse: on the library default it
shows nothing whatsoever, not even corruption, and does not answer a readback
either. It is the single setting that stood between a blank panel and a working
one, and nothing about the symptom points at it — a wiring fault looks exactly
the same.

### When nothing appears

`TFT_eSPI::init()` writes its sequence blind and never reads back, so the log
line only reports that the sequence was *sent*. The start-up splash — red,
green, blue, then a banner — is the only real evidence the panel is alive.

Nothing on screen with the backlight lit means the panel is not receiving, not
leaving reset, or being clocked on the wrong edge. Check, in this order: the
clock polarity above, then **DC**, **RES**, **CS**, then SCL/SDA not swapped.

Two bring-up environments help:

`pio run -e pin_test -t upload -t monitor` drives each of the five signals on
its own at 1 Hz, announcing which one, so every wire can be confirmed with a
multimeter or an LED. It uses no libraries — it proves the wire, not the
driver. Probe at the **module** end: that is what distinguishes a broken wire
from a wrong pin.

`pio run -e tft_test -t upload -t monitor` asks the panel to identify itself.
The module exposes no MISO pin, but the ST7789 answers on the SDA line, which
is bidirectional in 4-wire SPI, so `TFT_SDA_READ` turns the pin around for the
read. An ST7789V normally reports `0x85 0x85 0x52`. A plausible ID means the
panel is powered, out of reset and wired correctly in both directions, so
anything still wrong is driver configuration; all zeroes or all ones means
nothing is answering and the fault is power, reset, CS or the clock/data pair.
Some modules put a series resistor on SDA and cannot be read at all, so a silent
answer is suggestive rather than conclusive. The test then sweeps colours and
draws corner markers, which also reveal orientation and any row/column offset.

## Power supply

| Rail | Use |
|---|---|
| 24 V | contactors, Belimo actuator, RS485 modules |
| 5 V | Pico (VSYS), TFT |
| 3.3 V | SX1262, MAX3485, RTC |

The SX1262 draws around 120 mA during transmission — size the 3.3 V rail so a
transmission does not brown out the display.

## Bring-up order

1. Pico alone, USB serial: check the boot log.
2. TFT: mire, fonts, icons. Confirm orientation and colours.
3. Encoder: detents and click, no phantom steps.
   Button: one press starts, the next stops — check it never double-fires.
4. Outputs one at a time, **measuring at the connector before wiring the loads**
   — this is where a polarity mistake is caught.
5. Damper feedback: full travel, record the two end-stop values, calibrate.
6. RS485: probes first, then the hydraulic module.
7. Radio, with the display refreshing at the same time. The buses are
   independent, so this should be uneventful — confirm it anyway.
