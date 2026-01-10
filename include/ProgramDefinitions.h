#ifndef PROGRAM_DEFINITIONS_H
#define PROGRAM_DEFINITIONS_H

#include <Arduino.h>

// =============================================================================
// Program Data Structures
// =============================================================================

/**
 * Maximum limits for program structures
 */
constexpr uint8_t MAX_PHASES_PER_PROGRAM = 10;
constexpr uint8_t MAX_CYCLES_PER_PROGRAM = 5;
constexpr uint8_t MAX_PHASES_PER_CYCLE = 5;
constexpr uint8_t MAX_PROGRAMS = 10;

/**
 * Phase definition for drying programs
 */
struct Phase {
  uint8_t id;                      // Unique phase identifier
  char name[16];                   // Phase name for display
  float temperature_target;        // Target temperature (0 = no condition)
  bool transition_on_temperature;  // Transition when temp reached
  float humidity_max;              // Max humidity (0 = no condition, >0 = transition when below)
  bool transition_on_humidity;     // Transition when humidity drops below max
  uint32_t duration_s;             // Phase duration in seconds
};

/**
 * Cycle definition - a sequence of phases that can repeat
 */
struct Cycle {
  uint8_t id;                      // Unique cycle identifier
  uint8_t phase_ids[MAX_PHASES_PER_CYCLE];  // Array of phase IDs to execute
  uint8_t phase_count;             // Number of phases in this cycle
  int32_t repeat_duration_s;       // -1 = infinite, 0 = once, >0 = repeat for duration
};

/**
 * Program definition - a complete drying program
 */
struct Program {
  uint8_t id;                      // Unique program identifier
  char name[32];                   // Program name for display
  Phase phases[MAX_PHASES_PER_PROGRAM];  // Array of phase definitions
  uint8_t phase_count;             // Number of phases defined
  Cycle cycles[MAX_CYCLES_PER_PROGRAM];  // Array of cycles to execute sequentially
  uint8_t cycle_count;             // Number of cycles
};

/**
 * Find a phase by ID in a program
 * Returns nullptr if not found
 */
inline const Phase* FindPhaseById(const Program* program, uint8_t phase_id) {
  if (program == nullptr) return nullptr;
  for (uint8_t i = 0; i < program->phase_count; i++) {
    if (program->phases[i].id == phase_id) {
      return &program->phases[i];
    }
  }
  return nullptr;
}

#endif  // PROGRAM_DEFINITIONS_H
