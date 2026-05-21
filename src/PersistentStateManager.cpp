#include "PersistentStateManager.h"
#include "Logger.h"
#include <SD.h>

using SDFile = SDLib::File;

static constexpr size_t kStateSize = sizeof(PersistentState);

PersistentStateManager::PersistentStateManager() : state_() {}

void PersistentStateManager::Begin()
{
  Logger::Info("PersistentStateManager: using SD card (%s, %u bytes)", kStateFilePath, (unsigned)kStateSize);
}

void PersistentStateManager::Save(bool session_running, DryerPhase phase,
                                  uint32_t phase_elapsed_s, uint32_t total_elapsed_s)
{
  state_.version         = kStateVersion;
  state_.session_running = session_running;
  state_.phase           = phase;
  state_.phase_elapsed_s = phase_elapsed_s;
  state_.total_elapsed_s = total_elapsed_s;
  WriteToSD();
  Logger::Info("PersistentStateManager: saved (running=%d, phase=%d, total=%us)",
               session_running, (int)phase, total_elapsed_s);
}

bool PersistentStateManager::Load(DryerPhase &phase, uint32_t &phase_elapsed_s,
                                  uint32_t &total_elapsed_s)
{
  if (!ReadFromSD())
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
  WriteToSD();
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

void PersistentStateManager::WriteToSD()
{
  state_.checksum = CalculateChecksum(state_);
  SD.remove(kStateFilePath);
  SDFile f = SD.open(kStateFilePath, FILE_WRITE);
  if (!f)
  {
    Logger::Error("PersistentStateManager: cannot open %s for write", kStateFilePath);
    return;
  }
  f.write(reinterpret_cast<const uint8_t *>(&state_), kStateSize);
  f.close();
}

bool PersistentStateManager::ReadFromSD()
{
  if (!SD.exists(kStateFilePath))
    return false;
  SDFile f = SD.open(kStateFilePath, FILE_READ);
  if (!f)
    return false;
  int n = f.read(reinterpret_cast<uint8_t *>(&state_), kStateSize);
  f.close();
  if (n != (int)kStateSize)
  {
    Logger::Warning("PersistentStateManager: short read (%d/%u bytes)", n, (unsigned)kStateSize);
    return false;
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
