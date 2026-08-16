#include "config.h"
#include "ModbusSensors.h"
#include "Logger.h"

ModbusSensors::ModbusSensors(Rs485Bus *bus) : bus_(bus)
{
  addresses_[kInlet]  = MODBUS_INLET_ADDRESS;
  addresses_[kOutlet] = MODBUS_OUTLET_ADDRESS;
}

void ModbusSensors::Begin()
{
  Logger::Info("ModbusSensors: inlet @%d, outlet @%d on bus %s",
               addresses_[kInlet], addresses_[kOutlet], bus_->GetName());
}

bool ModbusSensors::Poll(uint8_t index)
{
  if (index >= kCount || bus_ == nullptr)
  {
    return false;
  }

  SensorReading &reading = readings_[index];
  uint16_t raw[2] = {0, 0};

  if (!bus_->ReadHoldingRegisters(addresses_[index], MODBUS_REG_HUMIDITY, 2, raw))
  {
    if (reading.error_count < kMaxErrors)
    {
      reading.error_count++;
    }
    Logger::Warning("ModbusSensors: read failed @%d (error 0x%02X, count %d)",
                    addresses_[index], bus_->GetLastError(), reading.error_count);
    return false;
  }

  // Register layout: 0x0000 = humidity, 0x0001 = temperature
  reading.humidity        = static_cast<float>(raw[0]) / MODBUS_RAW_SCALE;
  reading.temperature     = static_cast<float>(raw[1]) / MODBUS_RAW_SCALE;
  reading.last_success_ms = millis();
  reading.error_count     = 0;
  reading.valid           = true;
  return true;
}

const SensorReading &ModbusSensors::GetReading(uint8_t index) const
{
  if (index >= kCount)
  {
    return invalid_;
  }
  return readings_[index];
}

bool ModbusSensors::IsFresh(uint8_t index, uint32_t timeout_ms) const
{
  if (index >= kCount)
  {
    return false;
  }

  const SensorReading &reading = readings_[index];
  if (!reading.valid)
  {
    return false;
  }
  return (millis() - reading.last_success_ms) < timeout_ms;
}
