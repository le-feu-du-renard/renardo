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

  // Why the damper is being held open against what the mode wants.
  //
  //   kPurge      a fault the session may yet be brought down by: heat off,
  //               extraction open to shed it, because the safety cutoff is
  //               reading the same dead probe the fault came from.
  //   kOverheat   a dehumidifier's waste heat has carried the chamber past the
  //               setpoint, and outside air is the only way back down.
  //   kAirRenewal a dehumidifier has dried the air below the target, so there
  //               is nothing left in it to condense; renewing brings damp air
  //               back and the drying continues.
  //
  // Ranked, not a set: only one register and one command, so the question is
  // never which of two reasons applies but which one is being answered. kPurge
  // outranks the other two because it is the safety and they are the
  // regulation.
  enum class ForceOpen : uint8_t { kNone, kPurge, kOverheat, kAirRenewal };

  // An override above the mode rather than a mode of its own, because the mode
  // belongs to the phase and the phase has not ended: none of these is a stage
  // of a session, each is a thing happening *to* one. Update() is a pure
  // function of the mode every cycle, so clearing this restores whatever the
  // phase wanted with nothing to save and nothing to put back.
  //
  // Returns true when the reason actually changed, which is the caller's cue
  // that the register is about to travel and the air behind the probe to be
  // replaced — the same contract as SetMode(), and for the same reason.
  bool      SetForceOpen(ForceOpen reason);
  ForceOpen GetForceOpen() const { return force_open_; }
  bool      IsForcedOpen() const { return force_open_ != ForceOpen::kNone; }
  static const char *ForceOpenName(ForceOpen reason);

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

  Mode      mode_;
  ForceOpen force_open_;
  float    target_humidity_;
  float    current_inlet_humidity_;
  uint32_t action_next_allowed_ms_;

  static constexpr float    kDeadband   = 5.0f;   // %RH hysteresis
  static constexpr uint32_t kCooldownMs = 10000;  // 10 s between actions

  bool IsActionAllowed() const;
  void ArmCooldown();
};

#endif // HUMIDITY_MANAGER_H
