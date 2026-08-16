# Roadmap

## In Progress — v4 migration

The `v4` branch carries the board redesign: TFT + rotary encoder interface,
deported hydraulic module, LoRa link, and the removal of every v3 panel control.

### Done

- [x] Strip the v3 hardware layer (voltmeters, MCP23017, LEDs, potentiometers,
      selector, TM1637, SD card) and move to `rpipico`
- [x] New pin map: SPI0 for the TFT, SPI1 for the radio, single RS485 bus,
      three 2N2222 command outputs, damper ADC feedback, optional RTC
- [x] `Rs485Bus` transport, per-probe error and freshness tracking,
      `HydraulicRemote` client for the deported module
- [x] Sensor freshness interlock, cross-core seqlock snapshot
- [x] Replace the circulator PID with two independent on/off sources
- [x] `OutputDriver` with per-output polarity, damper position readback
- [x] LittleFS persistence for settings and session progress
- [x] Rotary encoder with a bounce-proof quadrature decoder
- [x] Main screen on the TFT, region-based rendering, animated fan
- [x] Configuration menu bound to the persisted settings record
- [x] LoRa telemetry and acknowledged remote commands over SX1262
- [x] Documentation rewritten for v4

### Remaining before the board is usable

- [ ] Bring-up on real hardware, in the order given in HARDWARE.md
- [ ] Confirm the ST7789 variant: orientation, colour inversion, offsets
- [ ] Confirm the two SPI buses stay independent under load: radio
      transmitting while the display refreshes
- [ ] Measure the three outputs at the connector before wiring the loads
- [ ] Record the damper end-stop ADC values and calibrate
- [ ] Build the deported hydraulic module against the register map in HARDWARE.md
- [ ] Commander side: decode the telemetry frame, send acknowledged commands

## Open questions

- [ ] Automatic session end? `DRYING_SESSION_DURATION` existed since the
      beginning and was never used; the cycle loops until STOP.
- [ ] `HumidityManager::Mode::kThreshold` is implemented but never selected —
      expose it from the menu, or remove it.
- [ ] `ResetControl()` fires on every phase transition, so roughly every
      19 minutes. Worth keeping now that the hydraulic timers are 300 s?
- [ ] Day/night water setpoint for the hydraulic module when an RTC is fitted.

## Later

- [ ] Diagnostics screen: bus state, error counters, RSSI
- [ ] Firmware update over LoRa, or at least a version report
- [ ] Second dryer on the same band (the device id already supports it)

## Completed in v3

- [x] Hybrid heating with predictive electric shutoff and anti-short-cycle guards
- [x] Three-phase drying sequence with humidity-driven transitions
- [x] RS485 SHT30 probes on a dedicated core
- [x] Session persistence across reboots
- [x] Watchdog and glitch-free relay state at boot
