# renard'o — Dryer Controller

Food dehydrator controller built on a Raspberry Pi Pico H. Hybrid heating
(hydraulic + electric), an RS485 probe and a deported hydraulic module, TFT
interface driven by a rotary encoder, START and STOP buttons, and two status
LEDs.

## Project Philosophy

**Low-tech, repairable, and accessible.** Regulation runs entirely on the
board, and there is nothing else for it to run on: no network, no server, no
radio. The dryer is complete on its own, and everything that could be added to
it later — logging, metering, a remote link — hangs off the RS485 bus as an
optional module that regulation never waits for.

## Documentation

| File | Contents |
|------|----------|
| [HARDWARE.md](HARDWARE.md) | GPIO map, wiring, output polarity, Modbus registers, bring-up |
| [DOCUMENTATION.md](DOCUMENTATION.md) | Drying sequence, temperature control, interface, persistence |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Build, upload, debug, tests |
| [ROADMAP.md](ROADMAP.md) | Planned work, completed features |

## Architecture at a glance

- **Core 1** owns the RS485 bus: the SHT30 inlet probe and the hydraulic module.
- **Core 0** runs everything else: regulation, display, menu.
- Readings cross between them through a seqlock-protected snapshot carrying
  freshness timestamps, which is what lets stale data block the heating.

The domain classes are hardware-free and compiled natively by the unit tests,
so the regulation is tested as the firmware actually runs it.

## Applications

Herbs, fruits, vegetables, mushrooms, flowers, artisan preparations.

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE-GPL-3.0).
