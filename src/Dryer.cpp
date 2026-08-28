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
      dehum_extraction_threshold_(DEHUM_EXTRACTION_THRESHOLD_DEFAULT),
      last_control_update_ms_(0),
      purging_(false),
      purge_since_ms_(0),
      purge_fault_(DryerFault::kNone) {}

void Dryer::Begin()
{
  temperature_manager_.Begin();
  humidity_manager_.Begin();
  session_manager_.Begin();

  Logger::Info("Dryer: initialized");
}

DryerFault Dryer::FaultReason() const
{
  DryerFault fault = DryerFault::kNone;

  if (air_damper_.IsAirflowBlocked())
  {
    fault = DryerFault::kAirflowBlocked;
  }
  else if (!air_damper_.IsFeedbackUsable())
  {
    fault = DryerFault::kDamperFeedback;
  }
  else if (!temperature_manager_.GetHeatingPermitted())
  {
    fault = DryerFault::kSensorStale;
  }
  else if (temperature_manager_.GetHydraulicEnabled() &&
           !temperature_manager_.GetHydraulicOnline())
  {
    fault = DryerFault::kHydraulicOffline;
  }

  return ApplyFaultGrace(fault, millis());
}

void Dryer::Start()
{
  if (session_manager_.IsRunning()) return;

  // No session begins while anything is wrong. The refusal reads from the same
  // FaultReason() the panel LED does, so a button that will not take is always
  // a button next to a blinking red LED and an alarm line naming the cause —
  // the operator is never left pressing a dead switch.
  //
  // It sits here rather than at the button, so every route into a session — the
  // button, whatever comes later — goes through the same door. RestoreSession()
  // deliberately does not: a reboot mid-batch has to pick the batch back up, and
  // Update() below stops it within the loop if the air path is genuinely shut.
  DryerFault fault = FaultReason();
  if (fault != DryerFault::kNone)
  {
    Logger::Warning("Dryer: start refused — %s", FaultName(fault));
    return;
  }

  temperature_manager_.SetFanActive(true);
  session_manager_.Start();
  Logger::Info("Dryer: session started");
}

void Dryer::Stop()
{
  if (!session_manager_.IsRunning()) return;

  // Cleared directly rather than through EndPurge(), which announces a recovery
  // that is not happening here. The damper is left where the purge put it: no
  // HumidityManager::Update() runs during the cooldown, so an extraction opened
  // to shed heat stays open for the fan to finish the job through.
  purging_     = false;
  purge_fault_ = DryerFault::kNone;
  humidity_manager_.SetForceOpen(HumidityManager::ForceOpen::kNone);

  temperature_manager_.SetFanActive(false);
  session_manager_.Stop();
  Logger::Info("Dryer: session stopped");
}

void Dryer::Update()
{
  session_manager_.UpdateCooldown();

  // Checked before anything else: no heating logic is worth running on a dryer
  // that has to come down.
  //
  // This is also the interlock a session restored from flash goes through:
  // RestoreSession() takes no preconditions, because a reboot mid-batch has to
  // pick the batch back up, and one pass through here is all it takes to bring
  // it down again if the machine it woke into is not fit to run.
  if (session_manager_.IsRunning())
  {
    UpdateFaultResponse();
  }

  if (!session_manager_.IsRunning())
    return;

  uint32_t now = millis();
  if (now - last_control_update_ms_ >= kControlIntervalMs)
  {
    last_control_update_ms_ = now;
    UpdateControl();
  }
}

// What a running session does about a fault: nothing, purge and wait, or stop.
void Dryer::UpdateFaultResponse()
{
  DryerFault fault   = FaultReason();
  uint32_t   holdoff = FaultStopHoldoffMs(fault);
  uint32_t   now     = millis();

  if (holdoff == kFaultNeverStops)
  {
    EndPurge();
    return;
  }

  // A different fault restarts the clock: the new one has its own deadline and
  // no claim on time already served under the old.
  if (!purging_ || fault != purge_fault_)
  {
    purging_        = true;
    purge_fault_    = fault;
    purge_since_ms_ = now;

    if (holdoff > 0)
    {
      // Shed heat rather than sit and hope. The probe is the control input, so
      // this is exactly the window in which nothing can be measured — including
      // by the safety_max cutoff, which is reading the same dead probe. Opening
      // the extraction is the one heat-removal action left that does not depend
      // on knowing the temperature.
      temperature_manager_.AllOff();
      humidity_manager_.SetForceOpen(HumidityManager::ForceOpen::kPurge);
      Logger::Error("Dryer: %s — heat off, extraction open, stopping in %u s",
                    FaultName(fault), holdoff / 1000U);
    }
  }

  if (now - purge_since_ms_ >= holdoff)
  {
    Logger::Error("Dryer: stopping session — %s", FaultName(purge_fault_));
    Stop();
  }
}

