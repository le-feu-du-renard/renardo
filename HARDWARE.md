# Hardware — v4

Controller for the renard'o dryer, built on a **Raspberry Pi Pico H** (RP2040, no
WiFi). Connectivity is provided solely by the LoRa radio.

> The v3 board is gone: no more panel voltmeters, MCP23017 expander, indicator
> LEDs, potentiometers, mode selector, TM1637 display or SD card. Everything the
> operator sees is on the TFT, and everything they change goes through the
> rotary encoder.

## GPIO map

23 of the 26 available GPIOs are used. **GP8, GP15 and GP22 are free.**

| Function | GPIO | Notes |
|---|---|---|
| SPI0 SCK | 18 | shared bus |
| SPI0 MOSI | 19 | shared bus |
| SPI0 MISO | 16 | only the SX1262 drives it |
| TFT CS | 17 | |
| TFT DC | 20 | |
| TFT RST | 21 | |
| LoRa NSS | 13 | |
| LoRa BUSY | 12 | mandatory on SX126x |
| LoRa DIO1 | 11 | RX interrupt |
| LoRa RST | 10 | |
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

## Shared SPI bus

The display and the radio sit on SPI0 with separate chip selects. Only the
SX1262 drives MISO; the ST7789 is write-only.

Both are serviced from core 0 in the same loop and neither runs asynchronously,
so they cannot interleave mid-transaction. They do need different bus settings,
which is why `SUPPORT_TRANSACTIONS` is enabled for `TFT_eSPI`.

**This is the main thing to exercise early during bring-up.** Symptoms of a
problem are a display that corrupts when the radio transmits, or a radio that
stops answering after a screen redraw.

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
on. If the panel comes up with inverted colours or an offset image, that is the
usual ST7789 variant question — try `TFT_INVERSION_ON` or a column/row offset.

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
7. Radio, with the display refreshing at the same time — see "Shared SPI bus".
