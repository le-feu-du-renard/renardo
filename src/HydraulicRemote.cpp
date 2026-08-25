#include "config.h"
#include "HydraulicProtocol.h"
#include "HydraulicRemote.h"
#include "Logger.h"

HydraulicRemote::HydraulicRemote(Rs485Bus *bus)
    : RemoteModule(bus, MODBUS_HYDRAULIC_ADDRESS, "HydraulicRemote",
                   kTimeoutMs, kRetryMs),
      enabled_(false),
      water_target_(WATER_TARGET_DEFAULT),
      dryer_air_temperature_(NAN),
      water_temperature_(NAN),
      tank_temperature_(NAN),
      pump_speed_percent_(NAN),
      status_bits_(0) {}

void HydraulicRemote::Begin()
{
  Logger::Info("HydraulicRemote: module @%d on bus %s, water target %F C",
               GetAddress(), bus()->GetName(), water_target_);
}

void HydraulicRemote::SetWaterTarget(float celsius)
{
  water_target_ = constrain(celsius, WATER_TARGET_MIN, WATER_TARGET_MAX);
}

bool HydraulicRemote::Update()
{
  // Command block: permission, water setpoint and dryer air temperature,
  // written in one FC16 transaction so the module never sees the permission
  // raised with a stale setpoint.
  HydraulicCommand command;
  command.run_permitted         = enabled_;
  command.water_target           = water_target_;
  command.dryer_air_temperature = dryer_air_temperature_;

  uint16_t command_regs[HYDRO_COMMAND_COUNT];
  HydroEncodeCommand(command, command_regs);

  uint16_t telemetry_regs[HYDRO_TELEMETRY_COUNT];

  if (!Exchange(HYDRO_REG_STATE, command_regs, HYDRO_COMMAND_COUNT,
                HYDRO_REG_WATER_TEMP, telemetry_regs, HYDRO_TELEMETRY_COUNT))
  {
    return false;
  }

  HydraulicTelemetry telemetry;
  HydroDecodeTelemetry(telemetry_regs, telemetry);

  water_temperature_  = telemetry.water_temperature;
  tank_temperature_   = telemetry.tank_temperature;
  pump_speed_percent_ = telemetry.pump_speed_percent;
  status_bits_        = HydroEncodeStatus(telemetry);
  return true;
}
