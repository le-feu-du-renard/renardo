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
  // --- Progress bar and header ---
  uint32_t total_elapsed_s;

  // DryerPhase, held as its underlying type so this header stays independent of
  // SessionManager. The renderer maps it to a screen label and a colour: naming
  // the phase and colouring it are one decision, and a string could not carry
  // the second half of it.
  uint8_t phase;

  // How far the current phase has run against its configured duration, 0..1,
  // NAN when nothing is running.
  //
  // An estimate, not a countdown: brassage and extraction can both end early on
  // a humidity threshold rather than on the clock, so the bar says "this phase
  // is well along", never "this many minutes remain".
  float phase_progress;

  bool running;

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
  // How many registers this dryer is configured for. With one, the recycling
  // cell says so outright instead of showing dashes: no feedback because there
  // is nothing to read reads very differently from no feedback because the wire
  // is dead, and the screen should not blur the two.
  uint8_t damper_count;

  // --- Eco mode ---
  // Two flags rather than one, because armed and acting are different things
  // the operator needs to tell apart. Eco lowers the setpoint only inside its
  // configured hours, and `target_temperature` above is already the lowered
  // figure: without `eco_window` on screen, the setpoint card would simply drop
  // one evening with nothing anywhere saying why.
  //
  // `eco_enabled` is the running mode, not the stored setting: it is false when
  // no RTC answered at boot, whatever the configuration says, because eco
  // cannot know what time it is.
  bool eco_enabled;
  bool eco_window;

  // --- Alarms ---
  bool sensor_fault;          // inlet probe stale, heating blocked
  bool airflow_fault;         // both registers shut: session refused and stopped
  bool damper_feedback_fault; // two registers declared, a feedback unusable

  DisplayModel()
      : total_elapsed_s(0),
        phase(0),
        phase_progress(NAN),
        running(false),
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
        damper_count(1),
        eco_enabled(false),
        eco_window(false),
        sensor_fault(false),
        airflow_fault(false),
        damper_feedback_fault(false) {}
};

#endif // DISPLAY_MODEL_H
