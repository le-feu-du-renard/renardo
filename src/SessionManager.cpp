#include "SessionManager.h"
#include "Logger.h"

SessionManager::SessionManager(TemperatureManager *temperature_manager,
                               HumidityManager    *humidity_manager)
    : temperature_manager_(temperature_manager),
      humidity_manager_(humidity_manager),
      state_(SessionState::kStopped),
      current_phase_(DryerPhase::kStop),
      program_(static_cast<DryerProgram>(DRYER_PROGRAM_DEFAULT)),
      user_target_humidity_(0.0f),
      init_extraction_end_ms_(0),
      phase_start_ms_(0),
      session_start_ms_(0),
      cooldown_end_ms_(0) {}

void SessionManager::Begin()
{
  state_                   = SessionState::kStopped;
  current_phase_           = DryerPhase::kStop;
  user_target_humidity_    = 0.0f;
  init_extraction_end_ms_  = 0;
  phase_start_ms_          = 0;
  session_start_ms_        = 0;
  cooldown_end_ms_         = 0;
  Logger::Info("SessionManager: initialized");
}

void SessionManager::Update(float current_temperature, float current_humidity)
{
  if (state_ != SessionState::kRunning) return;
  CheckPhaseTransition(current_temperature, current_humidity);
}

void SessionManager::Start()
{
  state_            = SessionState::kRunning;
  session_start_ms_ = millis();
  EnterPhase(IsClimate() ? DryerPhase::kClimat : DryerPhase::kInit);
  Logger::Info("SessionManager: session started (%s)",
               IsClimate() ? "climate" : "drying");
}

void SessionManager::Stop()
{
  state_           = SessionState::kCooling;
  current_phase_   = DryerPhase::kStop;
  cooldown_end_ms_ = millis() + (uint32_t)FAN_COOLDOWN_DURATION_S * 1000UL;

  // Turn off both heat sources immediately
  temperature_manager_->AllOff();

  // Disable humidity control and close damper. Not through SetDamperMode(): a
  // stop is not a transient to be ridden out, and there is nothing left running
  // for a tolerance window to protect.
  humidity_manager_->SetTargetHumidity(0.0f);
  humidity_manager_->SetMode(HumidityManager::Mode::kDisabled);
  humidity_manager_->ResetCooldown();

  Logger::Info("SessionManager: session stopped — cooling fan for %us", FAN_COOLDOWN_DURATION_S);
}

void SessionManager::UpdateCooldown()
{
  if (state_ == SessionState::kCooling && millis() >= cooldown_end_ms_)
  {
    state_ = SessionState::kStopped;
    Logger::Info("SessionManager: fan cooldown complete");
  }
}

const char *SessionManager::GetCurrentPhaseName() const
{
  switch (current_phase_)
  {
    case DryerPhase::kClimat:     return "Climat";
    case DryerPhase::kInit:       return "Init";
    case DryerPhase::kBrassage:   return "Brassage";
    case DryerPhase::kExtraction: return "Extraction";
    default:                      return "Stop";
  }
}

uint32_t SessionManager::GetPhaseElapsedTime() const
{
  if (phase_start_ms_ == 0) return 0;
  return (millis() - phase_start_ms_) / 1000UL;
}

uint32_t SessionManager::GetTotalElapsedTime() const
{
  if (session_start_ms_ == 0) return 0;
  return (millis() - session_start_ms_) / 1000UL;
}

void SessionManager::RestoreState(DryerPhase phase, uint32_t phase_elapsed_s,
                                  uint32_t total_elapsed_s)
{
  state_            = SessionState::kRunning;
  current_phase_    = phase;
  session_start_ms_ = millis() - total_elapsed_s * 1000UL;
  EnterPhase(phase);
  // Override phase_start_ms_ after EnterPhase() which resets it to millis()
  phase_start_ms_   = millis() - phase_elapsed_s * 1000UL;
  Logger::Info("SessionManager: state restored (phase=%s, phase_elapsed=%us, total=%us)",
               GetCurrentPhaseName(), phase_elapsed_s, total_elapsed_s);
}

// --- Private ---

void SessionManager::EnterPhase(DryerPhase phase)
{
  current_phase_  = phase;
  phase_start_ms_ = millis();

  // Only Init resets the control outright, because only Init is a session
  // start. Brassage and Extraction arm the air-renewal window instead: cutting
  // the heater and clearing its timers at the exact moment cold air arrives is
  // what used to make every transition a step backwards.
  switch (phase)
  {
    case DryerPhase::kClimat:
      // The register on the humidity threshold, which is what holding a climate
      // means: open while the air is too damp, shut once it is not. kThreshold
      // has been implemented since v3 and this is the first thing to select it.
      //
      // Except with a dehumidifier fitted, which takes the water out without
      // opening anything. Its register answers overheating and dry air instead,
      // from Dryer::UpdateForcedOpen(), and a threshold underneath would be a
      // second opinion on the same vane.
      SetDamperMode(temperature_manager_->IsDehumidifier()
                        ? HumidityManager::Mode::kDisabled
                        : HumidityManager::Mode::kThreshold);
      init_extraction_end_ms_ = 0;
      temperature_manager_->ResetControl();
      Logger::Info("SessionManager: entering Climat phase");
      break;

    case DryerPhase::kInit:
      SetDamperMode(HumidityManager::Mode::kDisabled);
      init_extraction_end_ms_ = 0;
      temperature_manager_->ResetControl();
      Logger::Info("SessionManager: entering Init phase");
      break;

    case DryerPhase::kBrassage:
      SetDamperMode(HumidityManager::Mode::kDisabled);
      Logger::Info("SessionManager: entering Brassage phase");
      break;

    case DryerPhase::kExtraction:
      // Force damper open for the full extraction phase duration
      SetDamperMode(HumidityManager::Mode::kForceOpen);
      Logger::Info("SessionManager: entering Extraction phase");
      break;

    default:
      break;
  }
}

