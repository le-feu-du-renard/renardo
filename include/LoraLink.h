#ifndef LORA_LINK_H
#define LORA_LINK_H

#include <Arduino.h>
#include <RadioLib.h>

#include "LoraProtocol.h"

// SX1262 (DX-LR30, 868 MHz) on SPI1, its own bus.
//
// The dryer sends telemetry on a timer and listens the rest of the time, so a
// command from the Commander is picked up within a second rather than at the
// next transmission. Reception is interrupt-driven on DIO1.
//
// The radio deliberately does not share the display's bus: TFT_eSPI may drive
// the panel through the RP2040's PIO instead of the hardware SPI block, which
// would put two different masters on the same pins. Separate buses also let
// each run at its own clock, the panel at 40 MHz and the SX1262 well below it.
class LoraLink
{
public:
  LoraLink();

  bool Begin(uint16_t device_id);
  bool IsReady() const { return ready_; }

  // Sends telemetry when due and services reception. Call from the main loop.
  void Update(const TelemetryData &data, uint32_t telemetry_interval_ms);

  // True while the Commander has been heard from recently.
  bool IsLinked() const;
  float GetRssi() const { return last_rssi_; }

  // Takes the next command to execute, if any. Repeats and stale frames are
  // filtered out before they get here.
  bool ConsumeCommand(uint8_t &command, float &argument);

private:
  SX1262 radio_;
  bool   ready_;

  uint16_t device_id_;
  uint32_t tx_sequence_;
  uint32_t last_tx_ms_;
  uint32_t last_rx_ms_;
  float    last_rssi_;

  LoraCommandFilter filter_;

  bool    pending_command_;
  uint8_t pending_code_;
  float   pending_argument_;

  void SendTelemetry(const TelemetryData &data);
  void ServiceReceive();

  // Set from the DIO1 interrupt; only one radio instance is supported because
  // the interrupt API takes a bare function pointer.
  static volatile bool packet_received_;
  static void OnDio1Action();

  static constexpr uint32_t kLinkTimeoutMs = 900000; // 15 min without a frame
};

#endif // LORA_LINK_H
