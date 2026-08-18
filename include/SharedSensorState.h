#ifndef SHARED_SENSOR_STATE_H
#define SHARED_SENSOR_STATE_H

#include <Arduino.h>
#include <hardware/sync.h>

// Everything Core 1 collects on RS485 bus A and Core 0 consumes.
struct SensorSnapshot
{
  float inlet_temperature;
  float inlet_humidity;

  float water_temperature;  // hydraulic module, circulating water
  float tank_temperature;   // hydraulic module, storage tank

  // millis() of the last successful probe read. millis() is driven by the same
  // timer on both cores, so Core 0 can compare this against SENSOR_TIMEOUT_MS
  // to decide whether heating may run.
  uint32_t inlet_updated_ms;

  bool inlet_valid;
  bool hydraulic_available;

  SensorSnapshot()
      : inlet_temperature(NAN), inlet_humidity(NAN),
        water_temperature(NAN), tank_temperature(NAN),
        inlet_updated_ms(0),
        inlet_valid(false), hydraulic_available(false) {}
};

// Single-writer / single-reader seqlock for passing a snapshot between cores.
//
// v3 shared four bare volatile floats, so Core 0 could observe a half-updated
// set and had no way to tell a fresh value from one frozen by a dead probe.
// The sequence counter is odd while a write is in flight; a reader that sees an
// odd counter, or a counter that changed across the copy, retries.
//
// The RP2040 cores share SRAM with no data cache, so plain data memory barriers
// are enough to order the counter against the payload.
class SharedSensorState
{
public:
  SharedSensorState() : sequence_(0) {}

  // Called by the owning core (Core 1) only.
  void Publish(const SensorSnapshot &snapshot)
  {
    sequence_ = sequence_ + 1;  // now odd: write in progress
    __dmb();
    data_ = snapshot;
    __dmb();
    sequence_ = sequence_ + 1;  // now even: write complete
  }

  // Called by the reading core (Core 0). Blocks only for the duration of a
  // concurrent write, which is a handful of instructions.
  void Read(SensorSnapshot &out) const
  {
    while (true)
    {
      uint32_t before = sequence_;
      if (before & 1u)
      {
        continue;  // write in progress, retry
      }
      __dmb();
      out = data_;
      __dmb();
      if (before == sequence_)
      {
        return;  // no write started or finished while we copied
      }
    }
  }

private:
  volatile uint32_t sequence_;
  SensorSnapshot    data_;
};

#endif // SHARED_SENSOR_STATE_H
