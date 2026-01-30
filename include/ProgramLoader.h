#ifndef PROGRAM_LOADER_H
#define PROGRAM_LOADER_H

#include <Arduino.h>
#include "ProgramDefinitions.h"
#include "Programs.h"

/**
 * Loads drying programs
 *
 * Programs are defined in C++ files (src/programs/) and loaded at compile time.
 */
class ProgramLoader {
 public:
  ProgramLoader();

  /**
   * Initialize the program loader
   * @return true if initialization successful
   */
  bool Begin();

  /**
   * Load all built-in programs
   * @return Number of programs loaded
   */
  uint8_t LoadPrograms();

  /**
   * Get program by index
   * @param index Program index (0-based)
   * @return Pointer to program or nullptr if invalid
   */
  const Program* GetProgram(uint8_t index) const;

  /**
   * Get program by ID
   * @param id Program ID
   * @return Pointer to program or nullptr if not found
   */
  const Program* GetProgramById(uint8_t id) const;

  /**
   * Get the default program (first loaded)
   * @return Pointer to default program or nullptr if none loaded
   */
  const Program* GetDefaultProgram() const;

  /**
   * Get number of loaded programs
   */
  uint8_t GetProgramCount() const { return program_count_; }

  /**
   * Create a manual program with custom temperature and humidity
   * @param temperature_target Target temperature for the manual program
   * @param humidity_max Maximum humidity for the manual program
   * @return Pointer to the manual program
   */
  const Program* CreateManualProgram(float temperature_target, float humidity_max);

  /**
   * Get the manual program if it exists
   * @return Pointer to manual program or nullptr if not created
   */
  const Program* GetManualProgram() const;

  /**
   * Update manual program parameters (while running)
   * @param temperature_target New target temperature
   * @param humidity_max New maximum humidity
   */
  void UpdateManualProgram(float temperature_target, float humidity_max);

 private:
  const Program* program_pointers_[MAX_PROGRAMS];
  uint8_t program_count_;
  Program manual_program_;
  bool manual_program_created_;
};

#endif  // PROGRAM_LOADER_H
