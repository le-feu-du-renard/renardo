#ifndef HYDRAULIC_REMOTE_H
#define HYDRAULIC_REMOTE_H

#include <Arduino.h>
#include "RemoteModule.h"
#include "Rs485Bus.h"

// Client for the remote hydraulic module on RS485 bus "ext".
//
// The module owns the three-way valve, the circulator, and its own regulation:
// it decides when to fire and holds the water at the setpoint it is given. What
// travels from here is a **run permission** and that setpoint — not an on/off
// command. The dryer raises the permission for as long as the source is enabled
// and a session is running with the interlocks holding, and regulates the air
// temperature with the electric heater alone. Cycling the module on air
// temperature would put a second regulator on a valve that takes minutes to
// travel, and the slower of the two controllers is not this one.
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

  // Run permission — pushed to the module on the next Update().
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  bool GetEnabled() const { return enabled_; }

  // Fixed water setpoint in C, set from the menu.
  void  SetWaterTarget(float celsius);
  float GetWaterTarget() const { return water_target_; }

  // Dryer inlet air temperature, so the module can tell whether circulating
  // would move heat into the dryer or out of it. NAN when the dryer has no
  // fresh reading.
  void SetDryerAirTemperature(float celsius) { dryer_air_temperature_ = celsius; }

  // One exchange with the module: write the command block, then read
  // telemetry. Returns true when both transactions succeed.
  bool Update();

  float    GetWaterTemperature() const { return water_temperature_; }
  float    GetTankTemperature()  const { return tank_temperature_; }
  float    GetPumpSpeed()        const { return pump_speed_percent_; }
  uint16_t GetStatusBits()       const { return status_bits_; }

private:
  bool  enabled_;
  float water_target_;
  float dryer_air_temperature_;

  float    water_temperature_;
  float    tank_temperature_;
  float    pump_speed_percent_;
  uint16_t status_bits_;
};

#endif // HYDRAULIC_REMOTE_H
