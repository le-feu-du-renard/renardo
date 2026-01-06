#include "Dryer.h"
#include "config.h"

Dryer::Dryer(DFRobot_GP8403 *dac)
    : electric_heater_(),
      hydraulic_heater_(),
      heaters_manager_(&electric_heater_, &hydraulic_heater_),
      phases_manager_(&heaters_manager_),
      settings_manager_(),
      air_recycling_manager_(dac),
      running_(false),
      start_time_(0),
      total_duty_time_s_(0),
      last_duty_time_save_(0),
      last_saved_phase_(DryerPhase::kStop),
      settings_changed_callback_(nullptr),
      inlet_temperature_(0.0f),
      outlet_temperature_(0.0f),
      water_temperature_(0.0f),
      inlet_humidity_(0.0f),
      outlet_humidity_(0.0f),
      fan_output_(0.0f),
      last_heating_update_(0)
{
}

void Dryer::Begin()
{
  // Initialize managers
  heaters_manager_.Begin();
  phases_manager_.Begin();
  settings_manager_.Begin();
  air_recycling_manager_.Begin();

  // Load settings from EEPROM
  LoadSettings();

  Serial.println("Dryer initialized");
}

void Dryer::Start()
{
  if (!running_)
  {
    Serial.println("Starting dryer...");
    running_ = true;
    start_time_ = millis();
    last_duty_time_save_ = millis();
    phases_manager_.SetPhase(DryerPhase::kInit);
    last_saved_phase_ = DryerPhase::kInit;

    // Save state to EEPROM immediately
    settings_manager_.SaveDryerState(true);

    // Trigger a full settings save
    NotifySettingsChanged();
  }
}

void Dryer::Stop()
{
  if (running_)
  {
    Serial.println("Stopping dryer...");
    running_ = false;
    phases_manager_.SetPhase(DryerPhase::kStop);

    // Save final state and duty time to EEPROM
    SaveSettings();
  }
}

void Dryer::Update()
{
  if (!running_ && phases_manager_.GetPhase() != DryerPhase::kStop)
  {
    phases_manager_.SetPhase(DryerPhase::kStop);
  }

  if (running_)
  {
    // Serial.println("Updating dryer...");

    // Update phases
    phases_manager_.Update();

    // Update heating control (with interval)
    UpdateHeatingControl();

    // Update ventilation control
    UpdateVentilationControl();

    // Update air recycling control
    air_recycling_manager_.Update();

    // Update duty time tracking
    UpdateDutyTime();

    // Serial.println("Dryer updated!");
  }
}

void Dryer::UpdateHeatingControl()
{
  unsigned long now = millis();

  if (now - last_heating_update_ >= kHeatingUpdateInterval)
  {
    last_heating_update_ = now;

    // Update temperature range status for phase manager
    bool temperature_in_range = heaters_manager_.GetParams().temperature_deadband > 0.0f &&
                                fabs(inlet_temperature_ - heaters_manager_.GetTargetTemperature()) <=
                                    heaters_manager_.GetParams().temperature_deadband;
    phases_manager_.SetTemperatureInRange(temperature_in_range);

    // Update heaters manager with current temperature
    heaters_manager_.Update(inlet_temperature_);
  }
}

void Dryer::UpdateVentilationControl()
{
  // Fan control based on phase
  DryerPhase phase = phases_manager_.GetPhase();

  switch (phase)
  {
  case DryerPhase::kStop:
    fan_output_ = 0.0f;
    break;

  case DryerPhase::kInit:
  case DryerPhase::kExtraction:
  case DryerPhase::kCirculation:
    fan_output_ = 1.0f; // Full speed when running
    break;
  }
}

const char *Dryer::GetPhaseName() const
{
  return phases_manager_.GetPhaseName();
}

unsigned long Dryer::GetElapsedTime() const
{
  unsigned long current_session_time = 0;
  if (running_)
  {
    current_session_time = (millis() - start_time_) / 1000;
  }
  return total_duty_time_s_ + current_session_time;
}

void Dryer::SetTargetTemperature(float temperature)
{
  heaters_manager_.SetTargetTemperature(temperature);
  Serial.print("Target temperature set to: ");
  Serial.println(temperature);
}

float Dryer::GetTargetTemperature() const
{
  return heaters_manager_.GetTargetTemperature();
}

float Dryer::GetHeaterOutput() const
{
  return electric_heater_.GetOutput();
}

float Dryer::GetCirculatorOutput() const
{
  return hydraulic_heater_.GetOutput();
}

void Dryer::UpdateDutyTime()
{
  // Update duty time on every call for accurate UI display
  unsigned long now = millis();
  unsigned long elapsed_since_last_update = (now - last_duty_time_save_) / 1000;

  if (elapsed_since_last_update > 0)
  {
    total_duty_time_s_ += elapsed_since_last_update;
    last_duty_time_save_ = now;
  }
}

void Dryer::SaveSettings()
{
  settings_manager_.SaveSettings(
      running_,
      phases_manager_.GetPhase(),
      phases_manager_.GetPhaseElapsedTime(),
      heaters_manager_.GetParams(),
      phases_manager_.GetParams(),
      total_duty_time_s_,
      air_recycling_manager_.GetRecyclingRate());
}

