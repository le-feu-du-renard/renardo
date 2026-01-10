#include "SessionManager.h"

SessionManager::SessionManager(TemperatureManager *temperature_manager, HumidityManager *humidity_manager)
    : temperature_manager_(temperature_manager),
      humidity_manager_(humidity_manager),
      current_program_(nullptr),
      state_(SessionState::kStopped),
      current_cycle_index_(0),
      current_phase_index_in_cycle_(0),
      phase_start_time_(0),
      cycle_start_time_(0),
      current_phase_(nullptr),
      current_cycle_(nullptr)
{
}

void SessionManager::Begin()
{
  state_ = SessionState::kStopped;
  current_cycle_index_ = 0;
  current_phase_index_in_cycle_ = 0;
  phase_start_time_ = 0;
  cycle_start_time_ = 0;
  current_phase_ = nullptr;
  current_cycle_ = nullptr;

  Serial.println("Session manager initialized");
}

void SessionManager::Update(float current_temperature, float current_humidity)
{
  if (state_ != SessionState::kRunning)
  {
    return;
  }

  if (current_program_ == nullptr)
  {
    Serial.println("Error: No program loaded");
    Stop();
    return;
  }

  // Check for phase transitions
  CheckPhaseTransition(current_temperature, current_humidity);
}

void SessionManager::SetProgram(const Program *program)
{
  current_program_ = program;
  current_phase_ = nullptr;
  current_cycle_ = nullptr;

  if (program != nullptr)
  {
    Serial.print("Program set: '");
    Serial.print(program->name);
    Serial.print("' (ID=");
    Serial.print(program->id);
    Serial.print(", Phases=");
    Serial.print(program->phase_count);
    Serial.print(", Cycles=");
    Serial.print(program->cycle_count);
    Serial.println(")");
  }
}

void SessionManager::Start()
{
  if (current_program_ == nullptr)
  {
    Serial.println("Error: Cannot start - no program loaded");
    return;
  }

  if (current_program_->cycle_count == 0)
  {
    Serial.println("Error: Program has no cycles");
    return;
  }

  state_ = SessionState::kRunning;
  current_cycle_index_ = 0;
  current_phase_index_in_cycle_ = 0;
  cycle_start_time_ = millis();

  // Set current cycle
  current_cycle_ = &current_program_->cycles[0];

  // Enter first phase of first cycle
  if (current_cycle_->phase_count > 0)
  {
    uint8_t first_phase_id = current_cycle_->phase_ids[0];
    EnterPhase(first_phase_id);
  }

  Serial.println("Session started");
}

void SessionManager::Stop()
{
  state_ = SessionState::kStopped;

  // Turn off heaters
  temperature_manager_->GetElectricHeater()->SetPower(0.0f);
  temperature_manager_->GetHydraulicHeater()->SetPower(0.0f);
  temperature_manager_->ResetCooldown();

  // Reset humidity control
  humidity_manager_->SetTargetHumidity(0.0f);
  humidity_manager_->ResetCooldown();

  current_phase_ = nullptr;
  current_cycle_ = nullptr;

  Serial.println("Session stopped");
}

uint8_t SessionManager::GetCurrentPhaseId() const
{
  if (current_phase_ == nullptr || state_ != SessionState::kRunning)
  {
    return 0;
  }
  return current_phase_->id;
}

const char *SessionManager::GetCurrentPhaseName() const
{
  if (state_ != SessionState::kRunning)
  {
    return "Stop";
  }

  if (current_phase_ == nullptr)
  {
    return "Unknown";
  }

  return current_phase_->name;
}

unsigned long SessionManager::GetPhaseElapsedTime() const
{
  if (phase_start_time_ == 0)
  {
    return 0;
  }
  return (millis() - phase_start_time_) / 1000;
}

unsigned long SessionManager::GetCycleElapsedTime() const
{
  if (cycle_start_time_ == 0)
  {
    return 0;
  }
  return (millis() - cycle_start_time_) / 1000;
}

uint32_t SessionManager::GetCurrentPhaseDuration() const
{
  if (current_phase_ == nullptr)
  {
    return 0;
  }
  return current_phase_->duration_s;
}

void SessionManager::RestoreState(uint8_t cycle_index, uint8_t phase_index_in_cycle,
                                  uint32_t phase_elapsed_s, uint32_t cycle_elapsed_s)
{
  if (current_program_ == nullptr)
  {
    Serial.println("Error: Cannot restore state - no program loaded");
    return;
  }

  // Validate indices
  if (cycle_index >= current_program_->cycle_count)
  {
    Serial.println("Error: Invalid cycle index for restore");
    return;
  }

  const Cycle *cycle = &current_program_->cycles[cycle_index];
  if (phase_index_in_cycle >= cycle->phase_count)
  {
    Serial.println("Error: Invalid phase index for restore");
    return;
  }

  // Restore state
  state_ = SessionState::kRunning;
  current_cycle_index_ = cycle_index;
  current_phase_index_in_cycle_ = phase_index_in_cycle;
  current_cycle_ = cycle;

  // Restore timing (simulate elapsed time)
  unsigned long now = millis();
  phase_start_time_ = now - (phase_elapsed_s * 1000UL);
  cycle_start_time_ = now - (cycle_elapsed_s * 1000UL);

  // Enter the phase (configure managers)
  uint8_t phase_id = cycle->phase_ids[phase_index_in_cycle];
  EnterPhase(phase_id);

  Serial.print("Session state restored - Cycle: ");
  Serial.print(cycle_index);
  Serial.print(", Phase: ");
  Serial.print(phase_index_in_cycle);
  Serial.print(", Phase elapsed: ");
  Serial.print(phase_elapsed_s);
  Serial.println("s");
}

