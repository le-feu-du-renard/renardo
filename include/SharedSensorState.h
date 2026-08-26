#ifndef SHARED_SENSOR_STATE_H
#define SHARED_SENSOR_STATE_H

#include <Arduino.h>
#include "Seqlock.h"

// Everything Core 1 collects across both RS485 buses and Core 0 consumes.
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

// The crossing itself is in Seqlock.h — the extension port needs the same
// protocol for its two crossings in the other direction.
using SharedSensorState = Seqlock<SensorSnapshot>;

#endif // SHARED_SENSOR_STATE_H
