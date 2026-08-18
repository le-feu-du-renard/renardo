#include "Dryer.h"
#include "Logger.h"

Dryer::Dryer()
    : electric_heater_(),
      air_damper_(),
      temperature_manager_(&electric_heater_),
      humidity_manager_(&air_damper_),
      session_manager_(&temperature_manager_, &humidity_manager_),
      inlet_temperature_(0.0f),
      inlet_humidity_(0.0f),
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
  humidity_manager_.Update(inlet_humidity_);
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

void Dryer::ApplySettings(const DryerSettings &settings, bool rtc_available)
{
  temperature_manager_.SetTargetTemperature(settings.target_temperature);
  SetTargetHumidity(settings.target_humidity);

  temperature_manager_.SetHydraulicEnabled(settings.hydraulic_enabled);
  temperature_manager_.SetElectricEnabled(settings.electric_enabled);

  TemperatureParams &params = temperature_manager_.GetParams();
  params.band_hydraulic      = settings.band_hydraulic;
  params.band_electric       = settings.band_electric;
  params.horizon_hydraulic   = settings.horizon_hydraulic;
  params.horizon_electric    = settings.horizon_electric;
  params.hydraulic_t_on_min  = settings.hydraulic_t_on_min;
  params.hydraulic_t_off_min = settings.hydraulic_t_off_min;
  params.electric_t_on_min   = settings.electric_t_on_min;
  params.electric_t_off_min  = settings.electric_t_off_min;
  params.safety_max          = settings.safety_max;
  params.eco_start_hour        = settings.eco_start_hour;
  params.eco_end_hour          = settings.eco_end_hour;
  params.eco_target_percentage = settings.eco_target_percentage;

  // Without an RTC there is no wall clock, so the ECO window cannot be
  // evaluated and the mode stays off whatever the stored setting says.
  bool eco = settings.eco_enabled && rtc_available;
  temperature_manager_.SetOperatingMode(eco ? OperatingMode::ECO
                                            : OperatingMode::PERFORMANCE);

  PhaseDurations &durations = session_manager_.GetDurations();
  durations.init                   = settings.init_phase_duration;
  durations.brassage               = settings.brassage_phase_duration;
  durations.extraction             = settings.extraction_phase_duration;
  durations.extraction_damper_open = settings.extraction_damper_open_duration;

  air_damper_.SetCalibration(settings.damper_raw_closed, settings.damper_raw_open);
}

void Dryer::CaptureSettings(DryerSettings &settings) const
{
  const TemperatureParams &params = temperature_manager_.GetParams();

  settings.target_temperature = params.temperature_target;
  settings.target_humidity    = humidity_manager_.GetTargetHumidity();

  settings.hydraulic_enabled = temperature_manager_.GetHydraulicEnabled();
  settings.electric_enabled  = temperature_manager_.GetElectricEnabled();

  settings.band_hydraulic      = params.band_hydraulic;
  settings.band_electric       = params.band_electric;
  settings.horizon_hydraulic   = params.horizon_hydraulic;
  settings.horizon_electric    = params.horizon_electric;
  settings.hydraulic_t_on_min  = params.hydraulic_t_on_min;
  settings.hydraulic_t_off_min = params.hydraulic_t_off_min;
  settings.electric_t_on_min   = params.electric_t_on_min;
  settings.electric_t_off_min  = params.electric_t_off_min;
  settings.safety_max          = params.safety_max;

  settings.eco_enabled           = temperature_manager_.IsEcoActive();
  settings.eco_start_hour        = params.eco_start_hour;
  settings.eco_end_hour          = params.eco_end_hour;
  settings.eco_target_percentage = params.eco_target_percentage;

  const PhaseDurations &durations = session_manager_.GetDurations();
  settings.init_phase_duration             = durations.init;
  settings.brassage_phase_duration         = durations.brassage;
  settings.extraction_phase_duration       = durations.extraction;
  settings.extraction_damper_open_duration = durations.extraction_damper_open;

  settings.damper_raw_closed = air_damper_.GetRawClosed();
  settings.damper_raw_open   = air_damper_.GetRawOpen();
}

void Dryer::CaptureSession(SessionSnapshot &session) const
{
  session.running         = session_manager_.IsRunning();
  session.phase           = static_cast<uint8_t>(session_manager_.GetCurrentPhase());
  session.phase_elapsed_s = session_manager_.GetPhaseElapsedTime();
  session.total_elapsed_s = session_manager_.GetTotalElapsedTime();
}

void Dryer::RestoreSession(DryerPhase phase, uint32_t phase_elapsed_s, uint32_t total_elapsed_s)
{
  temperature_manager_.SetFanActive(true);
  session_manager_.RestoreState(phase, phase_elapsed_s, total_elapsed_s);
  Logger::Info("Dryer: session restored (phase=%s elapsed=%us)",
               session_manager_.GetCurrentPhaseName(), total_elapsed_s);
}
