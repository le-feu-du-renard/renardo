#include "config.h"
#include "HydraulicRemote.h"
#include "Logger.h"

HydraulicRemote::HydraulicRemote(Rs485Bus *bus)
    : bus_(bus),
      requested_state_(false),
      water_target_(WATER_TARGET_DEFAULT),
      water_temperature_(NAN),
      tank_temperature_(NAN),
      status_bits_(0),
      last_success_ms_(0),
      error_count_(0),
      valid_(false) {}

void HydraulicRemote::Begin()
{
  Logger::Info("HydraulicRemote: module @%d on bus %s, water target %F C",
               MODBUS_HYDRAULIC_ADDRESS, bus_->GetName(), water_target_);
}

void HydraulicRemote::SetWaterTarget(float celsius)
{
  water_target_ = constrain(celsius, WATER_TARGET_MIN, WATER_TARGET_MAX);
}

bool HydraulicRemote::Update()
{
  if (bus_ == nullptr)
  {
    return false;
  }

  // Command block: state then water setpoint, written in one FC16 transaction
  // so the module never sees a state change with a stale setpoint.
  uint16_t command[2];
  command[0] = requested_state_ ? 1 : 0;
  command[1] = static_cast<uint16_t>(lroundf(water_target_ * 10.0f));

  bool ok = bus_->WriteMultipleRegisters(MODBUS_HYDRAULIC_ADDRESS,
                                         HYDRO_REG_STATE, 2, command);
  if (ok)
  {
    uint16_t telemetry[3] = {0, 0, 0};
    ok = bus_->ReadHoldingRegisters(MODBUS_HYDRAULIC_ADDRESS,
                                    HYDRO_REG_WATER_TEMP, 3, telemetry);
    if (ok)
    {
      // Temperatures are signed: the module may report below zero.
      water_temperature_ = static_cast<int16_t>(telemetry[0]) / 10.0f;
      tank_temperature_  = static_cast<int16_t>(telemetry[1]) / 10.0f;
      status_bits_       = telemetry[2];
    }
  }

  if (!ok)
  {
    if (error_count_ < kMaxErrors)
    {
      error_count_++;
    }
    Logger::Warning("HydraulicRemote: exchange failed @%d (error 0x%02X, count %d)",
                    MODBUS_HYDRAULIC_ADDRESS, bus_->GetLastError(), error_count_);
    return false;
  }

  last_success_ms_ = millis();
  error_count_     = 0;
  valid_           = true;
  return true;
}

bool HydraulicRemote::IsAvailable() const
{
  if (!valid_)
  {
    return false;
  }
  return (millis() - last_success_ms_) < kTimeoutMs;
}
