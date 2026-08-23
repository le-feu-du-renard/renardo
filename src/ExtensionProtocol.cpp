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

uint16_t ExtEncodePosition(float percent)
{
  if (isnan(percent))
  {
    return kExtNoPosition;
  }

  long whole = lroundf(percent);
  if (whole < 0)
  {
    whole = 0;
  }
  if (whole > 100)
  {
    whole = 100;
  }
  return static_cast<uint16_t>(whole);
}

float ExtDecodePosition(uint16_t raw)
{
  if (raw == kExtNoPosition)
  {
    return NAN;
  }
  return static_cast<float>(raw);
}

uint16_t ExtEncodeFlags(const ExtensionTelemetry &telemetry)
{
  uint16_t flags = 0;

  if (telemetry.running)        flags |= kExtFlagRunning;
  if (telemetry.fan_on)         flags |= kExtFlagFan;
  if (telemetry.electric_on)    flags |= kExtFlagElectric;
  if (telemetry.hydraulic_demand)   flags |= kExtFlagHydraulic;
  if (telemetry.damper_open)    flags |= kExtFlagDamperOpen;
  if (telemetry.sensor_fault)   flags |= kExtFlagSensorFault;
  if (telemetry.airflow_fault)  flags |= kExtFlagAirflowFault;
  if (telemetry.feedback_fault) flags |= kExtFlagFeedbackFault;

  // Reported as "the module is off the bus", so the bit is the negation of the
  // one the rest of the firmware carries.
  if (!telemetry.hydraulic_online) flags |= kExtFlagHydraulicOff;

  return flags;
}

void ExtEncodeTelemetry(const ExtensionTelemetry &telemetry, uint16_t *out)
{
  if (out == nullptr)
  {
    return;
  }

  out[kExtRegVersion] = EXT_PROTOCOL_VERSION;
  out[kExtRegFlags]   = ExtEncodeFlags(telemetry);
  out[kExtRegPhase]   = telemetry.phase;

  // Signed tenths ride in the register as their two's-complement bit pattern;
  // the module casts them back to int16_t.
  out[kExtRegInletTemp]      = static_cast<uint16_t>(ExtEncodeValue(telemetry.inlet_temperature));
  out[kExtRegInletHumidity]  = static_cast<uint16_t>(ExtEncodeValue(telemetry.inlet_humidity));
  out[kExtRegWaterTemp]      = static_cast<uint16_t>(ExtEncodeValue(telemetry.water_temperature));
  out[kExtRegTankTemp]       = static_cast<uint16_t>(ExtEncodeValue(telemetry.tank_temperature));
  out[kExtRegTargetTemp]     = static_cast<uint16_t>(ExtEncodeValue(telemetry.target_temperature));
  out[kExtRegTargetHumidity] = static_cast<uint16_t>(ExtEncodeValue(telemetry.target_humidity));

  out[kExtRegExtractionPos] = ExtEncodePosition(telemetry.extraction_position);
  out[kExtRegRecyclingPos]  = ExtEncodePosition(telemetry.recycling_position);

  out[kExtRegElapsedHigh] = static_cast<uint16_t>(telemetry.session_elapsed_s >> 16);
  out[kExtRegElapsedLow]  = static_cast<uint16_t>(telemetry.session_elapsed_s & 0xFFFF);
  out[kExtRegUptimeHigh]  = static_cast<uint16_t>(telemetry.uptime_s >> 16);
  out[kExtRegUptimeLow]   = static_cast<uint16_t>(telemetry.uptime_s & 0xFFFF);

  out[kExtRegAckSequence] = telemetry.ack_sequence;
  out[kExtRegAckResult]   = telemetry.ack_result;
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