void Dryer::EndPurge()
{
  if (!purging_) return;

  purging_     = false;
  purge_fault_ = DryerFault::kNone;

  // Deliberately not forced shut. Letting the next HumidityManager::Update()
  // apply the phase's own mode is the whole point of the override sitting above
  // it — and on the path where the session ends instead, that update never
  // comes, so the register stays open through the fan cooldown, which is where
  // an open extraction was wanted anyway.
  humidity_manager_.SetForceOpen(HumidityManager::ForceOpen::kNone);

  // The register is about to travel back to whatever the phase wanted, and the
  // chamber to climb out of however far the purge let it fall. That is an air
  // renewal by any other name, so it gets the same tolerance.
  temperature_manager_.NotifyAirRenewal();

  Logger::Info("Dryer: fault cleared — session resumes");
}

void Dryer::UpdateControl()
{
  // Phase transitions are suspended for the duration of a purge. Init, Brassage
  // and Extraction all end on a humidity threshold, and the reading behind that
  // threshold is stale — being stale is what started the purge. Letting them
  // run would have a batch reach a finish it never actually got to, on the
  // strength of the last number the probe managed to send.
  if (!purging_)
  {
    session_manager_.Update(inlet_temperature_, inlet_humidity_);
  }

  temperature_manager_.Update(inlet_temperature_, inlet_humidity_);

  // After the temperature manager, so the reason is decided against this
  // cycle's own effective setpoint, and before the humidity manager, which is
  // what acts on it.
  UpdateForcedOpen();

  humidity_manager_.Update(inlet_humidity_);
}

// Why, if at all, a dehumidifier's register is being held open against the
// phase. Both reasons are air renewals; neither exists with a resistance
// fitted, which throws its air away as a matter of course.
//
// The purge is not decided here — it is a safety, it outranks both of these,
// and UpdateFaultResponse() owns it. Reaching this function at all means no
// purge is running, since a purge suspends the control loop's phase machine but
// not the loop, and the ranking is enforced by simply not touching the override
// while purging_ is set.
void Dryer::UpdateForcedOpen()
{
  using ForceOpen = HumidityManager::ForceOpen;

  if (purging_) return;

  if (!temperature_manager_.IsDehumidifier())
  {
    if (humidity_manager_.SetForceOpen(ForceOpen::kNone))
    {
      temperature_manager_.NotifyAirRenewal();
    }
    return;
  }

  const float setpoint = temperature_manager_.GetEffectiveTargetTemperature();
  const float target_humidity = humidity_manager_.GetTargetHumidity();
  const ForceOpen current = humidity_manager_.GetForceOpen();

  ForceOpen wanted = ForceOpen::kNone;

  // Too hot. Opens at setpoint + threshold, closes at the setpoint itself, so
  // the threshold *is* the hysteresis — a separate return value would be a
  // second setting saying the same thing.
  if (!isnan(inlet_temperature_))
  {
    bool open_now = (current == ForceOpen::kOverheat)
                        ? inlet_temperature_ > setpoint
                        : inlet_temperature_ > setpoint + dehum_extraction_threshold_;
    if (open_now) wanted = ForceOpen::kOverheat;
  }

  // Too dry. Below the target there is no water left to condense and the
  // machine has nothing to work on; renewing brings damp air back in and the
  // drying continues. Ranked under overheating, which is the more urgent of the
  // two and wants the same register anyway.
  if (wanted == ForceOpen::kNone && target_humidity > 0.0f && !isnan(inlet_humidity_))
  {
    bool open_now = (current == ForceOpen::kAirRenewal)
                        ? inlet_humidity_ < target_humidity + DEHUM_RENEWAL_BAND
                        : inlet_humidity_ < target_humidity;
    if (open_now) wanted = ForceOpen::kAirRenewal;
  }

  if (humidity_manager_.SetForceOpen(wanted))
  {
    // The register is about to travel and the air behind the probe to be
    // replaced. Every movement the dryer commands says so; these are the two
    // that used to be able to happen without anyone being told.
    temperature_manager_.NotifyAirRenewal();
  }
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
  temperature_manager_.SetTargetHumidity(humidity);
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
  temperature_manager_.SetHeatSource(
      settings.heat_source == HEAT_SOURCE_DEHUMIDIFIER ? HeatSourceType::kDehumidifier
                                                       : HeatSourceType::kElectric);
  dehum_extraction_threshold_ = settings.dehum_extraction_threshold;

  TemperatureParams &params = temperature_manager_.GetParams();
  params.band_electric      = settings.band_electric;
  params.horizon_electric   = settings.horizon_electric;
  params.electric_t_on_min  = settings.electric_t_on_min;
  params.electric_t_off_min = settings.electric_t_off_min;
  params.air_renewal_window = settings.air_renewal_window;
  params.safety_max         = settings.safety_max;
  params.eco_start_hour        = settings.eco_start_hour;
  params.eco_end_hour          = settings.eco_end_hour;
  params.eco_target_percentage = settings.eco_target_percentage;

  // Without an RTC there is no wall clock, so the ECO window cannot be
  // evaluated and the mode stays off whatever the stored setting says.
  bool eco = settings.eco_enabled && rtc_available;
  temperature_manager_.SetOperatingMode(eco ? OperatingMode::ECO
                                            : OperatingMode::PERFORMANCE);

  session_manager_.SetProgram(settings.program == DRYER_PROGRAM_CLIMATE
                                  ? DryerProgram::kClimate
                                  : DryerProgram::kDrying);

  PhaseDurations &durations = session_manager_.GetDurations();
  durations.init                   = settings.init_phase_duration;
  durations.brassage               = settings.brassage_phase_duration;
  durations.extraction             = settings.extraction_phase_duration;
  durations.extraction_damper_open = settings.extraction_damper_open_duration;

  DamperConfig damper{};
  damper.count                = settings.damper_count;
  damper.feedback_low_is_open = settings.damper_feedback_low_is_open;
  damper.extraction_inverted  = settings.damper_extraction_inverted;
  damper.recycling_inverted   = settings.damper_recycling_inverted;
  damper.extraction_raw_min   = settings.extraction_raw_min;
  damper.extraction_raw_max   = settings.extraction_raw_max;
  damper.recycling_raw_min    = settings.recycling_raw_min;
  damper.recycling_raw_max    = settings.recycling_raw_max;
  air_damper_.ApplyConfig(damper);
}

