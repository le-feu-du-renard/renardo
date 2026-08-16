#ifndef MODBUS_SENSORS_H
#define MODBUS_SENSORS_H

#include <Arduino.h>
#include "Rs485Bus.h"

// State of one RS485 temperature/humidity probe.
struct SensorReading
{
  float    temperature;      // C, NAN until the first successful read
  float    humidity;         // %RH, NAN until the first successful read
  uint32_t last_success_ms;  // millis() of the last successful read
  uint16_t error_count;      // consecutive failures since the last success
  bool     valid;            // true once at least one read has succeeded

  SensorReading()
      : temperature(NAN), humidity(NAN), last_success_ms(0),
        error_count(0), valid(false) {}
};

// RS485 Modbus RTU interface for the SHT20/SHT30-type probes on bus A.
// Register map and scale factor come from config.h:
//   MODBUS_REG_HUMIDITY    : humidity    (raw / MODBUS_RAW_SCALE = %RH)
//   MODBUS_REG_TEMPERATURE : temperature (raw / MODBUS_RAW_SCALE = C)
// Both registers are read in a single FC03 transaction.
//
// Each probe keeps its own error counter and success timestamp. In v3 a single
// counter was shared by both probes, so a success on one silently reset the
// failure count of the other and no staleness could ever be detected.
class ModbusSensors
{
public:
  static constexpr uint8_t kInlet  = 0;
  static constexpr uint8_t kOutlet = 1;
  static constexpr uint8_t kCount  = 2;

  explicit ModbusSensors(Rs485Bus *bus);

  void Begin();

  // Poll one probe and update its slot. Returns true on success.
  // On failure the previous values are kept and the error counter grows.
  bool Poll(uint8_t index);

  const SensorReading &GetReading(uint8_t index) const;

  // True when the probe has produced a value within `timeout_ms`.
  // A probe that never answered is never fresh.
  bool IsFresh(uint8_t index, uint32_t timeout_ms) const;

private:
  Rs485Bus     *bus_;
  uint8_t       addresses_[kCount];
  SensorReading readings_[kCount];
  SensorReading invalid_;  // returned for out-of-range indices

  static constexpr uint16_t kMaxErrors = 100;
};

#endif // MODBUS_SENSORS_H
