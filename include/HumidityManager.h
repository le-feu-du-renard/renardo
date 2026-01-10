#ifndef HUMIDITY_MANAGER_H
#define HUMIDITY_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "AirRecyclingManager.h"

/**
 * Humidity control parameters
 */
struct HumidityParams {
  float humidity_deadband;           // Deadband for humidity control (default: 5%)
  float recycling_step_min;          // Minimum step for recycling rate adjustment
  float recycling_step_max;          // Maximum step for recycling rate adjustment
  float action_min_wait_s;           // Minimum time between actions

  HumidityParams()
    : humidity_deadband(5.0f),
      recycling_step_min(5.0f),
      recycling_step_max(20.0f),
      action_min_wait_s(10.0f) {}
};

/**
 * Manages humidity control via air recycling
 *
 * Controls the air recycling rate to achieve target humidity:
 * - humidity_target = -1: Minimize humidity (recycling = 0%, max fresh air)
 * - humidity_target = 0: No humidity control (recycling unchanged)
 * - humidity_target > 0: Target specific humidity level
 */
class HumidityManager {
 public:
  /**
   * Constructor
   * @param air_recycling_manager Pointer to AirRecyclingManager instance
   */
  HumidityManager(AirRecyclingManager* air_recycling_manager);

  /**
   * Initialize the humidity manager
   */
  void Begin();

  /**
   * Update humidity control based on current readings
   * @param inlet_humidity Current inlet humidity (%)
   * @param outlet_humidity Current outlet humidity (%)
   */
  void Update(float inlet_humidity, float outlet_humidity);

  /**
   * Set the target humidity
   * @param target -1 = minimize humidity, 0 = no control, >0 = target %
   */
  void SetTargetHumidity(float target);

  /**
   * Get the current target humidity
   * @return Current target humidity setting
   */
  float GetTargetHumidity() const { return target_humidity_; }

  /**
   * Check if current humidity has reached the target
   * @return true if target reached (or no target set)
   */
  bool IsHumidityTargetReached() const;

  /**
   * Get current outlet humidity (last reading)
   * @return Current outlet humidity
   */
  float GetCurrentHumidity() const { return current_outlet_humidity_; }

  // Parameters
  HumidityParams& GetParams() { return params_; }
  const HumidityParams& GetParams() const { return params_; }

  // Reset action cooldown
  void ResetCooldown();

 private:
  AirRecyclingManager* air_recycling_manager_;
  HumidityParams params_;

  float target_humidity_;           // -1 = min, 0 = off, >0 = target
  float current_inlet_humidity_;
  float current_outlet_humidity_;
  unsigned long action_next_allowed_ms_;

  // Check if action is allowed (cooldown)
  bool IsActionAllowed() const;
  void ArmActionCooldown();

  // Recycling rate adjustments (return true if value changed)
  bool IncreaseRecycling();
  bool DecreaseRecycling();

  // Calculate adaptive step based on error
  float CalculateStep() const;
};

#endif  // HUMIDITY_MANAGER_H
