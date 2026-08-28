#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "TemperatureManager.h"
#include "HumidityManager.h"

enum class SessionState : uint8_t
{
    kStopped = 0,
    kRunning = 1,
    kCooling = 2,  // heaters off, fan still running to cool electric heater
};

// Which question the session is answering.
//
//   kDrying  — Init (x1) -> [Brassage -> Extraction] x inf, the timed cycle.
//              The phases move the air on a clock and on humidity thresholds,
//              and the point is to take water out of a batch.
//   kClimate — one phase, no clock. Temperature and humidity are simply held
//              where they are asked to be, for as long as the session runs.
//
// A dryer and a climate chamber are the same machine asked two different
// questions, and the whole of the difference is in this file: everything under
// it — the interlocks, the safety cutoff, the air-renewal window, the source
// law — is common and never learns which programme is running.
enum class DryerProgram : uint8_t
{
    kDrying  = DRYER_PROGRAM_DRYING,
    kClimate = DRYER_PROGRAM_CLIMATE,
};

// Drying sequence, plus the climate programme's single phase:
// Init (x1) -> [Brassage -> Extraction] x inf, or Climat alone.
enum class DryerPhase : uint8_t
{
    kStop = 0,
    kInit = 1,
    kBrassage = 2,
    kExtraction = 3,
    kClimat = 4,
};

// Phase durations, in seconds. Defaults come from config.h and are overridden
// from the menu, then persisted by SettingsStore.
struct PhaseDurations
{
    uint32_t init;
    uint32_t brassage;
    uint32_t extraction;
    uint32_t extraction_damper_open;

    PhaseDurations()
        : init(INIT_PHASE_DURATION),
          brassage(BRASSAGE_PHASE_DURATION),
          extraction(EXTRACTION_PHASE_DURATION),
          extraction_damper_open(EXTRACTION_DAMPER_OPEN_DURATION) {}
};

// Manages the three-phase drying session.
class SessionManager
{
public:
    SessionManager(TemperatureManager *temperature_manager, HumidityManager *humidity_manager);

    void Begin();

    PhaseDurations       &GetDurations()       { return durations_; }
    const PhaseDurations &GetDurations() const { return durations_; }

    // Call every control loop iteration with fresh sensor readings.
    void Update(float current_temperature, float current_humidity);

    void Start();
    void Stop();
    bool IsRunning()   const { return state_ == SessionState::kRunning; }
    bool IsFanActive() const { return state_ == SessionState::kRunning ||
                                      state_ == SessionState::kCooling; }

    // Must be called every loop to detect end of post-stop fan cooldown.
    void UpdateCooldown();

    // State accessors
    DryerPhase GetCurrentPhase() const { return current_phase_; }
    const char *GetCurrentPhaseName() const;
    uint32_t GetPhaseElapsedTime() const; // seconds
    uint32_t GetTotalElapsedTime() const; // seconds

    // Target humidity from user potentiometer — used for humidity-based transitions
    void SetTargetHumidity(float humidity) { user_target_humidity_ = humidity; }

    // Which programme the next session runs. Changing it mid-session does
    // nothing to the session in progress: the phase machine is already inside a
    // programme, and switching the rails under a running train is not a
    // behaviour anyone asked for. The menu greys the entry out while running,
    // and this is the half of that guarantee the menu cannot make.
    void         SetProgram(DryerProgram program) { program_ = program; }
    DryerProgram GetProgram() const { return program_; }
    bool IsClimate() const { return program_ == DryerProgram::kClimate; }

    // State restoration after reboot (restores running session)
    void RestoreState(DryerPhase phase, uint32_t phase_elapsed_s, uint32_t total_elapsed_s);

private:
    TemperatureManager *temperature_manager_;
    HumidityManager *humidity_manager_;

    PhaseDurations durations_;

    SessionState state_;
    DryerPhase current_phase_;
    DryerProgram program_;

    float    user_target_humidity_;   // set from potentiometer each loop
    uint32_t init_extraction_end_ms_; // 0 = not extracting within init phase

    uint32_t phase_start_ms_;    // millis() at phase entry
    uint32_t session_start_ms_;  // millis() at session start (adjusted on restore)
    uint32_t cooldown_end_ms_;   // millis() target for end of post-stop fan cooldown

    void EnterPhase(DryerPhase phase);
    void CheckPhaseTransition(float current_temperature, float current_humidity);

    // Whether the Extraction phase means anything on this machine.
    //
    // It does not with a dehumidifier fitted. Extraction throws the chamber's
    // air away to take the moisture with it; a dehumidifier condenses that
    // moisture out and keeps the air, so a phase that periodically empties the
    // circuit is working against the machine rather than with it. Brassage then
    // simply repeats, and the register is driven by Dryer::UpdateForcedOpen()
    // on temperature and humidity instead of by the clock.
    bool ExtractionIsUsed() const;

    // Command the damper and, when it actually moves, tell the regulation the
    // air is about to be renewed. Every movement the session asks for goes
    // through here so none of them is missed.
    void SetDamperMode(HumidityManager::Mode mode);
};

#endif // SESSION_MANAGER_H
