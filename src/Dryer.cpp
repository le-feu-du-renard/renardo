#include "Dryer.h"
#include "config.h"

Dryer::Dryer(DFRobot_GP8403 *dac)
    : electric_heater_(),
      hydraulic_heater_(),
      temperature_manager_(&electric_heater_, &hydraulic_heater_),
      air_recycling_manager_(dac),
      humidity_manager_(&air_recycling_manager_),
      session_manager_(&temperature_manager_, &humidity_manager_),
      settings_manager_(),
      program_loader_(),
      total_duty_time_s_(0),
      last_duty_time_save_(0),
      start_time_(0),
      settings_changed_callback_(nullptr),
      inlet_temperature_(0.0f),
      outlet_temperature_(0.0f),
      water_temperature_(0.0f),
      inlet_humidity_(0.0f),
      outlet_humidity_(0.0f),
      fan_output_(0.0f),
      last_control_update_(0)
{
}

void Dryer::Begin()
{
  // Initialize managers
  temperature_manager_.Begin();
  air_recycling_manager_.Begin();
  humidity_manager_.Begin();
  session_manager_.Begin();
  settings_manager_.Begin();

  // Initialize program loader and load programs from LittleFS
  if (program_loader_.Begin()) {
    uint8_t count = program_loader_.LoadPrograms();
    if (count > 0) {
      // Set default program (first one loaded)
      session_manager_.SetProgram(program_loader_.GetDefaultProgram());
      Serial.print("Loaded ");
      Serial.print(count);
      Serial.println(" program(s) from LittleFS");
    } else {
      Serial.println("Warning: No programs found in /programs/");
    }
  } else {
    Serial.println("Warning: Failed to initialize LittleFS");
  }

  // Load settings from EEPROM
  LoadSettings();

  Serial.println("Dryer initialized");
}

void Dryer::Start()
{
  if (!session_manager_.IsRunning())
  {
    Serial.println("Starting dryer...");
    start_time_ = millis();
    last_duty_time_save_ = millis();
    session_manager_.Start();

    // Save state to EEPROM immediately
    settings_manager_.SaveDryerState(true);

    // Trigger a full settings save
    NotifySettingsChanged();
  }
}

void Dryer::Stop()
{
  if (session_manager_.IsRunning())
  {
    Serial.println("Stopping dryer...");
    session_manager_.Stop();

    // Reset state (duty times) but keep parameters
    total_duty_time_s_ = 0;
    start_time_ = 0;
    last_duty_time_save_ = 0;

    Serial.println("Dryer state reset (duty times cleared, parameters preserved)");

    // Save reset state to EEPROM
    SaveSettings();
  }
}

void Dryer::Update()
{
  if (session_manager_.IsRunning())
  {
    // Update control (with interval)
    UpdateControl();

    // Update ventilation control
    UpdateVentilationControl();

    // Update air recycling control
    air_recycling_manager_.Update();

    // Update duty time tracking
    UpdateDutyTime();
  }
  else
  {
    // Ensure outputs are off when not running
    fan_output_ = 0.0f;
  }
}

void Dryer::UpdateControl()
{
  unsigned long now = millis();

  if (now - last_control_update_ >= kControlUpdateInterval)
  {
    last_control_update_ = now;

    // Update session manager with current sensor values
    Serial.print("[Dryer] Calling SessionManager.Update() - inlet_temp=");
    Serial.print(inlet_temperature_);
    Serial.print(", outlet_humidity=");
    Serial.println(outlet_humidity_);
    session_manager_.Update(inlet_temperature_, outlet_humidity_);

    // Update temperature manager with current temperature
    temperature_manager_.Update(inlet_temperature_);

    // Update humidity manager with current humidity values
    humidity_manager_.Update(inlet_humidity_, outlet_humidity_);
  }
}

void Dryer::UpdateVentilationControl()
{
  // Fan is always on when session is running
  if (session_manager_.IsRunning())
  {
    fan_output_ = 1.0f;
  }
  else
  {
    fan_output_ = 0.0f;
  }
}

const char *Dryer::GetPhaseName() const
{
  return session_manager_.GetCurrentPhaseName();
}

uint8_t Dryer::GetCurrentPhaseId() const
{
  return session_manager_.GetCurrentPhaseId();
}

uint8_t Dryer::GetCurrentCycleIndex() const
{
  return session_manager_.GetCurrentCycleIndex();
}

unsigned long Dryer::GetElapsedTime() const
{
  unsigned long current_session_time = 0;
  if (session_manager_.IsRunning())
  {
    current_session_time = (millis() - start_time_) / 1000;
  }
  return total_duty_time_s_ + current_session_time;
}

unsigned long Dryer::GetPhaseElapsedTime() const
{
  return session_manager_.GetPhaseElapsedTime();
}

void Dryer::SetTargetTemperature(float temperature)
{
  temperature_manager_.SetTargetTemperature(temperature);
  Serial.print("Target temperature set to: ");
  Serial.println(temperature);
}

float Dryer::GetTargetTemperature() const
{
  return temperature_manager_.GetTargetTemperature();
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
  // Get current program ID (default to 0 if no program is set)
  uint8_t current_program_id = 0;
  const Program* current_program = session_manager_.GetProgram();
  if (current_program != nullptr) {
    current_program_id = current_program->id;
  }

  // Save with new session state format
  settings_manager_.SaveSessionState(
      session_manager_.IsRunning(),
      session_manager_.GetCurrentCycleIndex(),
      session_manager_.GetCurrentPhaseIndexInCycle(),
      session_manager_.GetPhaseElapsedTime(),
      session_manager_.GetCycleElapsedTime(),
      current_program_id,
      temperature_manager_.GetParams(),
      total_duty_time_s_,
      air_recycling_manager_.GetRecyclingRate(),
      temperature_manager_.GetHydraulicEnabled(),
      temperature_manager_.GetElectricEnabled());
}

