#include "ProgramDefinitions.h"

// Program 2: Doux (Gentle)
// - Init temperature: 28°C
// - Drying temperature: 35°C
// - Humidity max: 65%
// - Gentler drying process for delicate products

static Program program_doux = {
    .id = 2,
    .name = "Doux",
    .phases = {
        // Phase 1: Init (28°C)
        {
            .id = 1,
            .name = "Init",
            .temperature_target = 28.0f,
            .transition_on_temperature = true,
            .humidity_max = 65.0f,
            .transition_on_humidity = false,
            .duration_s = 3600},
        // Phase 2: Extraction (35°C)
        {
            .id = 2,
            .name = "Extraction",
            .temperature_target = 35.0f,
            .transition_on_temperature = false,
            .humidity_max = -1.0f,
            .transition_on_humidity = false,
            .duration_s = 120},
        // Phase 3: Circulation (35°C, 65% HR)
        {
            .id = 3,
            .name = "Circulation",
            .temperature_target = 35.0f,
            .transition_on_temperature = false,
            .humidity_max = 65.0f,
            .transition_on_humidity = false,
            .duration_s = 300}},
    .phase_count = 3,
    .cycles = {// Cycle 1: Init (run once)
               {.id = 1, .phase_ids = {1}, .phase_count = 1, .repeat_duration_s = 0},
               // Cycle 2: Drying (Extraction + Circulation, repeat infinitely)
               {.id = 2, .phase_ids = {2, 3}, .phase_count = 2, .repeat_duration_s = -1}},
    .cycle_count = 2};

const Program *GetProgram2_Doux()
{
  return &program_doux;
}
