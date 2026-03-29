#ifndef DRYER_H
#define DRYER_H

#include <Arduino.h>
#include "ElectricHeater.h"
#include "HydraulicHeater.h"
#include "TemperatureManager.h"
#include "HumidityManager.h"
#include "AirDamper.h"
#include "SessionManager.h"
#include "PersistentStateManager.h"

// Main dryer coordinator.
// Owns all hardware managers and forwards sensor readings / mode updates to them.
class Dryer
{
public:
  Dryer();

  void Begin();
  void Update();

  // Session control
  void Start();
  void Stop();
  bool IsRunning() const { return session_manager_.IsRunning(); }

  // State
  const char *GetPhaseName()        const;
  DryerPhase  GetCurrentPhase()     const;
  uint32_t    GetTotalElapsedTime() const;
  uint32_t    GetPhaseElapsedTime() const;

  // Sensor inputs
  void  SetInletTemperature(float t)  { inlet_temperature_ = t; }
  void  SetOutletTemperature(float t) { outlet_temperature_ = t; }
  void  SetInletHumidity(float h)     { inlet_humidity_ = h; }
  void  SetOutletHumidity(float h)    { outlet_humidity_ = h; }

  float GetInletTemperature()  const { return inlet_temperature_; }
  float GetOutletTemperature() const { return outlet_temperature_; }
  float GetInletHumidity()     const { return inlet_humidity_; }
  float GetOutletHumidity()    const { return outlet_humidity_; }

  // Target setpoints (from potentiometers, updated each loop)
  void  SetTargetTemperature(float temperature);
  float GetTargetTemperature() const;
  void  SetTargetHumidity(float humidity);

  // Operating mode (from physical mode selector, updated each loop)
  void SetOperatingMode(OperatingMode mode);

  // Outputs
  float GetHeaterOutput()     const;  // electric 0.0/1.0
  float GetCirculatorOutput() const;  // hydraulic 0-100%
  float GetFanOutput()        const { return fan_output_; }
  bool  GetDamperOutput()     const { return air_damper_.IsOpen(); }

  // Manager access
  TemperatureManager *GetTemperatureManager() { return &temperature_manager_; }
  HumidityManager    *GetHumidityManager()    { return &humidity_manager_; }
  SessionManager     *GetSessionManager()     { return &session_manager_; }
  PersistentStateManager *GetPersistentStateManager() { return &state_manager_; }

  // Settings persistence
  void SaveSettings();
  void LoadSettings();

private:
  ElectricHeater     electric_heater_;
  HydraulicHeater    hydraulic_heater_;
  AirDamper          air_damper_;
  TemperatureManager temperature_manager_;
  HumidityManager    humidity_manager_;
  SessionManager     session_manager_;
  PersistentStateManager state_manager_;

  float inlet_temperature_;
  float outlet_temperature_;
  float inlet_humidity_;
  float outlet_humidity_;
  float fan_output_;

  uint32_t last_control_update_ms_;
  static constexpr uint32_t kControlIntervalMs = 1000;

  void UpdateControl();
};

#endif // DRYER_H