// Every damper movement the session commands goes through here, so the
// regulation is told about all of them and not just the ones that happen to
// coincide with a phase change — the Init sub-extraction is a damper movement
// in the middle of a phase, and it used to tell the regulation nothing at all.
void SessionManager::SetDamperMode(HumidityManager::Mode mode)
{
  bool changed = humidity_manager_->SetMode(mode);
  humidity_manager_->ResetCooldown();
  if (changed)
  {
    temperature_manager_->NotifyAirRenewal();
  }
}

bool SessionManager::ExtractionIsUsed() const
{
  return !temperature_manager_->IsDehumidifier();
}

void SessionManager::CheckPhaseTransition(float current_temperature, float current_humidity)
{
  uint32_t elapsed = GetPhaseElapsedTime();

  switch (current_phase_)
  {
    case DryerPhase::kClimat:
      // Nowhere to go, ever. A climate is held until someone stops it; there is
      // no duration to serve and no next stage to reach.
      break;

    case DryerPhase::kInit:
    {
      // Humidity check: if target reached, extract within init or transition to
      // Extraction. Skipped entirely with a dehumidifier fitted, which has no
      // use for either — see ExtractionIsUsed().
      if (user_target_humidity_ > 0.0f && ExtractionIsUsed())
      {
        if (init_extraction_end_ms_ != 0)
        {
          // Sub-extraction in progress within init
          if (millis() >= init_extraction_end_ms_)
          {
            init_extraction_end_ms_ = 0;
            SetDamperMode(HumidityManager::Mode::kDisabled);
            Logger::Info("SessionManager: Init extraction done");
          }
        }
        else if (current_humidity >= user_target_humidity_)
        {
          uint32_t remaining = durations_.init - elapsed;
          if (remaining > durations_.extraction_damper_open)
          {
            init_extraction_end_ms_ = millis() + (uint32_t)durations_.extraction_damper_open * 1000UL;
            SetDamperMode(HumidityManager::Mode::kForceOpen);
            Logger::Info("SessionManager: Init humidity reached, extracting for %us",
                         durations_.extraction_damper_open);
          }
          else
          {
            Logger::Info("SessionManager: Init humidity reached, going to Extraction");
            EnterPhase(DryerPhase::kExtraction);
            break;
          }
        }
      }
      // Transition when temperature target is reached OR max duration elapsed
      bool temp_reached = (current_temperature >= temperature_manager_->GetTargetTemperature());
      bool timed_out    = (elapsed >= durations_.init);
      if (temp_reached || timed_out)
      {
        Logger::Info("SessionManager: Init -> Brassage (%s)",
                     temp_reached ? "temp reached" : "timeout");
        EnterPhase(DryerPhase::kBrassage);
      }
      break;
    }

    case DryerPhase::kBrassage:
      // With a dehumidifier there is nowhere to go: Brassage is the whole
      // running state, and the register answers to temperature and humidity
      // rather than to the phase clock.
      if (!ExtractionIsUsed()) break;

      if (user_target_humidity_ > 0.0f && current_humidity >= user_target_humidity_)
      {
        Logger::Info("SessionManager: Brassage -> Extraction (humidity reached)");
        EnterPhase(DryerPhase::kExtraction);
        break;
      }
      if (elapsed >= durations_.brassage)
      {
        Logger::Info("SessionManager: Brassage -> Extraction");
        EnterPhase(DryerPhase::kExtraction);
      }
      break;

    case DryerPhase::kExtraction:
      // A source swapped mid-session leaves the phase machine in a phase that
      // no longer exists for it. Leave at once rather than serve out a duration
      // that is now meaningless.
      if (!ExtractionIsUsed())
      {
        Logger::Info("SessionManager: Extraction -> Brassage (dehumidifier fitted)");
        EnterPhase(DryerPhase::kBrassage);
        break;
      }
      // Run for full duration to remove maximum moisture
      if (elapsed >= durations_.extraction)
      {
        Logger::Info("SessionManager: Extraction -> Brassage");
        EnterPhase(DryerPhase::kBrassage);
      }
      break;

    default:
      break;
  }
}