void Dryer::CaptureSettings(DryerSettings &settings) const
{
  const TemperatureParams &params = temperature_manager_.GetParams();

  settings.target_temperature = params.temperature_target;
  settings.target_humidity    = humidity_manager_.GetTargetHumidity();

  settings.hydraulic_enabled = temperature_manager_.GetHydraulicEnabled();
  settings.electric_enabled  = temperature_manager_.GetElectricEnabled();
  settings.heat_source       = static_cast<uint8_t>(temperature_manager_.GetHeatSource());
  settings.dehum_extraction_threshold = dehum_extraction_threshold_;

  settings.band_electric      = params.band_electric;
  settings.horizon_electric   = params.horizon_electric;
  settings.electric_t_on_min  = params.electric_t_on_min;
  settings.electric_t_off_min = params.electric_t_off_min;
  settings.air_renewal_window = params.air_renewal_window;
  settings.safety_max         = params.safety_max;

  settings.eco_enabled           = temperature_manager_.IsEcoActive();
  settings.eco_start_hour        = params.eco_start_hour;
  settings.eco_end_hour          = params.eco_end_hour;
  settings.eco_target_percentage = params.eco_target_percentage;

  settings.program = static_cast<uint8_t>(session_manager_.GetProgram());

  const PhaseDurations &durations = session_manager_.GetDurations();
  settings.init_phase_duration             = durations.init;
  settings.brassage_phase_duration         = durations.brassage;
  settings.extraction_phase_duration       = durations.extraction;
  settings.extraction_damper_open_duration = durations.extraction_damper_open;

  settings.damper_count                = air_damper_.GetCount();
  settings.damper_feedback_low_is_open = air_damper_.Extraction().GetLowIsOpen();
  settings.damper_extraction_inverted  = air_damper_.GetExtractionInverted();
  settings.damper_recycling_inverted   = air_damper_.GetRecyclingInverted();

  settings.extraction_raw_min = air_damper_.Extraction().GetRawMin();
  settings.extraction_raw_max = air_damper_.Extraction().GetRawMax();
  settings.recycling_raw_min  = air_damper_.Recycling().GetRawMin();
  settings.recycling_raw_max  = air_damper_.Recycling().GetRawMax();
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
