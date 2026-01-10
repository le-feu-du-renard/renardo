#ifndef PROGRAM_LOADER_H
#define PROGRAM_LOADER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ProgramDefinitions.h"

/**
 * Loads drying programs from JSON files
 *
 * Programs are loaded from:
 * - LittleFS: /programs/*.json (built-in programs)
 * - SD card: /programs/*.json (user programs, future)
 */
class ProgramLoader {
 public:
  ProgramLoader();

  /**
   * Initialize the program loader and filesystem
   * @return true if initialization successful
   */
  bool Begin();

  /**
   * Load all programs from filesystem
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
   * Check if filesystem is available
   */
  bool IsFilesystemAvailable() const { return fs_available_; }

 private:
  Program programs_[MAX_PROGRAMS];
  uint8_t program_count_;
  bool fs_available_;

  /**
   * Load a single program from JSON file
   * @param path File path
   * @param program Output program structure
   * @return true if successful
   */
  bool LoadProgramFromFile(const char* path, Program& program);

  /**
   * Parse phase from JSON object
   */
  bool ParsePhase(JsonObject& json, Phase& phase);

  /**
   * Parse cycle from JSON object
   */
  bool ParseCycle(JsonObject& json, Cycle& cycle);
};

#endif  // PROGRAM_LOADER_H
