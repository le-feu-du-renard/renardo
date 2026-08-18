#include "config.h"
#include "ModbusSensors.h"
#include "Logger.h"

ModbusSensors::ModbusSensors(Rs485Bus *bus)
    : bus_(bus), address_(MODBUS_INLET_ADDRESS) {}

void ModbusSensors::Begin()
{
  Logger::Info("ModbusSensors: inlet @%d on bus %s", address_, bus_->GetName());
}

bool ModbusSensors::Poll()
{
  if (bus_ == nullptr)
  {
    return false;
  }

  uint16_t raw[2] = {0, 0};

  if (!bus_->ReadHoldingRegisters(address_, MODBUS_REG_HUMIDITY, 2, raw))
  {
    if (reading_.error_count < kMaxErrors)
    {
      reading_.error_count++;
    }
    Logger::Warning("ModbusSensors: read failed @%d (error %X, count %d)",
                    address_, bus_->GetLastError(), reading_.error_count);
    return false;
  }

  // Register layout: 0x0000 = humidity, 0x0001 = temperature
  reading_.humidity        = static_cast<float>(raw[0]) / MODBUS_RAW_SCALE;
  reading_.temperature     = static_cast<float>(raw[1]) / MODBUS_RAW_SCALE;
  reading_.last_success_ms = millis();
  reading_.error_count     = 0;
  reading_.valid           = true;
  return true;
}

bool ModbusSensors::IsFresh(uint32_t timeout_ms) const
{
  if (!reading_.valid)
  {
    return false;
  }
  return (millis() - reading_.last_success_ms) < timeout_ms;
}
