#ifndef HUMIDITY_MANAGER_H
#define HUMIDITY_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "AirDamper.h"

// Controls the binary air damper to manage inlet humidity.
//
// Behaviour:
//   target = 0   -> damper closed (no humidity control / recirculation)
//   target > 0   -> open damper when humidity > target, close when humidity <= target
//
// A cooldown of 10 s is enforced between state changes to avoid hunting.
class HumidityManager
{
public:
  explicit HumidityManager(AirDamper *air_damper);

  void Begin();

  // Call each control cycle with fresh sensor readings.
  void Update(float inlet_humidity, float outlet_humidity);

  // Set humidity threshold (%RH).  0 = no control (damper closed).
  void  SetTargetHumidity(float target);
  float GetTargetHumidity()   const { return target_humidity_; }
  float GetCurrentHumidity()  const { return current_inlet_humidity_; }

  // Returns true when the damper is in the correct state for the current target.
  bool IsHumidityTargetReached() const;

  // Reset the cooldown timer (call when entering a new phase).
  void ResetCooldown();

private:
  AirDamper *air_damper_;

  float    target_humidity_;
  float    current_inlet_humidity_;
  uint32_t action_next_allowed_ms_;

  static constexpr float    kDeadband   = 5.0f;   // %RH hysteresis
  static constexpr uint32_t kCooldownMs = 10000;  // 10 s between actions

  bool IsActionAllowed() const;
  void ArmCooldown();
};

#endif // HUMIDITY_MANAGER_H
