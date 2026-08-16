# renard'o — Dryer Controller

Food dehydrator controller built on a Raspberry Pi Pico H. Hybrid heating
(hydraulic + electric), RS485 probes and a deported hydraulic module, TFT
interface driven by a rotary encoder, LoRa link to a server.

## Project Philosophy

**Low-tech, repairable, and accessible.** Regulation runs entirely on the
board: it keeps working with no network, no server, and no hydraulic module.
The radio reports and accepts commands, it never regulates.

## Documentation

| File | Contents |
|------|----------|
| [HARDWARE.md](HARDWARE.md) | GPIO map, wiring, output polarity, Modbus registers, bring-up |
| [DOCUMENTATION.md](DOCUMENTATION.md) | Drying sequence, temperature control, interface, remote link |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Build, upload, debug, tests |
| [ROADMAP.md](ROADMAP.md) | Planned work, completed features |

## Architecture at a glance

- **Core 1** owns the RS485 bus: two SHT30 probes and the hydraulic module.
- **Core 0** runs everything else: regulation, display, menu, radio.
- Readings cross between them through a seqlock-protected snapshot carrying
  freshness timestamps, which is what lets stale data block the heating.

The domain classes are hardware-free and compiled natively by the unit tests,
so the regulation is tested as the firmware actually runs it.

## Applications

Herbs, fruits, vegetables, mushrooms, flowers, artisan preparations.

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE-GPL-3.0).
