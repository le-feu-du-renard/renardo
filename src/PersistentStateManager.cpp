#include "PersistentStateManager.h"
#include "Logger.h"

static constexpr size_t kStateSize = sizeof(PersistentState);

PersistentStateManager::PersistentStateManager() : state_() {}

void PersistentStateManager::Begin()
{
  EEPROM.begin(kStateSize);
  Logger::Info("PersistentStateManager: EEPROM initialized (%u bytes)", (unsigned)kStateSize);
}

void PersistentStateManager::Save(bool session_running, DryerPhase phase,
                                  uint32_t phase_elapsed_s, uint32_t total_elapsed_s)
{
  state_.version         = kStateVersion;
  state_.session_running = session_running;
  state_.phase           = phase;
  state_.phase_elapsed_s = phase_elapsed_s;
  state_.total_elapsed_s = total_elapsed_s;
  WriteToEeprom();
  Logger::Info("PersistentStateManager: saved (running=%d, phase=%d, total=%us)",
               session_running, (int)phase, total_elapsed_s);
}

bool PersistentStateManager::Load(DryerPhase &phase, uint32_t &phase_elapsed_s,
                                  uint32_t &total_elapsed_s)
{
  if (!ReadFromEeprom())
  {
    Logger::Warning("PersistentStateManager: no valid saved state, using defaults");
    return false;
  }
  if (!state_.session_running)
  {
    Logger::Info("PersistentStateManager: last session was stopped, not restoring");
    return false;
  }
  phase           = state_.phase;
  phase_elapsed_s = state_.phase_elapsed_s;
  total_elapsed_s = state_.total_elapsed_s;
  Logger::Info("PersistentStateManager: loaded running session (phase=%d, total=%us)",
               (int)phase, total_elapsed_s);
  return true;
}

void PersistentStateManager::Reset()
{
  state_ = PersistentState();
  WriteToEeprom();
  Logger::Info("PersistentStateManager: reset to defaults");
}

uint16_t PersistentStateManager::CalculateChecksum(const PersistentState &s) const
{
  uint16_t       sum  = 0;
  const uint8_t *data = reinterpret_cast<const uint8_t *>(&s);
  size_t         stop = offsetof(PersistentState, checksum);
  for (size_t i = 0; i < stop; i++) sum += data[i];
  return sum;
}

void PersistentStateManager::WriteToEeprom()
{
  state_.checksum        = CalculateChecksum(state_);
  const uint8_t *data    = reinterpret_cast<const uint8_t *>(&state_);
  for (size_t i = 0; i < kStateSize; i++)
  {
    EEPROM.write(kEepromAddress + i, data[i]);
  }
  if (!EEPROM.commit())
  {
    Logger::Error("PersistentStateManager: EEPROM commit failed");
  }
}

bool PersistentStateManager::ReadFromEeprom()
{
  uint8_t *data = reinterpret_cast<uint8_t *>(&state_);
  for (size_t i = 0; i < kStateSize; i++)
  {
    data[i] = EEPROM.read(kEepromAddress + i);
  }
  if (state_.version != kStateVersion)
  {
    Logger::Warning("PersistentStateManager: version mismatch (expected %u, got %u)",
                    kStateVersion, state_.version);
    return false;
  }
  if (CalculateChecksum(state_) != state_.checksum)
  {
    Logger::Warning("PersistentStateManager: checksum mismatch");
    return false;
  }
  return true;
}
