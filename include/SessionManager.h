#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "ProgramDefinitions.h"
#include "TemperatureManager.h"
#include "HumidityManager.h"

/**
 * Session state enumeration
 */
enum class SessionState : uint8_t {
  kStopped = 0,
  kRunning = 1
};

/**
 * Manages drying session execution
 *
 * Responsible for:
 * - Loading and executing programs (sequences of cycles and phases)
 * - Tracking progress through cycles and phases
 * - Coordinating with TemperatureManager and HumidityManager
 * - Handling phase transitions based on conditions
 */
class SessionManager {
 public:
  /**
   * Constructor
   * @param temperature_manager Pointer to TemperatureManager instance
   * @param humidity_manager Pointer to HumidityManager instance
   */
  SessionManager(TemperatureManager* temperature_manager, HumidityManager* humidity_manager);

  /**
   * Initialize the session manager
   */
  void Begin();

  /**
   * Update session state (call in main loop)
   * @param current_temperature Current temperature reading
   * @param current_humidity Current humidity reading
   */
  void Update(float current_temperature, float current_humidity);

  // Program management
  void SetProgram(const Program* program);
  const Program* GetProgram() const { return current_program_; }

  // Session control
  void Start();
  void Stop();
  bool IsRunning() const { return state_ == SessionState::kRunning; }

  // Current state accessors
  uint8_t GetCurrentCycleIndex() const { return current_cycle_index_; }
  uint8_t GetCurrentPhaseIndexInCycle() const { return current_phase_index_in_cycle_; }
  uint8_t GetCurrentPhaseId() const;
  const char* GetCurrentPhaseName() const;

  // Timing
  unsigned long GetPhaseElapsedTime() const;
  unsigned long GetCycleElapsedTime() const;
  uint32_t GetCurrentPhaseDuration() const;

  // State restoration (for reboot recovery)
  void RestoreState(uint8_t cycle_index, uint8_t phase_index_in_cycle,
                    uint32_t phase_elapsed_s, uint32_t cycle_elapsed_s);

 private:
  TemperatureManager* temperature_manager_;
  HumidityManager* humidity_manager_;

  const Program* current_program_;
  SessionState state_;

  // Current position in program
  uint8_t current_cycle_index_;
  uint8_t current_phase_index_in_cycle_;

  // Timing
  unsigned long phase_start_time_;
  unsigned long cycle_start_time_;

  // Cached current phase (direct pointer, no PROGMEM)
  const Phase* current_phase_;
  const Cycle* current_cycle_;

  // Internal methods
  void EnterPhase(uint8_t phase_id);
  void ExitPhase();
  void CheckPhaseTransition(float current_temperature, float current_humidity);
  void TransitionToNextPhase();
  void TransitionToNextCycle();

  // Get phase by ID from current program
  const Phase* FindPhaseInProgram(uint8_t phase_id) const;
};

// Backward compatibility - keep old enum values accessible
enum class DryerPhase : uint8_t {
  kStop = 0,
  kInit = 1,
  kExtraction = 2,
  kCirculation = 3
};

#endif  // SESSION_MANAGER_H
