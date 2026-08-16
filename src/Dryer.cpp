#include "Dryer.h"
#include "Logger.h"

Dryer::Dryer()
    : electric_heater_(),
      air_damper_(),
      temperature_manager_(&electric_heater_),
      humidity_manager_(&air_damper_),
      session_manager_(&temperature_manager_, &humidity_manager_),
      inlet_temperature_(0.0f),
      outlet_temperature_(0.0f),
      inlet_humidity_(0.0f),
      outlet_humidity_(0.0f),
      last_control_update_ms_(0) {}

void Dryer::Begin()
{
  temperature_manager_.Begin();
  humidity_manager_.Begin();
  session_manager_.Begin();

  Logger::Info("Dryer: initialized");
}

void Dryer::Start()
{
  if (session_manager_.IsRunning()) return;
  temperature_manager_.SetFanActive(true);
  session_manager_.Start();
  Logger::Info("Dryer: session started");
}

void Dryer::Stop()
{
  if (!session_manager_.IsRunning()) return;
  temperature_manager_.SetFanActive(false);
  session_manager_.Stop();
  Logger::Info("Dryer: session stopped");
}

void Dryer::Update()
{
  session_manager_.UpdateCooldown();

  if (!session_manager_.IsRunning())
    return;

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
  session_manager_.SetTargetHumidity(humidity);
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

void Dryer::RestoreSession(DryerPhase phase, uint32_t phase_elapsed_s, uint32_t total_elapsed_s)
{
  temperature_manager_.SetFanActive(true);
  session_manager_.RestoreState(phase, phase_elapsed_s, total_elapsed_s);
  Logger::Info("Dryer: session restored (phase=%s elapsed=%us)",
               session_manager_.GetCurrentPhaseName(), total_elapsed_s);
}
