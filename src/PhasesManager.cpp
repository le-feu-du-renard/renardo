#include "PhasesManager.h"

PhasesManager::PhasesManager(HeatersManager* heaters_manager)
  : heaters_manager_(heaters_manager),
    params_(),
    current_phase_(DryerPhase::kStop),
    phase_start_time_(0),
    temperature_in_range_(false),
    extraction_optimized_hydraulic_power_(100.0f),
    extraction_optimized_electric_power_(0.0f),
    circulation_optimized_hydraulic_power_(100.0f),
    circulation_optimized_electric_power_(0.0f) {
}

void PhasesManager::Begin() {
  current_phase_ = DryerPhase::kStop;
  phase_start_time_ = 0;
  temperature_in_range_ = false;

  Serial.println("Phases manager initialized");
}

void PhasesManager::Update() {
  CheckPhaseTransition();
}

void PhasesManager::SetPhase(DryerPhase phase) {
  if (phase == current_phase_) {
    return;
  }

  current_phase_ = phase;
  phase_start_time_ = millis();

  // Enter phase-specific logic
  switch (phase) {
    case DryerPhase::kStop:
      EnterStopPhase();
      break;
    case DryerPhase::kInit:
      EnterInitPhase();
      break;
    case DryerPhase::kExtraction:
      EnterExtractionPhase();
      break;
    case DryerPhase::kCirculation:
      EnterCirculationPhase();
      break;
  }

  Serial.print("Phase changed to: ");
  Serial.println(GetPhaseName());
}

const char* PhasesManager::GetPhaseName() const {
  switch (current_phase_) {
    case DryerPhase::kStop:
      return "Stop";
    case DryerPhase::kInit:
      return "Init";
    case DryerPhase::kExtraction:
      return "Extraction";
    case DryerPhase::kCirculation:
      return "Circulation";
    default:
      return "Unknown";
  }
}

unsigned long PhasesManager::GetPhaseElapsedTime() const {
  if (phase_start_time_ == 0) {
    return 0;
  }
  return (millis() - phase_start_time_) / 1000;  // Return in seconds
}

void PhasesManager::RestorePhaseState(DryerPhase phase, uint32_t elapsed_time_s) {
  // Restore phase without calling SetPhase to avoid resetting phase_start_time_
  current_phase_ = phase;

  // Call the appropriate Enter phase to restore heater states
  switch (phase) {
    case DryerPhase::kStop:
      EnterStopPhase();
      break;
    case DryerPhase::kInit:
      EnterInitPhase();
      break;
    case DryerPhase::kExtraction:
      EnterExtractionPhase();
      break;
    case DryerPhase::kCirculation:
      EnterCirculationPhase();
      break;
  }

  // Now restore the phase start time
  RestorePhaseStartTime(elapsed_time_s);

  Serial.print("Phase state restored to: ");
  Serial.println(GetPhaseName());
}

void PhasesManager::RestorePhaseStartTime(uint32_t elapsed_time_s) {
  // After a reboot, millis() starts at 0, so we need to set phase_start_time_
  // as if the phase started elapsed_time_s seconds ago from now.
  // This works because we only care about the *difference* between now and phase_start_time_.

  unsigned long elapsed_ms = elapsed_time_s * 1000UL;
  unsigned long now = millis();

  // Since millis() just rebooted, now will be small (few seconds at most)
  // We want: (now - phase_start_time_) / 1000 = elapsed_time_s
  // So: phase_start_time_ = now - (elapsed_time_s * 1000)
  //
  // Since now is small and elapsed_time_s could be large, this will underflow,
  // which is exactly what we want for unsigned arithmetic!
  phase_start_time_ = now - elapsed_ms;

  Serial.print("Restored phase start time with elapsed: ");
  Serial.print(elapsed_time_s);
  Serial.print("s (now=");
  Serial.print(now);
  Serial.print("ms, phase_start=");
  Serial.print(phase_start_time_);
  Serial.println("ms)");
}

