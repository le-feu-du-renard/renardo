#include "HydraulicProtocol.h"

int16_t HydroEncodeValue(float value)
{
  if (isnan(value))
  {
    return kHydroInvalidValue;
  }

  long tenths = lroundf(value * 10.0f);

  // Clamp short of the sentinel at the bottom end, so a wild reading cannot
  // encode itself as "no reading".
  if (tenths <= static_cast<long>(kHydroInvalidValue))
  {
    return kHydroInvalidValue + 1;
  }
  if (tenths > INT16_MAX)
  {
    return INT16_MAX;
  }
  return static_cast<int16_t>(tenths);
}

float HydroDecodeValue(int16_t raw)
{
  if (raw == kHydroInvalidValue)
  {
    return NAN;
  }
  return raw / 10.0f;
}

uint16_t HydroEncodeSpeed(float percent)
{
  if (isnan(percent))
  {
    return kHydroNoSpeed;
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

float HydroDecodeSpeed(uint16_t raw)
{
  if (raw == kHydroNoSpeed)
  {
    return NAN;
  }
  return static_cast<float>(raw);
}

uint16_t HydroEncodeStatus(const HydraulicTelemetry &telemetry)
{
  uint16_t bits = 0;

  if (telemetry.circulating)       bits |= kHydroFlagCirculating;
  if (telemetry.permission)        bits |= kHydroFlagPermission;
  if (telemetry.tank_too_cold)     bits |= kHydroFlagTankTooCold;
  if (telemetry.water_probe_fault) bits |= kHydroFlagWaterProbeFault;
  if (telemetry.tank_probe_fault)  bits |= kHydroFlagTankProbeFault;
  if (telemetry.watchdog_tripped)  bits |= kHydroFlagWatchdogTripped;
  if (telemetry.setpoint_missed)   bits |= kHydroFlagSetpointMissed;

  return bits;
}

void HydroDecodeStatus(uint16_t bits, HydraulicTelemetry &telemetry)
{
  telemetry.circulating       = (bits & kHydroFlagCirculating) != 0;
  telemetry.permission        = (bits & kHydroFlagPermission) != 0;
  telemetry.tank_too_cold     = (bits & kHydroFlagTankTooCold) != 0;
  telemetry.water_probe_fault = (bits & kHydroFlagWaterProbeFault) != 0;
  telemetry.tank_probe_fault  = (bits & kHydroFlagTankProbeFault) != 0;
  telemetry.watchdog_tripped  = (bits & kHydroFlagWatchdogTripped) != 0;
  telemetry.setpoint_missed   = (bits & kHydroFlagSetpointMissed) != 0;
}

void HydroEncodeCommand(const HydraulicCommand &command, uint16_t *registers)
{
  registers[kHydroCmdRegState] = command.run_permitted ? 1 : 0;
  registers[kHydroCmdRegDryerAirTemp] =
      static_cast<uint16_t>(HydroEncodeValue(command.dryer_air_temperature));
}

void HydroDecodeCommand(const uint16_t *registers, HydraulicCommand &command)
{
  // Any non-zero counts as permission. The dryer only ever writes 0 or 1, but a
  // module that refused to run on a 2 would be failing in the dangerous
  // direction for a reason nobody could see on the wire.
  command.run_permitted = registers[kHydroCmdRegState] != 0;

  command.dryer_air_temperature =
      HydroDecodeValue(static_cast<int16_t>(registers[kHydroCmdRegDryerAirTemp]));
}

void HydroEncodeTelemetry(const HydraulicTelemetry &telemetry, uint16_t *registers)
{
  registers[kHydroRegWaterTemp] =
      static_cast<uint16_t>(HydroEncodeValue(telemetry.water_temperature));
  registers[kHydroRegTankTemp] =
      static_cast<uint16_t>(HydroEncodeValue(telemetry.tank_temperature));
  registers[kHydroRegStatus] = HydroEncodeStatus(telemetry);
  registers[kHydroRegPumpSpeed] = HydroEncodeSpeed(telemetry.pump_speed_percent);
}

void HydroDecodeTelemetry(const uint16_t *registers, HydraulicTelemetry &telemetry)
{
  telemetry.water_temperature =
      HydroDecodeValue(static_cast<int16_t>(registers[kHydroRegWaterTemp]));
  telemetry.tank_temperature =
      HydroDecodeValue(static_cast<int16_t>(registers[kHydroRegTankTemp]));
  HydroDecodeStatus(registers[kHydroRegStatus], telemetry);
  telemetry.pump_speed_percent = HydroDecodeSpeed(registers[kHydroRegPumpSpeed]);
}
