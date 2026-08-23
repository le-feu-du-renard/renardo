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
- [x] Hand the hydraulic back to its own module — a run permission over RS485
      instead of a second regulator — and give the electric a tolerance window
      each time the damper moves
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
- [ ] Build the deported hydraulic module against the register map in
      HARDWARE.md, now that `0x0000` is settled as a run permission held for the
      whole session rather than an on/off command
- [ ] Exercise the extension port against a Modbus slave simulator on address 2
      before any module firmware exists

## Open questions

- [ ] Automatic session end? `DRYING_SESSION_DURATION` existed since the
      beginning and was never used; the cycle loops until STOP.
- [ ] `HumidityManager::Mode::kThreshold` is implemented but never selected —
      expose it from the menu, or remove it.
- [ ] Day/night water setpoint for the hydraulic module when an RTC is fitted.
- [ ] Init exits on `GetTargetTemperature()`, the raw setpoint, not the
      ECO-effective one. Under ECO the loop holds the reduced setpoint, which
      the exit test never sees, so Init runs its full hour. Move the test, or
      decide that warming up at the full target is what Init is for.

## Done — the hydraulic module owns its own heat

v4 arrived carrying two regulators pointed at one three-way valve. The module
holds its water setpoint and fires its own circulator; the dryer cycled it on top
of that, on a 1.5 °C band on *air* temperature with 300 s minimum on and off
times. **The slower of the two controllers was never ours**, and the valve takes
minutes to travel. What leaves now is a run permission — raised when the source is
enabled and a session is running with the interlocks holding, held across every
phase transition, withdrawn when they fail — and the dryer regulates the electric
heater and nothing else.

The second half is the air renewal. Extraction injects outside air and the inlet
temperature drops, which is the point of the phase. The damage was on the way
back: with the register shut the chamber recovers far faster than any approach to
setpoint the 60 s horizon was sized for, the predictive shutoff read that as an
impending overshoot, and `CTRL_T_OFF_MIN` then held the heater off for another
minute. Two minutes off-setpoint per cycle is an accepted cost of renewing air;
sagging for ten because the loop was fighting its own transient is not.

Three things the plan did not foresee:

- **The 300 s valve protection was never real.** `ResetControl()` fired on every
  phase entry and reset the hydraulic timers with everything else, so the guard
  was wiped roughly every 19 minutes — and extraction is shorter than the guard
  it was wiping. The phase machine could drop the valve out and re-energise it
  seconds later, all cycle long. Removing the loop removed the timers, and the
  open question about `ResetControl()` answered itself on the way past.
- **The window belongs to the damper, not the phase.** Hooking phase transitions
  was the obvious move and would have missed two real cases: Init's
  sub-extraction, which opens the register for two minutes in the middle of a
  phase and told the regulation nothing at all, and the end of a fault purge,
  which puts the register back wherever the phase wanted it. Every damper
  movement the session commands now goes through one place that announces it.
- **The transition was cutting the heater at the worst possible moment.**
  `ResetControl()` opened the contactor on entry to every phase — exactly as cold
  air arrived and the chamber most needed heat — then let it close again a tick
  later. That was never the intent; it was a full reset borrowed for a job that
  only needed a hint.

The settings record went to v5 for it. Four hydraulic knobs left the Régulation
page and the persisted struct, and `air_renewal_window` took their place. Same
cost as the v4 bump: every stored record is discarded and the register
calibration has to be captured again — which is free today, and would not have
been after bring-up.

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
  which way authority runs — the dryer permits the hydraulic module and obeys
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
- [ ] Surface `HydraulicRemote::GetStatusBits()`. It is read every cycle and used
      nowhere, and it is what would let the screen show whether the module is
      actually firing rather than whether it was cleared to

## Completed in v3

- [x] Hybrid heating with predictive electric shutoff and anti-short-cycle guards
- [x] Three-phase drying sequence with humidity-driven transitions
- [x] RS485 SHT30 probes on a dedicated core
- [x] Session persistence across reboots
- [x] Watchdog and glitch-free relay state at boot
