#ifndef AIR_RECYCLING_MANAGER_H
#define AIR_RECYCLING_MANAGER_H

#include <Arduino.h>
#include <DFRobot_GP8403.h>

/**
 * Manages air recycling rate control using DAC GP8403
 *
 * Controls the ratio between fresh air and recycled air:
 * - 0%: 100% fresh air (no recycling)
 * - 100%: 100% recycled air (no fresh air)
 *
 * The DAC outputs a voltage signal (0-10V) to control a damper or valve.
 */
class AirRecyclingManager {
 public:
  /**
   * Constructor
   * @param dac Pointer to DFRobot_GP8403 DAC instance
   * @param channel DAC channel to use (0 or 1)
   */
  AirRecyclingManager(DFRobot_GP8403* dac, uint8_t channel = 0);

  /**
   * Initialize the air recycling manager
   */
  void Begin();

  /**
   * Set the air recycling rate
   * @param rate Recycling rate (0.0 to 100.0%)
   */
  void SetRecyclingRate(float rate);

  /**
   * Get the current air recycling rate
   * @return Current recycling rate (0.0 to 100.0%)
   */
  float GetRecyclingRate() const { return recycling_rate_; }

  /**
   * Update the DAC output based on current recycling rate
   */
  void Update();

 private:
  static constexpr float kMinRecyclingRate = 0.0f;
  static constexpr float kMaxRecyclingRate = 100.0f;
  static constexpr float kDefaultRecyclingRate = 50.0f;
  static constexpr uint16_t kMaxDacValue = 4095;  // 12-bit DAC

  DFRobot_GP8403* dac_;
  uint8_t channel_;
  float recycling_rate_;

  void ApplyRecyclingRate();
};

#endif  // AIR_RECYCLING_MANAGER_H
