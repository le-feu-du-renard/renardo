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

// Three-phase drying sequence:
// Init (x1) -> [Brassage -> Extraction] x inf
enum class DryerPhase : uint8_t
{
    kStop = 0,
    kInit = 1,
    kBrassage = 2,
    kExtraction = 3,
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

    // State restoration after reboot (restores running session)
    void RestoreState(DryerPhase phase, uint32_t phase_elapsed_s, uint32_t total_elapsed_s);

private:
    TemperatureManager *temperature_manager_;
    HumidityManager *humidity_manager_;

    PhaseDurations durations_;

    SessionState state_;
    DryerPhase current_phase_;

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