void PhasesManager::EnterStopPhase() {
  // Turn off all heaters
  heaters_manager_->GetElectricHeater()->SetPower(0.0f);
  heaters_manager_->GetHydraulicHeater()->SetPower(0.0f);
  heaters_manager_->ResetCooldown();

  Serial.println("Phase: Stop activated");
}

void PhasesManager::EnterInitPhase() {
  // Start with hydraulic at max, electric off
  heaters_manager_->GetHydraulicHeater()->SetPower(100.0f);
  heaters_manager_->GetElectricHeater()->SetPower(0.0f);
  heaters_manager_->ResetCooldown();

  Serial.println("Phase: Init activated");
}

void PhasesManager::EnterExtractionPhase() {
  // Restore optimized values from previous extraction phase
  heaters_manager_->GetHydraulicHeater()->SetPower(extraction_optimized_hydraulic_power_);
  heaters_manager_->GetElectricHeater()->SetPower(extraction_optimized_electric_power_);
  heaters_manager_->ResetCooldown();

  Serial.println("Phase: Extraction activated");
}

void PhasesManager::EnterCirculationPhase() {
  // Restore optimized values from previous circulation phase
  heaters_manager_->GetHydraulicHeater()->SetPower(circulation_optimized_hydraulic_power_);
  heaters_manager_->GetElectricHeater()->SetPower(circulation_optimized_electric_power_);
  heaters_manager_->ResetCooldown();

  Serial.println("Phase: Circulation activated");
}

void PhasesManager::CheckPhaseTransition() {
  // Only check transitions when not stopped
  if (current_phase_ == DryerPhase::kStop) {
    return;
  }

  bool should_transition = false;

  // Check phase duration
  unsigned long elapsed_s = GetPhaseElapsedTime();
  uint32_t phase_duration = GetCurrentPhaseDuration();

  if (elapsed_s >= phase_duration) {
    Serial.print("Phase duration reached: ");
    Serial.print(elapsed_s);
    Serial.print("s >= ");
    Serial.print(phase_duration);
    Serial.println("s");
    should_transition = true;
  }

  // Init phase: also check if temperature target reached
  if (current_phase_ == DryerPhase::kInit && temperature_in_range_) {
    Serial.println("Temperature target reached in init phase");
    should_transition = true;
  }

  if (should_transition) {
    TransitionToNextPhase();
  }
}

void PhasesManager::TransitionToNextPhase() {
  switch (current_phase_) {
    case DryerPhase::kInit:
      SetPhase(DryerPhase::kExtraction);
      break;

    case DryerPhase::kExtraction:
      SaveExtractionValues();
      SetPhase(DryerPhase::kCirculation);
      break;

    case DryerPhase::kCirculation:
      SaveCirculationValues();
      SetPhase(DryerPhase::kExtraction);
      break;

    default:
      break;
  }
}

void PhasesManager::SaveExtractionValues() {
  extraction_optimized_hydraulic_power_ = heaters_manager_->GetHydraulicHeater()->GetPower();
  extraction_optimized_electric_power_ = heaters_manager_->GetElectricHeater()->GetPower();

  Serial.print("Saved extraction values - Hydraulic: ");
  Serial.print(extraction_optimized_hydraulic_power_);
  Serial.print("%, Electric: ");
  Serial.println(extraction_optimized_electric_power_);
}

void PhasesManager::SaveCirculationValues() {
  circulation_optimized_hydraulic_power_ = heaters_manager_->GetHydraulicHeater()->GetPower();
  circulation_optimized_electric_power_ = heaters_manager_->GetElectricHeater()->GetPower();

  Serial.print("Saved circulation values - Hydraulic: ");
  Serial.print(circulation_optimized_hydraulic_power_);
  Serial.print("%, Electric: ");
  Serial.println(circulation_optimized_electric_power_);
}

uint32_t PhasesManager::GetCurrentPhaseDuration() const {
  switch (current_phase_) {
    case DryerPhase::kInit:
      return params_.init_phase_duration_s;
    case DryerPhase::kExtraction:
      return params_.extraction_phase_duration_s;
    case DryerPhase::kCirculation:
      return params_.circulation_phase_duration_s;
    default:
      return 0;
  }
}
