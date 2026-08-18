#include <string.h>
#include <SPI.h>

#include "config.h"
#include "LoraLink.h"
#include "Logger.h"

volatile bool LoraLink::packet_received_ = false;

void LoraLink::OnDio1Action()
{
  packet_received_ = true;
}

LoraLink::LoraLink()
    : radio_(new Module(LORA_NSS_PIN, LORA_DIO1_PIN, LORA_RST_PIN, LORA_BUSY_PIN,
                        SPI1,
                        SPISettings(LORA_SPI_FREQUENCY, MSBFIRST, SPI_MODE0))),
      ready_(false),
      device_id_(0),
      tx_sequence_(0),
      last_tx_ms_(0),
      last_rx_ms_(0),
      last_rssi_(NAN),
      pending_command_(false),
      pending_code_(kLoraCommandNone),
      pending_argument_(0.0f) {}

bool LoraLink::Begin(uint16_t device_id)
{
  device_id_ = device_id;

  // The RP2040 needs its SPI1 pins assigned before the peripheral starts, and
  // RadioLib calls begin() on the bus from inside radio_.begin() — so this has
  // to happen first.
  SPI1.setSCK(SPI1_SCK_PIN);
  SPI1.setTX(SPI1_MOSI_PIN);
  SPI1.setRX(SPI1_MISO_PIN);

  int16_t state = radio_.begin(LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                               LORA_CODING_RATE, LORA_SYNC_WORD, LORA_TX_POWER,
                               LORA_PREAMBLE_LENGTH);
  if (state != RADIOLIB_ERR_NONE)
  {
    // A missing radio must not stop the dryer: the link is for reporting and
    // remote control, never for regulation.
    Logger::Error("LoraLink: SX1262 init failed (%d) — running without radio", state);
    ready_ = false;
    return false;
  }

  radio_.setDio1Action(OnDio1Action);

  state = radio_.startReceive();
  if (state != RADIOLIB_ERR_NONE)
  {
    Logger::Error("LoraLink: cannot enter receive mode (%d)", state);
    ready_ = false;
    return false;
  }

  ready_ = true;
  last_tx_ms_ = millis();
  Logger::Info("LoraLink: SX1262 ready at %F MHz SF%d, device id %d",
               LORA_FREQUENCY, LORA_SPREADING_FACTOR, device_id_);
  return true;
}

void LoraLink::Update(const TelemetryData &data, uint32_t telemetry_interval_ms)
{
  if (!ready_)
  {
    return;
  }

  ServiceReceive();

  uint32_t now = millis();
  if (now - last_tx_ms_ >= telemetry_interval_ms)
  {
    last_tx_ms_ = now;
    SendTelemetry(data);
  }
}

void LoraLink::SendTelemetry(const TelemetryData &data)
{
  TelemetryPacket packet;
  memset(&packet, 0, sizeof(packet));

  packet.device_id         = device_id_;
  packet.sequence          = ++tx_sequence_;
  packet.uptime_s          = millis() / 1000;
  packet.session_elapsed_s = data.session_elapsed_s;
  packet.phase             = data.phase;

  packet.inlet_temperature  = LoraEncodeValue(data.inlet_temperature);
  packet.inlet_humidity     = LoraEncodeValue(data.inlet_humidity);
  packet.water_temperature  = LoraEncodeValue(data.water_temperature);
  packet.tank_temperature   = LoraEncodeValue(data.tank_temperature);
  packet.target_temperature = LoraEncodeValue(data.target_temperature);
  packet.target_humidity    = LoraEncodeValue(data.target_humidity);

  packet.flags = 0;
  if (data.running)           packet.flags |= kLoraFlagRunning;
  if (data.fan_on)            packet.flags |= kLoraFlagFan;
  if (data.electric_on)       packet.flags |= kLoraFlagElectric;
  if (data.hydraulic_on)      packet.flags |= kLoraFlagHydraulic;
  if (data.damper_open)       packet.flags |= kLoraFlagDamperOpen;
  if (data.sensor_fault)      packet.flags |= kLoraFlagSensorFault;
  if (!data.hydraulic_online) packet.flags |= kLoraFlagHydraulicOff;

  packet.damper_position = isnan(data.damper_position)
                               ? 0xFF
                               : static_cast<uint8_t>(data.damper_position);
  packet.last_command_ack = filter_.GetLastSequence();

  SealTelemetry(packet);

  int16_t state = radio_.transmit(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
  if (state != RADIOLIB_ERR_NONE)
  {
    Logger::Warning("LoraLink: transmit failed (%d)", state);
  }

  // transmit() leaves the radio idle, so listening has to be re-armed or the
  // next command would be missed entirely.
  radio_.startReceive();
}

void LoraLink::ServiceReceive()
{
  if (!packet_received_)
  {
    return;
  }
  packet_received_ = false;

  CommandPacket packet;
  int16_t state = radio_.readData(reinterpret_cast<uint8_t *>(&packet), sizeof(packet));

  // Re-arm reception whatever happened, so one bad frame cannot deafen the link.
  radio_.startReceive();

  if (state != RADIOLIB_ERR_NONE)
  {
    Logger::Debug("LoraLink: receive error (%d)", state);
    return;
  }

  if (radio_.getPacketLength() != sizeof(packet))
  {
    return; // not one of ours
  }

  if (!IsCommandValid(packet, device_id_))
  {
    Logger::Debug("LoraLink: frame rejected (magic/id/checksum)");
    return;
  }

  last_rx_ms_ = millis();
  last_rssi_  = radio_.getRSSI();

  if (!filter_.ShouldExecute(packet.sequence))
  {
    // The Commander repeats until acknowledged, so duplicates are normal.
    Logger::Debug("LoraLink: duplicate command %d ignored", packet.sequence);
    return;
  }

  pending_command_  = true;
  pending_code_     = packet.command;
  pending_argument_ = LoraDecodeValue(packet.argument);
  Logger::Info("LoraLink: command %d received (seq %d, arg %F)",
               pending_code_, packet.sequence, pending_argument_);
}

bool LoraLink::ConsumeCommand(uint8_t &command, float &argument)
{
  if (!pending_command_)
  {
    return false;
  }
  pending_command_ = false;
  command  = pending_code_;
  argument = pending_argument_;
  return true;
}

bool LoraLink::IsLinked() const
{
  if (!ready_ || last_rx_ms_ == 0)
  {
    return false;
  }
  return (millis() - last_rx_ms_) < kLinkTimeoutMs;
}
