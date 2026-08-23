#ifndef HUMIDITY_MANAGER_H
#define HUMIDITY_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "AirDamper.h"

// Controls the binary air damper to manage inlet humidity.
//
// Mode:
//   kDisabled  -> damper always closed (Init, Brassage)
//   kForceOpen -> damper always open   (Extraction, Init sub-extraction)
//   kThreshold -> open when humidity > target + deadband, close when <= target
//
// A cooldown of 10 s is enforced between state changes in kThreshold mode.
class HumidityManager
{
public:
  enum class Mode { kDisabled, kForceOpen, kThreshold };

  explicit HumidityManager(AirDamper *air_damper);

  void Begin();

  // Call each control cycle with a fresh inlet reading.
  void Update(float inlet_humidity);

  // Set operating mode (called by SessionManager on phase transitions).
  // Returns true when the mode actually changed, which is the caller's cue that
  // the register is about to travel and the air behind the probe to be replaced.
  bool SetMode(Mode mode);
  Mode GetMode() const { return mode_; }

  // Hold the damper open regardless of mode, to shed heat while the dryer is
  // riding out a fault it may yet be brought down by.
  //
  // An override above the mode rather than a mode of its own, because the mode
  // belongs to the phase and the phase has not ended: the purge is a thing
  // happening *to* a session, not a stage of one. Update() is a pure function
  // of mode every cycle, so clearing this restores whatever the phase wanted
  // with nothing to save and nothing to put back.
  void SetPurge(bool purge);
  bool IsPurging() const { return purge_; }

  // Set humidity threshold (%RH) — used for kThreshold mode and transition logic.
  void  SetTargetHumidity(float target);
  float GetTargetHumidity()   const { return target_humidity_; }
  float GetCurrentHumidity()  const { return current_inlet_humidity_; }

  // Returns true when inlet humidity is at or below the target.
  bool IsHumidityTargetReached() const;

  // Reset the cooldown timer (call when entering a new phase).
  void ResetCooldown();

private:
  AirDamper *air_damper_;

  Mode     mode_;
  bool     purge_;
  float    target_humidity_;
  float    current_inlet_humidity_;
  uint32_t action_next_allowed_ms_;

  static constexpr float    kDeadband   = 5.0f;   // %RH hysteresis
  static constexpr uint32_t kCooldownMs = 10000;  // 10 s between actions

  bool IsActionAllowed() const;
  void ArmCooldown();
};

#endif // HUMIDITY_MANAGER_H
