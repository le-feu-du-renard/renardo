#include "config.h"
#include "HydraulicProtocol.h"
#include "HydraulicRemote.h"
#include "Logger.h"

HydraulicRemote::HydraulicRemote(Rs485Bus *bus)
    : RemoteModule(bus, MODBUS_HYDRAULIC_ADDRESS, "HydraulicRemote",
                   kTimeoutMs, kRetryMs),
      enabled_(false),
      dryer_air_temperature_(NAN),
      water_temperature_(NAN),
      tank_temperature_(NAN),
      pump_speed_percent_(NAN),
      status_bits_(0) {}

void HydraulicRemote::Begin()
{
  Logger::Info("HydraulicRemote: module @%d on bus %s",
               GetAddress(), bus()->GetName());
}

bool HydraulicRemote::Update()
{
  // Command block: permission and dryer air temperature,
  // written in one FC16 transaction so the module never sees the permission
  // raised with a stale setpoint.
  HydraulicCommand command;
  command.run_permitted         = enabled_;
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
