#include "SessionManager.h"
#include "Logger.h"

SessionManager::SessionManager(TemperatureManager *temperature_manager,
                               HumidityManager    *humidity_manager)
    : temperature_manager_(temperature_manager),
      humidity_manager_(humidity_manager),
      state_(SessionState::kStopped),
      current_phase_(DryerPhase::kStop),
      user_target_humidity_(0.0f),
      init_extraction_end_ms_(0),
      phase_start_ms_(0),
      session_start_ms_(0) {}

void SessionManager::Begin()
{
  state_                   = SessionState::kStopped;
  current_phase_           = DryerPhase::kStop;
  user_target_humidity_    = 0.0f;
  init_extraction_end_ms_  = 0;
  phase_start_ms_          = 0;
  session_start_ms_        = 0;
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
  EnterPhase(DryerPhase::kInit);
  Logger::Info("SessionManager: session started");
}

void SessionManager::Stop()
{
  state_         = SessionState::kStopped;
  current_phase_ = DryerPhase::kStop;

  // Turn off heaters
  temperature_manager_->GetElectricHeater()->SetPower(0.0f);
  temperature_manager_->GetHydraulicHeater()->SetPower(0.0f);

  // Disable humidity control and close damper
  humidity_manager_->SetTargetHumidity(0.0f);
  humidity_manager_->SetMode(HumidityManager::Mode::kDisabled);
  humidity_manager_->ResetCooldown();

  Logger::Info("SessionManager: session stopped");
}

const char *SessionManager::GetCurrentPhaseName() const
{
  switch (current_phase_)
  {
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
  phase_start_ms_   = millis() - phase_elapsed_s * 1000UL;
  session_start_ms_ = millis() - total_elapsed_s * 1000UL;
  EnterPhase(phase);
  Logger::Info("SessionManager: state restored (phase=%s, elapsed=%us)",
               GetCurrentPhaseName(), total_elapsed_s);
}

// --- Private ---

void SessionManager::EnterPhase(DryerPhase phase)
{
  current_phase_  = phase;
  phase_start_ms_ = millis();

  switch (phase)
  {
    case DryerPhase::kInit:
      temperature_manager_->SetTargetTemperature(
          temperature_manager_->GetTargetTemperature());
      humidity_manager_->SetMode(HumidityManager::Mode::kDisabled);
      humidity_manager_->ResetCooldown();
      init_extraction_end_ms_ = 0;
      // Start hydraulic at full power for initial ramp-up
      temperature_manager_->GetHydraulicHeater()->SetPower(100.0f);
      temperature_manager_->GetElectricHeater()->SetPower(0.0f);
      Logger::Info("SessionManager: entering Init phase");
      break;

    case DryerPhase::kBrassage:
      humidity_manager_->SetMode(HumidityManager::Mode::kDisabled);
      humidity_manager_->ResetCooldown();
      Logger::Info("SessionManager: entering Brassage phase");
      break;

    case DryerPhase::kExtraction:
      // Force damper open for the full extraction phase duration
      humidity_manager_->SetMode(HumidityManager::Mode::kForceOpen);
      humidity_manager_->ResetCooldown();
      Logger::Info("SessionManager: entering Extraction phase");
      break;

    default:
      break;
  }
}

void SessionManager::CheckPhaseTransition(float current_temperature, float current_humidity)
{
  uint32_t elapsed = GetPhaseElapsedTime();

  switch (current_phase_)
  {
    case DryerPhase::kInit:
    {
      // Humidity check: if target reached, extract within init or transition to Extraction
      if (user_target_humidity_ > 0.0f)
      {
        if (init_extraction_end_ms_ != 0)
        {
          // Sub-extraction in progress within init
          if (millis() >= init_extraction_end_ms_)
          {
            init_extraction_end_ms_ = 0;
            humidity_manager_->SetMode(HumidityManager::Mode::kDisabled);
            Logger::Info("SessionManager: Init extraction done");
          }
        }
        else if (current_humidity >= user_target_humidity_)
        {
          uint32_t remaining = INIT_PHASE_DURATION - elapsed;
          if (remaining > EXTRACTION_DAMPER_OPEN_DURATION)
          {
            init_extraction_end_ms_ = millis() + (uint32_t)EXTRACTION_DAMPER_OPEN_DURATION * 1000UL;
            humidity_manager_->SetMode(HumidityManager::Mode::kForceOpen);
            Logger::Info("SessionManager: Init humidity reached, extracting for %us",
                         EXTRACTION_DAMPER_OPEN_DURATION);
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
      bool timed_out    = (elapsed >= INIT_PHASE_DURATION);
      if (temp_reached || timed_out)
      {
        Logger::Info("SessionManager: Init -> Brassage (%s)",
                     temp_reached ? "temp reached" : "timeout");
        EnterPhase(DryerPhase::kBrassage);
      }
      break;
    }

    case DryerPhase::kBrassage:
      if (user_target_humidity_ > 0.0f && current_humidity >= user_target_humidity_)
      {
        Logger::Info("SessionManager: Brassage -> Extraction (humidity reached)");
        EnterPhase(DryerPhase::kExtraction);
        break;
      }
      if (elapsed >= BRASSAGE_PHASE_DURATION)
      {
        Logger::Info("SessionManager: Brassage -> Extraction");
        EnterPhase(DryerPhase::kExtraction);
      }
      break;

    case DryerPhase::kExtraction:
      // Run for full duration to remove maximum moisture
      if (elapsed >= EXTRACTION_PHASE_DURATION)
      {
        Logger::Info("SessionManager: Extraction -> Brassage");
        EnterPhase(DryerPhase::kBrassage);
      }
      break;

    default:
      break;
  }
}
