#include "ExtensionProtocol.h"

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
