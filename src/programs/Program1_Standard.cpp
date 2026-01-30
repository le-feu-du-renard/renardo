#include "ProgramDefinitions.h"

// Program 1: Standard
// - Temperature: 40°C
// - Humidity max: 70%
// - Init phase: heat until temperature reached, then transition to drying cycle
// - Drying cycle: Extraction (2 min) + Circulation (5 min) in loop

static Program program_standard = {
  .id = 1,
  .name = "Standard",
  .phases = {
    // Phase 1: Init
    {
      .id = 1,
      .name = "Init",
      .temperature_target = 40.0f,
      .transition_on_temperature = true,
      .humidity_max = 70.0f,
      .transition_on_humidity = false,
      .duration_s = 3600
    },
    // Phase 2: Extraction
    {
      .id = 2,
      .name = "Extraction",
      .temperature_target = 40.0f,
      .transition_on_temperature = false,
      .humidity_max = -1.0f,
      .transition_on_humidity = false,
      .duration_s = 120
    },
    // Phase 3: Circulation
    {
      .id = 3,
      .name = "Circulation",
      .temperature_target = 40.0f,
      .transition_on_temperature = false,
      .humidity_max = 70.0f,
      .transition_on_humidity = false,
      .duration_s = 300
    }
  },
  .phase_count = 3,
  .cycles = {
    // Cycle 1: Init (run once)
    {
      .id = 1,
      .phase_ids = {1},
      .phase_count = 1,
      .repeat_duration_s = 0
    },
    // Cycle 2: Drying (Extraction + Circulation, repeat infinitely)
    {
      .id = 2,
      .phase_ids = {2, 3},
      .phase_count = 2,
      .repeat_duration_s = -1
    }
  },
  .cycle_count = 2
};

const Program* GetProgram1_Standard() {
  return &program_standard;
}
