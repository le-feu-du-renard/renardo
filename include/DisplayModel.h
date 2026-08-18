#ifndef DISPLAY_MODEL_H
#define DISPLAY_MODEL_H

#include <Arduino.h>

// Everything the main screen shows, gathered in one place.
//
// The renderer is a pure function of this struct: main.cpp fills it from the
// managers, TftDisplay compares it against the previous frame and redraws only
// the regions whose contents actually changed. Keeping it a plain value type is
// what makes that comparison cheap and the layout code free of manager
// dependencies.
struct DisplayModel
{
  // --- Status bar ---
  uint32_t    total_elapsed_s;
  const char *phase_name;
  bool        running;
  bool        lora_linked;

  // --- Measurement and setpoint tiles ---
  float inlet_temperature;
  float inlet_humidity;
  float target_temperature;
  float target_humidity;

  // --- Hydraulic block ---
  bool  hydraulic_online;   // remote module answering
  bool  hydraulic_enabled;  // menu toggle
  bool  hydraulic_on;       // circulator currently running
  float water_temperature;
  float tank_temperature;

  // --- Status band ---
  bool fan_on;
  bool fan_cooling;         // post-stop cooldown: icon blinks
  bool electric_on;
  bool electric_enabled;
  bool damper_open;         // commanded air path: true = extraction
  // Measured opening of each register, %, NAN when there is no usable feedback.
  // Both are shown at once: the two registers are asymmetric, so one figure
  // would not describe the other, and comparing them is how a jammed vane or a
  // dead feedback wire becomes visible at a glance.
  float extraction_position;
  bool  extraction_moving;
  float recycling_position;
  bool  recycling_moving;

  // --- Alarms ---
  bool sensor_fault;        // inlet probe stale, heating blocked

  DisplayModel()
      : total_elapsed_s(0),
        phase_name("ARRET"),
        running(false),
        lora_linked(false),
        inlet_temperature(NAN),
        inlet_humidity(NAN),
        target_temperature(NAN),
        target_humidity(NAN),
        hydraulic_online(false),
        hydraulic_enabled(false),
        hydraulic_on(false),
        water_temperature(NAN),
        tank_temperature(NAN),
        fan_on(false),
        fan_cooling(false),
        electric_on(false),
        electric_enabled(false),
        damper_open(false),
        extraction_position(NAN),
        extraction_moving(false),
        recycling_position(NAN),
        recycling_moving(false),
        sensor_fault(false) {}
};

#endif // DISPLAY_MODEL_H
