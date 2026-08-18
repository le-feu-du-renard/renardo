#include <math.h>
#include <stddef.h>

#include "LoraProtocol.h"

int16_t LoraEncodeValue(float value)
{
  if (isnan(value))
  {
    return kLoraInvalidValue;
  }

  float scaled = value * 10.0f;
  if (scaled > 32760.0f)
  {
    scaled = 32760.0f;
  }
  if (scaled < -32760.0f)
  {
    scaled = -32760.0f;
  }
  return static_cast<int16_t>(scaled < 0 ? scaled - 0.5f : scaled + 0.5f);
}

uint8_t LoraEncodePosition(float percent)
{
  if (isnan(percent))
  {
    return kLoraNoPosition;
  }

  // Clamp rather than cast straight through: a feedback reading slightly past
  // its calibrated end stop would otherwise wrap into a small opening, and
  // anything at or above 255 would land on the "no feedback" sentinel.
  if (percent <= 0.0f)
  {
    return 0;
  }
  if (percent >= 100.0f)
  {
    return 100;
  }
  return static_cast<uint8_t>(percent + 0.5f);
}

float LoraDecodePosition(uint8_t raw)
{
  return raw == kLoraNoPosition ? NAN : static_cast<float>(raw);
}

float LoraDecodeValue(int16_t raw)
{
  if (raw == kLoraInvalidValue)
  {
    return NAN;
  }
  return raw / 10.0f;
}

uint16_t LoraChecksum(const void *data, size_t length)
{
  const uint8_t *bytes = static_cast<const uint8_t *>(data);
  uint16_t sum = 0;
  for (size_t i = 0; i < length; i++)
  {
    sum = static_cast<uint16_t>(sum + bytes[i]);
  }
  return sum;
}

void SealTelemetry(TelemetryPacket &packet)
{
  packet.magic   = LORA_TELEMETRY_MAGIC;
  packet.version = LORA_PROTOCOL_VERSION;
  packet.checksum = LoraChecksum(&packet, offsetof(TelemetryPacket, checksum));
}

bool IsTelemetryValid(const TelemetryPacket &packet)
{
  return packet.magic == LORA_TELEMETRY_MAGIC &&
         packet.version == LORA_PROTOCOL_VERSION &&
         packet.checksum == LoraChecksum(&packet, offsetof(TelemetryPacket, checksum));
}

void SealCommand(CommandPacket &packet)
{
  packet.magic   = LORA_COMMAND_MAGIC;
  packet.version = LORA_PROTOCOL_VERSION;
  packet.checksum = LoraChecksum(&packet, offsetof(CommandPacket, checksum));
}

bool IsCommandValid(const CommandPacket &packet, uint16_t expected_device_id)
{
  // The device id is checked here as well as the checksum: on a shared band,
  // a well-formed frame meant for another dryer must not be obeyed.
  return packet.magic == LORA_COMMAND_MAGIC &&
         packet.version == LORA_PROTOCOL_VERSION &&
         packet.device_id == expected_device_id &&
         packet.checksum == LoraChecksum(&packet, offsetof(CommandPacket, checksum));
}

bool LoraCommandFilter::ShouldExecute(uint8_t sequence)
{
  if (!primed_)
  {
    primed_ = true;
    last_sequence_ = sequence;
    return true;
  }

  // Sequence numbers wrap at 256, so "newer" is a signed distance rather than a
  // plain comparison: without this, the counter rolling over would freeze the
  // link until it caught up again.
  int8_t distance = static_cast<int8_t>(sequence - last_sequence_);
  if (distance <= 0)
  {
    return false; // already executed, or an old frame arriving late
  }

  last_sequence_ = sequence;
  return true;
}
