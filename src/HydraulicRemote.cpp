#include "config.h"
#include "HydraulicRemote.h"
#include "Logger.h"

HydraulicRemote::HydraulicRemote(Rs485Bus *bus)
    : RemoteModule(bus, MODBUS_HYDRAULIC_ADDRESS, "HydraulicRemote",
                   kTimeoutMs, kRetryMs),
      enabled_(false),
      water_target_(WATER_TARGET_DEFAULT),
      water_temperature_(NAN),
      tank_temperature_(NAN),
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
  // Permission block: run permission then water setpoint, written in one FC16
  // transaction so the module never sees the permission raised with a stale
  // setpoint.
  uint16_t command[2];
  command[0] = enabled_ ? 1 : 0;
  command[1] = static_cast<uint16_t>(lroundf(water_target_ * 10.0f));

  uint16_t telemetry[3] = {0, 0, 0};

  if (!Exchange(HYDRO_REG_STATE, command, 2,
                HYDRO_REG_WATER_TEMP, telemetry, 3))
  {
    return false;
  }

  // Temperatures are signed: the module may report below zero.
  water_temperature_ = static_cast<int16_t>(telemetry[0]) / 10.0f;
  tank_temperature_  = static_cast<int16_t>(telemetry[1]) / 10.0f;
  status_bits_       = telemetry[2];
  return true;
}
