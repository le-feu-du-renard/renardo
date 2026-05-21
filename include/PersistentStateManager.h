#ifndef PERSISTENT_STATE_MANAGER_H
#define PERSISTENT_STATE_MANAGER_H

#include <Arduino.h>
#include "SessionManager.h"

// Session state persisted to SD card (state.bin) for reboot recovery.
// PID parameters and phase durations are compile-time constants (config.h)
// and are not stored here.
struct PersistentState
{
  uint16_t   version;
  bool       session_running;
  DryerPhase phase;
  uint32_t   phase_elapsed_s;
  uint32_t   total_elapsed_s;
  uint16_t   checksum;

  PersistentState()
      : version(9),
        session_running(false),
        phase(DryerPhase::kStop),
        phase_elapsed_s(0),
        total_elapsed_s(0),
        checksum(0) {}
};

class PersistentStateManager
{
public:
  PersistentStateManager();
  void Begin();

  // Persist current session state to SD card.
  void Save(bool session_running, DryerPhase phase,
            uint32_t phase_elapsed_s, uint32_t total_elapsed_s);

  // Load session state from SD card.
  // Returns true if data is valid and a running session was saved.
  bool Load(DryerPhase &phase, uint32_t &phase_elapsed_s, uint32_t &total_elapsed_s);

  void Reset();

private:
  static constexpr uint16_t    kStateVersion  = 9;
  static constexpr const char *kStateFilePath = "/state.bin";

  PersistentState state_;

  uint16_t CalculateChecksum(const PersistentState &s) const;
  void     WriteToSD();
  bool     ReadFromSD();
};

#endif // PERSISTENT_STATE_MANAGER_H
