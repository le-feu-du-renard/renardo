#ifndef PROGRAMS_H
#define PROGRAMS_H

#include "ProgramDefinitions.h"

/**
 * Built-in program definitions
 * Each program is defined in its own .cpp file in src/programs/
 */

// Program 1: Standard drying (40°C, 70% HR)
const Program* GetProgram1_Standard();

// Program 2: Gentle drying (28-35°C, 65% HR)
const Program* GetProgram2_Doux();

// Program 3: Germination mode (25°C, 70% HR, continuous)
const Program* GetProgram3_Germination();

#endif  // PROGRAMS_H
