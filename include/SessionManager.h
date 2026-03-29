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

// Manages the three-phase drying session.
// Phase durations and thresholds come from config.h constants.
class SessionManager
{
public:
    SessionManager(TemperatureManager *temperature_manager, HumidityManager *humidity_manager);

    void Begin();

    // Call every control loop iteration with fresh sensor readings.
    void Update(float current_temperature, float current_humidity);

    void Start();
    void Stop();
    bool IsRunning() const { return state_ == SessionState::kRunning; }

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

    SessionState state_;
    DryerPhase current_phase_;

    float    user_target_humidity_;   // set from potentiometer each loop
    uint32_t init_extraction_end_ms_; // 0 = not extracting within init phase

    uint32_t phase_start_ms_;   // millis() at phase entry
    uint32_t session_start_ms_; // millis() at session start (adjusted on restore)

    void EnterPhase(DryerPhase phase);
    void CheckPhaseTransition(float current_temperature, float current_humidity);
};

#endif // SESSION_MANAGER_H