void Dryer::LoadSettings()
{
  bool saved_running_state = false;
  DryerPhase saved_phase = DryerPhase::kStop;
  uint32_t saved_phase_elapsed_time = 0;
  HeatingParams heating_params;
  PhaseParams phase_params;
  uint32_t saved_duty_time = 0;
  float saved_recycling_rate = 50.0f;

  bool success = settings_manager_.LoadSettings(
      saved_running_state,
      saved_phase,
      saved_phase_elapsed_time,
      heating_params,
      phase_params,
      saved_duty_time,
      saved_recycling_rate);

  if (success)
  {
    // Restore parameters
    heaters_manager_.GetParams() = heating_params;
    phases_manager_.GetParams() = phase_params;
    total_duty_time_s_ = saved_duty_time;
    air_recycling_manager_.SetRecyclingRate(saved_recycling_rate);

    // Restore running state if dryer was running before reboot
    if (saved_running_state)
    {
      Serial.println("Dryer was running before reboot, resuming...");
      running_ = true;
      start_time_ = millis();
      last_duty_time_save_ = millis();
      phases_manager_.RestorePhaseState(saved_phase, saved_phase_elapsed_time);
      last_saved_phase_ = saved_phase;
    }

    Serial.println("Settings restored from EEPROM");
  }
  else
  {
    Serial.println("Using default settings");
  }
}

void Dryer::SetRecyclingRate(float rate)
{
  air_recycling_manager_.SetRecyclingRate(rate);
  NotifySettingsChanged();
}

float Dryer::GetRecyclingRate() const
{
  return air_recycling_manager_.GetRecyclingRate();
}

void Dryer::NotifySettingsChanged()
{
  if (settings_changed_callback_ != nullptr)
  {
    settings_changed_callback_();
  }
}

// ========== Menu Parameter Access ==========

// Heating parameters
float Dryer::GetTemperatureTarget() const
{
  return heaters_manager_.GetParams().temperature_target;
}

void Dryer::SetTemperatureTarget(float value)
{
  heaters_manager_.GetParams().temperature_target = value;
  NotifySettingsChanged();
}

float Dryer::GetTemperatureDeadband() const
{
  return heaters_manager_.GetParams().temperature_deadband;
}

void Dryer::SetTemperatureDeadband(float value)
{
  heaters_manager_.GetParams().temperature_deadband = value;
  NotifySettingsChanged();
}

float Dryer::GetHeatingActionMinWait() const
{
  return heaters_manager_.GetParams().heating_action_min_wait_s;
}

void Dryer::SetHeatingActionMinWait(float value)
{
  heaters_manager_.GetParams().heating_action_min_wait_s = value;
  NotifySettingsChanged();
}

float Dryer::GetHeaterStepMin() const
{
  return heaters_manager_.GetParams().heater_step_min;
}

void Dryer::SetHeaterStepMin(float value)
{
  heaters_manager_.GetParams().heater_step_min = value;
  NotifySettingsChanged();
}

float Dryer::GetHeaterStepMax() const
{
  return heaters_manager_.GetParams().heater_step_max;
}

void Dryer::SetHeaterStepMax(float value)
{
  heaters_manager_.GetParams().heater_step_max = value;
  NotifySettingsChanged();
}

float Dryer::GetHeaterFullScaleDelta() const
{
  return heaters_manager_.GetParams().heater_full_scale_delta;
}

void Dryer::SetHeaterFullScaleDelta(float value)
{
  heaters_manager_.GetParams().heater_full_scale_delta = value;
  NotifySettingsChanged();
}

// Phase parameters
float Dryer::GetInitPhaseDuration() const
{
  return static_cast<float>(phases_manager_.GetParams().init_phase_duration_s);
}

void Dryer::SetInitPhaseDuration(float value)
{
  phases_manager_.GetParams().init_phase_duration_s = static_cast<uint32_t>(value);
  NotifySettingsChanged();
}

float Dryer::GetExtractionPhaseDuration() const
{
  return static_cast<float>(phases_manager_.GetParams().extraction_phase_duration_s);
}

void Dryer::SetExtractionPhaseDuration(float value)
{
  phases_manager_.GetParams().extraction_phase_duration_s = static_cast<uint32_t>(value);
  NotifySettingsChanged();
}

float Dryer::GetCirculationPhaseDuration() const
{
  return static_cast<float>(phases_manager_.GetParams().circulation_phase_duration_s);
}

void Dryer::SetCirculationPhaseDuration(float value)
{
  phases_manager_.GetParams().circulation_phase_duration_s = static_cast<uint32_t>(value);
  NotifySettingsChanged();
}

float Dryer::GetDryingSessionDuration() const
{
  return static_cast<float>(phases_manager_.GetParams().drying_session_duration_s);
}

void Dryer::SetDryingSessionDuration(float value)
{
  phases_manager_.GetParams().drying_session_duration_s = static_cast<uint32_t>(value);
  NotifySettingsChanged();
}
