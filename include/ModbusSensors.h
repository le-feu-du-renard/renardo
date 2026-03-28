#ifndef MODBUS_SENSORS_H
#define MODBUS_SENSORS_H

#include <Arduino.h>
#include <ModbusMaster.h>

// RS485 Modbus RTU sensor interface for SHT20/SHT30-type sensors.
// Register map and scale factor defined in config.h:
//   MODBUS_REG_TEMPERATURE : temperature (raw / MODBUS_RAW_SCALE = C)
//   MODBUS_REG_HUMIDITY    : humidity    (raw / MODBUS_RAW_SCALE = %RH)
// Function code 0x04 (Read Input Registers) is used.
//
// Connections (pins defined in config.h):
//   UART1 TX  -> RS485_TX_PIN
//   UART1 RX  -> RS485_RX_PIN
//   DE/RE pin -> RS485_DE_PIN  (HIGH = transmit, LOW = receive)

class ModbusSensors
{
public:
  ModbusSensors();

  // Configure UART1 and direction-control pin, then enable receive mode.
  void Begin(uint32_t baudrate);

  // Read temperature and humidity from one sensor at the given Modbus address.
  // Returns true on success; on failure the output values are left unchanged.
  bool ReadSensor(uint8_t address, float &temperature, float &humidity);

  // Return the number of consecutive read errors since the last success.
  uint8_t GetErrorCount() const { return error_count_; }

private:
  ModbusMaster node_;
  uint8_t      error_count_;

  // Called by ModbusMaster before/after each transmission to toggle DE/RE.
  static void PreTransmission();
  static void PostTransmission();
};

#endif // MODBUS_SENSORS_H
