#include "ProgramDefinitions.h"

// Program 3: Germination
// - Temperature: 25°C
// - Humidity max: 70%
// - Single phase that runs continuously for seed germination
// - Duration: 24 hours per cycle, repeats infinitely

static Program program_germination = {
    .id = 3,
    .name = "Germination",
    .phases = {
        // Phase 1: Germination (continuous)
        {
            .id = 1,
            .name = "Circulation",
            .temperature_target = 25.0f,
            .transition_on_temperature = false,
            .humidity_max = 70.0f,
            .transition_on_humidity = false,
            .duration_s = 86400 // 24 hours
        }},
    .phase_count = 1,
    .cycles = {// Cycle 1: Germination (repeat infinitely)
               {.id = 1, .phase_ids = {1}, .phase_count = 1, .repeat_duration_s = -1}},
    .cycle_count = 1};

const Program *GetProgram3_Germination()
{
  return &program_germination;
}