void Dryer::LoadSettings()
{
  bool saved_running_state = false;
  uint8_t saved_cycle_index = 0;
  uint8_t saved_phase_index = 0;
  uint32_t saved_phase_elapsed = 0;
  uint32_t saved_cycle_elapsed = 0;
  uint8_t saved_program_id = 0;
  TemperatureParams temp_params;
  uint32_t saved_duty_time = 0;
  float saved_recycling_rate = 50.0f;
  bool saved_hydraulic_enabled = true;
  bool saved_electric_enabled = true;

  bool success = settings_manager_.LoadSessionState(
      saved_running_state,
      saved_cycle_index,
      saved_phase_index,
      saved_phase_elapsed,
      saved_cycle_elapsed,
      saved_program_id,
      temp_params,
      saved_duty_time,
      saved_recycling_rate,
      saved_hydraulic_enabled,
      saved_electric_enabled);

  if (success)
  {
    // Restore program if available
    if (saved_program_id > 0) {
      const Program* saved_program = program_loader_.GetProgramById(saved_program_id);
      if (saved_program != nullptr) {
        session_manager_.SetProgram(saved_program);
        Serial.print("Restored program: ");
        Serial.println(saved_program->name);
      } else {
        Serial.print("Warning: Program ID ");
        Serial.print(saved_program_id);
        Serial.println(" not found, using default program");
      }
    }

    // Restore parameters
    temperature_manager_.GetParams() = temp_params;
    total_duty_time_s_ = saved_duty_time;
    air_recycling_manager_.SetRecyclingRate(saved_recycling_rate);
    temperature_manager_.SetHydraulicEnabled(saved_hydraulic_enabled);
    temperature_manager_.SetElectricEnabled(saved_electric_enabled);

    // Restore running state if dryer was running before reboot
    if (saved_running_state)
    {
      Serial.println("Dryer was running before reboot, resuming...");
      start_time_ = millis();
      last_duty_time_save_ = millis();
      session_manager_.RestoreState(saved_cycle_index, saved_phase_index,
                                     saved_phase_elapsed, saved_cycle_elapsed);
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
  return temperature_manager_.GetParams().temperature_target;
}

void Dryer::SetTemperatureTarget(float value)
{
  temperature_manager_.GetParams().temperature_target = value;
  temperature_manager_.SetTargetTemperature(value);

  // If in manual mode, update the manual program
  const Program* current_program = session_manager_.GetProgram();
  if (current_program != nullptr && current_program->id == 255) {
    // Get current humidity max to preserve it
    float current_hr_max = humidity_manager_.GetTargetHumidity();
    program_loader_.UpdateManualProgram(value, current_hr_max);
    Serial.println("[Dryer] Manual program updated with new temperature");
  }

  NotifySettingsChanged();
}

float Dryer::GetTemperatureDeadband() const
{
  return temperature_manager_.GetParams().temperature_deadband;
}

void Dryer::SetTemperatureDeadband(float value)
{
  temperature_manager_.GetParams().temperature_deadband = value;
  NotifySettingsChanged();
}

float Dryer::GetHeatingActionMinWait() const
{
  return temperature_manager_.GetParams().heating_action_min_wait_s;
}

void Dryer::SetHeatingActionMinWait(float value)
{
  temperature_manager_.GetParams().heating_action_min_wait_s = value;
  NotifySettingsChanged();
}

float Dryer::GetHeaterStepMin() const
{
  return temperature_manager_.GetParams().heater_step_min;
}

void Dryer::SetHeaterStepMin(float value)
{
  temperature_manager_.GetParams().heater_step_min = value;
  NotifySettingsChanged();
}

float Dryer::GetHeaterStepMax() const
{
  return temperature_manager_.GetParams().heater_step_max;
}

void Dryer::SetHeaterStepMax(float value)
{
  temperature_manager_.GetParams().heater_step_max = value;
  NotifySettingsChanged();
}

float Dryer::GetHeaterFullScaleDelta() const
{
  return temperature_manager_.GetParams().heater_full_scale_delta;
}

void Dryer::SetHeaterFullScaleDelta(float value)
{
  temperature_manager_.GetParams().heater_full_scale_delta = value;
  NotifySettingsChanged();
}

float Dryer::GetHumidityMax() const
{
  return humidity_manager_.GetTargetHumidity();
}

void Dryer::SetHumidityMax(float value)
{
  humidity_manager_.SetTargetHumidity(value);

  // If in manual mode, update the manual program
  const Program* current_program = session_manager_.GetProgram();
  if (current_program != nullptr && current_program->id == 255) {
    // Get current temperature target to preserve it
    float current_temp = temperature_manager_.GetTargetTemperature();
    program_loader_.UpdateManualProgram(current_temp, value);
    Serial.println("[Dryer] Manual program updated with new humidity max");
  }

  NotifySettingsChanged();
}

bool Dryer::GetHydraulicEnabled() const
{
  return temperature_manager_.GetHydraulicEnabled();
}

void Dryer::SetHydraulicEnabled(bool enabled)
{
  temperature_manager_.SetHydraulicEnabled(enabled);
  NotifySettingsChanged();
}

bool Dryer::GetElectricEnabled() const
{
  return temperature_manager_.GetElectricEnabled();
}

void Dryer::SetElectricEnabled(bool enabled)
{
  temperature_manager_.SetElectricEnabled(enabled);
  NotifySettingsChanged();
}
