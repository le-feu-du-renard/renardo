#include "Dryer.h"
#include "Logger.h"

Dryer::Dryer()
    : electric_heater_(),
      hydraulic_heater_(),
      air_damper_(),
      temperature_manager_(&electric_heater_, &hydraulic_heater_),
      humidity_manager_(&air_damper_),
      session_manager_(&temperature_manager_, &humidity_manager_),
      state_manager_(),
      inlet_temperature_(0.0f),
      outlet_temperature_(0.0f),
      inlet_humidity_(0.0f),
      outlet_humidity_(0.0f),
      fan_output_(0.0f),
      last_control_update_ms_(0) {}

void Dryer::Begin()
{
  temperature_manager_.Begin();
  humidity_manager_.Begin();
  session_manager_.Begin();
  state_manager_.Begin();

  LoadSettings();
  Logger::Info("Dryer: initialized");
}

void Dryer::Start()
{
  if (session_manager_.IsRunning()) return;
  session_manager_.Start();
  Logger::Info("Dryer: session started");
}

void Dryer::Stop()
{
  if (!session_manager_.IsRunning()) return;
  session_manager_.Stop();
  state_manager_.Save(false, DryerPhase::kStop, 0, 0);
  Logger::Info("Dryer: session stopped");
}

void Dryer::Update()
{
  if (!session_manager_.IsRunning())
  {
    fan_output_ = 0.0f;
    return;
  }

  fan_output_ = 1.0f;  // Fan always on during session

  uint32_t now = millis();
  if (now - last_control_update_ms_ >= kControlIntervalMs)
  {
    last_control_update_ms_ = now;
    UpdateControl();
  }
}

void Dryer::UpdateControl()
{
  session_manager_.Update(inlet_temperature_, inlet_humidity_);
  temperature_manager_.Update(inlet_temperature_);
  humidity_manager_.Update(inlet_humidity_, outlet_humidity_);
}

const char *Dryer::GetPhaseName() const
{
  return session_manager_.GetCurrentPhaseName();
}

DryerPhase Dryer::GetCurrentPhase() const
{
  return session_manager_.GetCurrentPhase();
}

uint32_t Dryer::GetTotalElapsedTime() const
{
  return session_manager_.GetTotalElapsedTime();
}

uint32_t Dryer::GetPhaseElapsedTime() const
{
  return session_manager_.GetPhaseElapsedTime();
}

void Dryer::SetTargetTemperature(float temperature)
{
  temperature_manager_.SetTargetTemperature(temperature);
}

void Dryer::SetTargetHumidity(float humidity)
{
  humidity_manager_.SetTargetHumidity(humidity);
}

float Dryer::GetTargetTemperature() const
{
  return temperature_manager_.GetTargetTemperature();
}

void Dryer::SetOperatingMode(OperatingMode mode)
{
  temperature_manager_.SetOperatingMode(mode);
}

float Dryer::GetHeaterOutput() const
{
  return electric_heater_.GetOutput();
}

float Dryer::GetCirculatorOutput() const
{
  return hydraulic_heater_.GetOutput();
}

void Dryer::SaveSettings()
{
  state_manager_.Save(
      session_manager_.IsRunning(),
      session_manager_.GetCurrentPhase(),
      session_manager_.GetPhaseElapsedTime(),
      session_manager_.GetTotalElapsedTime());
}

void Dryer::LoadSettings()
{
  DryerPhase phase         = DryerPhase::kStop;
  uint32_t   phase_elapsed = 0;
  uint32_t   total_elapsed = 0;

  if (state_manager_.Load(phase, phase_elapsed, total_elapsed))
  {
    session_manager_.RestoreState(phase, phase_elapsed, total_elapsed);
    Logger::Info("Dryer: session restored from EEPROM");
  }
  else
  {
    Logger::Info("Dryer: no session to restore");
  }
}
