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

  // Call each control cycle with fresh sensor readings.
  void Update(float inlet_humidity, float outlet_humidity);

  // Set operating mode (called by SessionManager on phase transitions).
  void SetMode(Mode mode);
  Mode GetMode() const { return mode_; }

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
  float    target_humidity_;
  float    current_inlet_humidity_;
  uint32_t action_next_allowed_ms_;

  static constexpr float    kDeadband   = 5.0f;   // %RH hysteresis
  static constexpr uint32_t kCooldownMs = 10000;  // 10 s between actions

  bool IsActionAllowed() const;
  void ArmCooldown();
};

#endif // HUMIDITY_MANAGER_H
