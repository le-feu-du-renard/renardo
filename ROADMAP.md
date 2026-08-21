# Roadmap

## In Progress — v4 migration

The `v4` branch carries the board redesign: TFT + rotary encoder interface,
deported hydraulic module, everything remote on RS485, and the removal of every
v3 panel control.

### Done

- [x] Strip the v3 hardware layer (voltmeters, MCP23017, LEDs, potentiometers,
      selector, TM1637, SD card) and move to `rpipico`
- [x] New pin map: hardware SPI for the TFT, single RS485 bus, three BC337
      command outputs, damper ADC feedback, optional RTC
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
- [x] Panel controls back on three of those GPIOs: dedicated START and STOP
      buttons in place of the single toggle, and green/red status LEDs saying
      running, cooling, stopped and fault from across the room
- [x] The RS485 extension port itself: telemetry block and command mailbox on
      address 2, a `RemoteModule` base shared with the hydraulic client, and a
      backoff that stops an unplugged module from stalling the poll loop
- [x] Pin map rearranged for the PCB: every module's signals on consecutive
      header pins with their ground inside the block, panel and encoder and
      display and bus on pins 1-20, plant I/O and RTC on 21-40. The display
      moved to SPI1 to make room, since the ten GPIOs on the plant side would
      not carry it as well; GP7, GP18, GP19 and GP28 are what is left free

### Remaining before the board is usable

- [ ] Bring-up on real hardware, in the order given in HARDWARE.md
- [ ] Confirm the ST7789 variant: orientation, colour inversion, offsets
- [ ] Measure the three outputs at the connector before wiring the loads
- [ ] `panel_test` on the wired panel: both LEDs dark at boot, one line per
      press under the right name, the green readable in daylight
- [ ] Record the damper end-stop ADC values and calibrate
- [ ] Build the deported hydraulic module against the register map in HARDWARE.md
- [ ] Exercise the extension port against a Modbus slave simulator on address 2
      before any module firmware exists

## Open questions

- [ ] Automatic session end? `DRYING_SESSION_DURATION` existed since the
      beginning and was never used; the cycle loops until STOP.
- [ ] `HumidityManager::Mode::kThreshold` is implemented but never selected —
      expose it from the menu, or remove it.
- [ ] `ResetControl()` fires on every phase transition, so roughly every
      19 minutes. Worth keeping now that the hydraulic timers are 300 s?
- [ ] Day/night water setpoint for the hydraulic module when an RTC is fitted.

## Done — the RS485 extension port

What replaced the radio, and deliberately more than one: a connector on which
optional modules hang — data logger, energy metering, an SD card, a LoRa or WiFi
gateway for whoever still wants one. The dryer gains a feature by gaining a
module, and none of them is ever load-bearing for regulation. The register map is
in [HARDWARE.md](HARDWARE.md); the wire format both sides compile is
`include/ExtensionProtocol.h`.

**The dryer stays the Modbus RTU master on the single bus**, so the extension is
another slave — address 2, vacant since the outlet probe was dropped. No second
transceiver, no second UART, no GPIO. Telemetry goes out on one FC16, the command
mailbox comes back on one FC03, and the acknowledgement rides inside the next
telemetry block rather than costing a write of its own.

Three things the plan did not foresee:

- **The shape shared with `HydraulicRemote` was worth extracting.** Address,
  availability timeout and error counter are the same on both, so they moved into
  `RemoteModule`. The two clients differ only in their register blocks and in
  which way authority runs — the dryer commands the hydraulic module and obeys
  neither.
- **An absent module was already stalling the poll loop**, before any extension
  existed. ModbusMaster's response timeout is fixed at 2 s, so an unplugged
  hydraulic module cost `loop1()` two seconds every cycle, and Core 0 cuts the
  heating once the probe reading ages past `SENSOR_TIMEOUT_MS`. `BackoffGate`
  retries a dead module once per interval instead of once per cycle, and
  `loop1()` now publishes the sensor snapshot before polling either module rather
  than after.
- **Remote setpoints persist lazily.** A menu commit is one value per knob click;
  a module can send one every cycle, and a flash write every two seconds would
  wear the part out for nothing.

The question this section left open is settled, in the opposite direction to the
guess. The replay filter **is** still needed: a Modbus read cannot duplicate, but
the dryer re-reads the same mailbox every cycle and would replay a resident
command forever. With neither loss nor reordering on the bus it collapses to an
inequality, so the signed window the radio's byte-wide sequence needed is gone.

Starting a session is the one thing the port cannot do. The opcode is reserved
and refused every time, so a module author gets an answer rather than silence,
but no remote launches a dryer nobody is standing in front of.

## Later

- [ ] Diagnostics screen: bus state and error counters
- [ ] Version report over the extension port

## Completed in v3

- [x] Hybrid heating with predictive electric shutoff and anti-short-cycle guards
- [x] Three-phase drying sequence with humidity-driven transitions
- [x] RS485 SHT30 probes on a dedicated core
- [x] Session persistence across reboots
- [x] Watchdog and glitch-free relay state at boot
