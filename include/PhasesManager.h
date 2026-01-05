#ifndef PHASES_MANAGER_H
#define PHASES_MANAGER_H

#include <Arduino.h>
#include "HeatersManager.h"

/**
 * Dryer phases
 */
enum class DryerPhase : uint8_t {
  kStop = 0,
  kInit = 1,
  kExtraction = 2,
  kCirculation = 3
};

/**
 * Phase parameters
 */
struct PhaseParams {
  uint32_t init_phase_duration_s;        // 5-7200s
  uint32_t extraction_phase_duration_s;  // 5-7200s
  uint32_t circulation_phase_duration_s; // 5-3600s

  PhaseParams()
    : init_phase_duration_s(3600),      // 1 hour
      extraction_phase_duration_s(120),  // 2 minutes
      circulation_phase_duration_s(300)  // 5 minutes
  {}
};

/**
 * Manages dryer phases and transitions
 */
class PhasesManager {
 public:
  PhasesManager(HeatersManager* heaters_manager);

  void Begin();
  void Update();

  // Phase control
  void SetPhase(DryerPhase phase);
  DryerPhase GetPhase() const { return current_phase_; }
  const char* GetPhaseName() const;

  // Phase timing
  unsigned long GetPhaseElapsedTime() const;

  // Parameters
  PhaseParams& GetParams() { return params_; }
  const PhaseParams& GetParams() const { return params_; }

  // Temperature check for init phase
  void SetTemperatureInRange(bool in_range) { temperature_in_range_ = in_range; }

 private:
  HeatersManager* heaters_manager_;
  PhaseParams params_;

  DryerPhase current_phase_;
  unsigned long phase_start_time_;
  bool temperature_in_range_;

  // Saved optimized values for extraction/circulation phases
  float extraction_optimized_hydraulic_power_;
  float extraction_optimized_electric_power_;
  float circulation_optimized_hydraulic_power_;
  float circulation_optimized_electric_power_;

  // Phase entry handlers
  void EnterStopPhase();
  void EnterInitPhase();
  void EnterExtractionPhase();
  void EnterCirculationPhase();

  // Phase transitions
  void CheckPhaseTransition();
  void TransitionToNextPhase();

  // Save optimized values
  void SaveExtractionValues();
  void SaveCirculationValues();

  // Get current phase duration
  uint32_t GetCurrentPhaseDuration() const;
};

#endif  // PHASES_MANAGER_H
