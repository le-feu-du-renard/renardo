#ifndef HYDRAULIC_REMOTE_H
#define HYDRAULIC_REMOTE_H

#include <Arduino.h>
#include "RemoteModule.h"
#include "Rs485Bus.h"

// Client for the remote hydraulic module on RS485 bus A.
//
// The module owns the three-way valve and the circulator. It accepts only a
// requested state and a fixed water setpoint, and reports the circulating and
// storage-tank water temperatures. The valve is far too slow to be modulated
// from here, so the dryer commands it on/off and trims the air temperature
// with the electric heater instead.
//
// Register map is declared in config.h (HYDRO_REG_*) so the module firmware
// and this client stay in agreement.
//
// Address, health and backoff come from RemoteModule; what is left here is the
// two register blocks and their meaning.
class HydraulicRemote : public RemoteModule
{
public:
  // The module is considered unavailable after this long without an answer.
  static constexpr uint32_t kTimeoutMs = 30000;

  // It carries the heat, so it is poked back to life four times more often than
  // the extension port — losing it degrades the dryer to electric-only.
  static constexpr uint32_t kRetryMs = 10000;

  explicit HydraulicRemote(Rs485Bus *bus);

  void Begin();

  // Requested state — pushed to the module on the next Update().
  void SetState(bool on) { requested_state_ = on; }
  bool GetRequestedState() const { return requested_state_; }

  // Fixed water setpoint in C, set from the menu.
  void  SetWaterTarget(float celsius);
  float GetWaterTarget() const { return water_target_; }

  // One exchange with the module: write the command, then read telemetry.
  // Returns true when both transactions succeed.
  bool Update();

  float    GetWaterTemperature() const { return water_temperature_; }
  float    GetTankTemperature()  const { return tank_temperature_; }
  uint16_t GetStatusBits()       const { return status_bits_; }

private:
  bool  requested_state_;
  float water_target_;

  float    water_temperature_;
  float    tank_temperature_;
  uint16_t status_bits_;
};

#endif // HYDRAULIC_REMOTE_H
