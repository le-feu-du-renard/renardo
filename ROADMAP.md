# Roadmap

## In Progress — v4 migration

The `v4` branch carries the board redesign: TFT + rotary encoder interface,
deported hydraulic module, everything remote on RS485, and the removal of every
v3 panel control.

### Done

- [x] Strip the v3 hardware layer (voltmeters, MCP23017, LEDs, potentiometers,
      selector, TM1637, SD card) and move to `rpipico`
- [x] New pin map: SPI0 for the TFT, single RS485 bus, three BC337 command
      outputs, damper ADC feedback, optional RTC
- [x] `Rs485Bus` transport, per-probe error and freshness tracking,
      `HydraulicRemote` client for the deported module
- [x] Sensor freshness interlock, cross-core seqlock snapshot
- [x] Replace the circulator PID with two independent on/off sources
- [x] `OutputDriver` with per-output polarity, damper position readback
- [x] LittleFS persistence for settings and session progress
- [x] Rotary encoder with a bounce-proof quadrature decoder
- [x] Main screen on the TFT, region-based rendering, animated fan
- [x] Configuration menu bound to the persisted settings record
- [x] Documentation rewritten for v4
- [x] Drop the LoRa radio entirely — the remote link becomes an RS485 extension
      port, freeing seven GPIOs, the SPI1 block and the RadioLib dependency

### Remaining before the board is usable

- [ ] Bring-up on real hardware, in the order given in HARDWARE.md
- [ ] Confirm the ST7789 variant: orientation, colour inversion, offsets
- [ ] Measure the three outputs at the connector before wiring the loads
- [ ] Record the damper end-stop ADC values and calibrate
- [ ] Build the deported hydraulic module against the register map in HARDWARE.md

## Open questions

- [ ] Automatic session end? `DRYING_SESSION_DURATION` existed since the
      beginning and was never used; the cycle loops until STOP.
- [ ] `HumidityManager::Mode::kThreshold` is implemented but never selected —
      expose it from the menu, or remove it.
- [ ] `ResetControl()` fires on every phase transition, so roughly every
      19 minutes. Worth keeping now that the hydraulic timers are 300 s?
- [ ] Day/night water setpoint for the hydraulic module when an RTC is fitted.

## Next — the RS485 extension port

The dryer has no remote link at all since the radio came out. This is what
replaces it, and it is deliberately more than a radio: one connector on which
optional modules hang — data logger, energy metering, an SD card, a LoRa or
WiFi gateway for whoever still wants one. The dryer gains a feature by gaining
a module, and none of them is ever load-bearing for regulation.

**The dryer stays the Modbus RTU master on the single bus.** Modbus allows one
master per segment, and the probe @1 and the hydraulic module @10 both depend on
the dryer being it. So the extension is **another slave**, on a free address —
2 to 9 or 11 upwards, address 2 having been vacant since the outlet probe was
dropped. No second transceiver, no second UART, no GPIO: this costs the board
nothing.

- **Uplink.** The dryer pushes its telemetry into the module's registers with a
  single `WriteMultipleRegisters` (FC16), exactly as `HydraulicRemote::Update()`
  pushes state and setpoint together.
- **Downlink.** A mailbox: the dryer reads a command block with FC03 on each
  cycle and writes back an acknowledgement register. A slave cannot speak
  unprompted, so a command waits at most one `loop1()` cycle —
  `SENSOR_UPDATE_INTERVAL`, 2 s. The radio it replaces answered once a minute.
- **Reused as-is.** `Rs485Bus` unchanged; the `HydraulicRemote` shape (own
  address, `IsAvailable()` on a timeout, error counter); the register map
  declared in `config.h` beside the `HYDRO_REG_*` block; and the cross-core
  request pattern in `main.cpp` — the bus belongs to Core 1 alone, so commands
  arrive there and cross over the same way `g_hydraulic_request` does.
- **Where the register map comes from.** The v3 telemetry frame already settled
  what is worth sending: probe readings, both water temperatures, setpoints,
  phase, elapsed time, actuator flags, both register openings, each with a
  sentinel distinct from a real zero. It is in git, in `include/LoraProtocol.h`
  on the commit before its removal — start from `TelemetryData` rather than from
  a blank page.

Open with it: whether commands need the sequence-number replay filter the radio
carried. A byte-summed frame over the air could arrive twice; a Modbus register
read cannot, so the filter may well be answering a question the bus no longer
asks.

## Later

- [ ] Diagnostics screen: bus state and error counters
- [ ] Version report over the extension port

## Completed in v3

- [x] Hybrid heating with predictive electric shutoff and anti-short-cycle guards
- [x] Three-phase drying sequence with humidity-driven transitions
- [x] RS485 SHT30 probes on a dedicated core
- [x] Session persistence across reboots
- [x] Watchdog and glitch-free relay state at boot