void SessionManager::EnterPhase(uint8_t phase_id)
{
  const Phase *phase = FindPhaseInProgram(phase_id);
  if (phase == nullptr)
  {
    Serial.print("Error: Phase not found: ");
    Serial.println(phase_id);
    return;
  }

  current_phase_ = phase;
  phase_start_time_ = millis();

  Serial.print("Entering phase ");
  Serial.print(current_phase_->id);
  Serial.print(" - Temp target: ");
  Serial.print(current_phase_->temperature_target);
  Serial.print("C, Humidity max: ");
  Serial.print(current_phase_->humidity_max);
  Serial.print(", Duration: ");
  Serial.print(current_phase_->duration_s);
  Serial.println("s");

  // Configure TemperatureManager
  if (current_phase_->temperature_target > 0.0f)
  {
    temperature_manager_->SetTargetTemperature(current_phase_->temperature_target);
  }
  temperature_manager_->ResetCooldown();

  // Configure HumidityManager - no target needed, we just check against humidity_max
  humidity_manager_->ResetCooldown();

  // Start with hydraulic at max for heating phases
  if (current_phase_->temperature_target > 0.0f)
  {
    temperature_manager_->GetHydraulicHeater()->SetPower(100.0f);
    temperature_manager_->GetElectricHeater()->SetPower(0.0f);
  }
}

void SessionManager::ExitPhase()
{
  if (current_phase_ != nullptr)
  {
    Serial.print("Exiting phase ");
    Serial.println(current_phase_->id);
  }
}

void SessionManager::CheckPhaseTransition(float current_temperature, float current_humidity)
{
  if (current_phase_ == nullptr)
  {
    return;
  }

  bool should_transition = false;
  const char *reason = "";

  // Check temperature condition
  if (current_phase_->transition_on_temperature &&
      current_phase_->temperature_target > 0.0f)
  {
    if (current_temperature >= current_phase_->temperature_target)
    {
      should_transition = true;
      reason = "temperature target reached";
    }
  }

  // Check humidity condition - transition when humidity drops below max
  if (!should_transition &&
      current_phase_->transition_on_humidity &&
      current_phase_->humidity_max > 0.0f)
  {
    if (current_humidity >= current_phase_->humidity_max)
    {
      should_transition = true;
      reason = "humidity max reached";
    }
  }

  // Check duration (always checked)
  unsigned long elapsed = GetPhaseElapsedTime();
  if (!should_transition && elapsed >= current_phase_->duration_s)
  {
    should_transition = true;
    reason = "duration reached";
  }

  if (should_transition)
  {
    Serial.print("Phase transition: ");
    Serial.println(reason);
    TransitionToNextPhase();
  }
}

void SessionManager::TransitionToNextPhase()
{
  ExitPhase();

  if (current_cycle_ == nullptr)
  {
    Stop();
    return;
  }

  // Move to next phase in cycle
  current_phase_index_in_cycle_++;

  if (current_phase_index_in_cycle_ >= current_cycle_->phase_count)
  {
    // End of cycle - check if we should repeat or move to next cycle
    unsigned long cycle_elapsed = GetCycleElapsedTime();

    bool should_repeat = false;

    if (current_cycle_->repeat_duration_s < 0)
    {
      // Infinite repeat
      should_repeat = true;
    }
    else if (current_cycle_->repeat_duration_s > 0)
    {
      // Repeat for duration
      should_repeat = (cycle_elapsed < (unsigned long)current_cycle_->repeat_duration_s);
    }
    // repeat_duration_s == 0 means no repeat

    if (should_repeat)
    {
      // Restart cycle from first phase
      current_phase_index_in_cycle_ = 0;
      Serial.print("Repeating cycle ");
      Serial.println(current_cycle_index_);
    }
    else
    {
      // Move to next cycle
      TransitionToNextCycle();
      return;
    }
  }

  // Enter next phase
  uint8_t next_phase_id = current_cycle_->phase_ids[current_phase_index_in_cycle_];
  EnterPhase(next_phase_id);
}

void SessionManager::TransitionToNextCycle()
{
  current_cycle_index_++;

  if (current_cycle_index_ >= current_program_->cycle_count)
  {
    // End of program
    Serial.println("Program completed");
    Stop();
    return;
  }

  // Start next cycle
  current_phase_index_in_cycle_ = 0;
  cycle_start_time_ = millis();
  current_cycle_ = &current_program_->cycles[current_cycle_index_];

  Serial.print("Starting cycle ");
  Serial.println(current_cycle_index_);

  if (current_cycle_->phase_count > 0)
  {
    uint8_t first_phase_id = current_cycle_->phase_ids[0];
    EnterPhase(first_phase_id);
  }
}

const Phase *SessionManager::FindPhaseInProgram(uint8_t phase_id) const
{
  if (current_program_ == nullptr)
  {
    return nullptr;
  }
  return FindPhaseById(current_program_, phase_id);
}
