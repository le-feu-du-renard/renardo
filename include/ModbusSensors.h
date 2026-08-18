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

// RS485 Modbus RTU interface for the SHT20/SHT30-type inlet probe.
// Register map and scale factor come from config.h:
//   MODBUS_REG_HUMIDITY    : humidity    (raw / MODBUS_RAW_SCALE = %RH)
//   MODBUS_REG_TEMPERATURE : temperature (raw / MODBUS_RAW_SCALE = C)
// Both registers are read in a single FC03 transaction.
//
// One probe, addressed by MODBUS_INLET_ADDRESS. v3 and early v4 also carried an
// outlet probe, reached through an index on every call; it never fed a control
// decision, so the index went with it. The error counter and the success
// timestamp stay, because they are what tells a stale reading from a live one —
// v3 shared a single counter between the two probes, so a success on one reset
// the other's failure count and staleness could never be detected at all.
class ModbusSensors
{
public:
  explicit ModbusSensors(Rs485Bus *bus);

  void Begin();

  // Poll the probe. Returns true on success; on failure the previous values are
  // kept and the error counter grows.
  bool Poll();

  const SensorReading &GetReading() const { return reading_; }

  // True when the probe has produced a value within `timeout_ms`.
  // A probe that never answered is never fresh.
  bool IsFresh(uint32_t timeout_ms) const;

private:
  Rs485Bus     *bus_;
  uint8_t       address_;
  SensorReading reading_;

  static constexpr uint16_t kMaxErrors = 100;
};

#endif // MODBUS_SENSORS_H
