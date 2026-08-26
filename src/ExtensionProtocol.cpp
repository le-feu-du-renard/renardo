#include "ExtensionProtocol.h"

#include <string.h>

int16_t ExtEncodeValue(float value)
{
  if (isnan(value))
  {
    return kExtInvalidValue;
  }

  long tenths = lroundf(value * 10.0f);

  // Clamp short of the sentinel at the bottom end, so a wild reading cannot
  // encode itself as "no reading".
  if (tenths <= static_cast<long>(kExtInvalidValue))
  {
    return kExtInvalidValue + 1;
  }
  if (tenths > INT16_MAX)
  {
    return INT16_MAX;
  }
  return static_cast<int16_t>(tenths);
}

float ExtDecodeValue(int16_t raw)
{
  if (raw == kExtInvalidValue)
  {
    return NAN;
  }
  return raw / 10.0f;
}

bool ExtPutMetricValue(ExtensionTelemetryRecord &record, uint16_t metric_id, float value)
{
  if (record.metric_count >= kExtMaxMetrics)
  {
    return false;
  }

  ExtensionMetricTuple &tuple = record.metrics[record.metric_count++];
  tuple.metric_id = (metric_id & kExtMetricIdMask) | kExtMetricKindTenths;
  tuple.value     = static_cast<uint16_t>(ExtEncodeValue(value));
  return true;
}

bool ExtPutMetricCounter(ExtensionTelemetryRecord &record, uint16_t metric_id, uint32_t value)
{
  if (record.metric_count >= kExtMaxMetrics)
  {
    return false;
  }

  ExtensionMetricTuple &tuple = record.metrics[record.metric_count++];
  tuple.metric_id = (metric_id & kExtMetricIdMask) | kExtMetricKindCounter;
  // Whole units, wrapping. See kExtMetricKindCounter's comment in the header.
  tuple.value = static_cast<uint16_t>(value & 0xFFFFu);
  return true;
}

void ExtEncodeCatalogName(const char *name, uint16_t *out)
{
  if (out == nullptr)
  {
    return;
  }

  // Empty registers, then overlay as much of `name` as fits — a short name
  // is zero-padded, a long one is truncated at kExtCatalogNameChars, and
  // neither case reads past `name`'s own terminator first.
  for (uint8_t i = 0; i < kExtCatalogNameRegisters; i++)
  {
    out[i] = 0;
  }

  if (name == nullptr)
  {
    return;
  }

  const size_t length = strnlen(name, kExtCatalogNameChars);
  for (size_t i = 0; i < length; i++)
  {
    const uint8_t byte = static_cast<uint8_t>(name[i]);
    if ((i % 2) == 0)
    {
      out[i / 2] = static_cast<uint16_t>(byte) << 8;
    }
    else
    {
      out[i / 2] |= byte;
    }
  }
}

void ExtDecodeCatalogName(const uint16_t *in, char *out)
{
  if (out == nullptr)
  {
    return;
  }

  if (in == nullptr)
  {
    out[0] = '\0';
    return;
  }

  for (uint8_t i = 0; i < kExtCatalogNameChars; i++)
  {
    const uint16_t reg = in[i / 2];
    const uint8_t  byte = ((i % 2) == 0) ? static_cast<uint8_t>(reg >> 8)
                                          : static_cast<uint8_t>(reg & 0xFF);
    // A padding zero ends the name early, same as a normal C string would —
    // whichever comes first, a real null byte or running out of registers.
    if (byte == 0)
    {
      out[i] = '\0';
      return;
    }
    out[i] = static_cast<char>(byte);
  }
  out[kExtCatalogNameChars] = '\0';
}

size_t ExtEncodeTelemetry(const ExtensionTelemetryRecord &record, uint16_t *out)
{
  if (out == nullptr)
  {
    return 0;
  }

  out[kExtRegVersion]    = EXT_PROTOCOL_VERSION;
  out[kExtRegCount]      = record.metric_count;
  out[kExtRegUptimeHigh] = static_cast<uint16_t>(record.uptime_s >> 16);
  out[kExtRegUptimeLow]  = static_cast<uint16_t>(record.uptime_s & 0xFFFF);
  out[kExtRegAckSequence] = record.ack_sequence;
  out[kExtRegAckResult]   = record.ack_result;

  out[kExtRegCatalogId] = record.catalog_metric_id;
  ExtEncodeCatalogName(record.catalog_metric_name, &out[kExtRegCatalogName]);

  for (uint8_t i = 0; i < record.metric_count; i++)
  {
    const size_t offset = kExtTelemetryHeaderCount + (static_cast<size_t>(i) * 2);
    out[offset]     = record.metrics[i].metric_id;
    out[offset + 1] = record.metrics[i].value;
  }

  return kExtTelemetryHeaderCount + (static_cast<size_t>(record.metric_count) * 2);
}

ExtensionResult ExtDecodeCommand(const uint16_t *in, ExtensionCommand &command)
{
  command = ExtensionCommand();

  if (in == nullptr)
  {
    return kExtResultOk;
  }

  command.sequence = in[kExtCmdRegSequence];

  // Empty mailbox: nothing to run, nothing to refuse. Checked before the version
  // so a module that has not written anything yet — every register zero — is not
  // answered with a version complaint.
  if (command.sequence == 0 || in[kExtCmdRegOpcode] == kExtCmdNone)
  {
    return kExtResultOk;
  }

  if (in[kExtCmdRegVersion] != EXT_PROTOCOL_VERSION)
  {
    return kExtResultBadVersion;
  }

  command.opcode   = in[kExtCmdRegOpcode];
  command.argument = ExtDecodeValue(static_cast<int16_t>(in[kExtCmdRegArgument]));

  switch (command.opcode)
  {
  case kExtCmdStart:
    // Understood, and refused every time. Starting a session stays a physical
    // gesture in front of the machine.
    return kExtResultRefused;

  case kExtCmdStop:
    return kExtResultOk;

  case kExtCmdSetTemp:
    if (isnan(command.argument) ||
        command.argument < TARGET_TEMP_MIN || command.argument > TARGET_TEMP_MAX)
    {
      return kExtResultOutOfRange;
    }
    return kExtResultOk;

  case kExtCmdSetHumidity:
    if (isnan(command.argument) ||
        command.argument < TARGET_HUM_MIN || command.argument > TARGET_HUM_MAX)
    {
      return kExtResultOutOfRange;
    }
    return kExtResultOk;

  default:
    return kExtResultUnknownOpcode;
  }
}
