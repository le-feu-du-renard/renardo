#ifndef I2C_SENSOR_H
#define I2C_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include "ISensor.h"

// DFRobot SEN0546 (CHT8305) temperature + humidity sensor over I2C.
// Protocol: write reg 0x00, wait 20 ms, read 4 bytes (no CRC).
class I2CSensor : public ISensor
{
public:
  I2CSensor(TwoWire &bus, uint8_t address);

  bool Begin() override;
  bool Read(float &temperature, float &humidity) override;

  uint8_t GetErrorCount() const { return error_count_; }

private:
  static constexpr uint8_t  kMaxErrors  = 5;
  static constexpr uint32_t kBusClock   = 10000;
  static constexpr uint16_t kBusTimeout = 1000;

  TwoWire &bus_;
  uint8_t  address_;
  uint8_t  error_count_;

  void RecoverBus();
};

#endif // I2C_SENSOR_H
