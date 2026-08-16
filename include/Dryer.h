#ifndef DRYER_H
#define DRYER_H

#include <Arduino.h>
#include "config.h"
#include "ElectricHeater.h"
#include "TemperatureManager.h"
#include "HumidityManager.h"
#include "AirDamper.h"
#include "SessionManager.h"
#include "DryerSettings.h"

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

  // Current hour from RTC — forwarded to TemperatureManager each loop
  void SetCurrentHour(uint8_t hour) { temperature_manager_.SetCurrentHour(hour); }

  // True when ECO switch is ON and current time is inside the ECO night window
  bool IsEcoWindowActive() const { return temperature_manager_.IsEcoWindowActive(); }

  // Outputs
  float GetHeaterOutput()   const;  // electric 0.0/1.0
  bool  GetHydraulicOn()    const { return temperature_manager_.GetHydraulicOn(); }
  float GetFanOutput()      const { return session_manager_.IsFanActive() ? 1.0f : 0.0f; }
  bool  GetDamperOutput()   const { return air_damper_.IsOpen(); }

  // Manager access
  TemperatureManager *GetTemperatureManager() { return &temperature_manager_; }
  HumidityManager    *GetHumidityManager()    { return &humidity_manager_; }
  SessionManager     *GetSessionManager()     { return &session_manager_; }
  AirDamper          *GetAirDamper()          { return &air_damper_; }

  // Session restoration after reboot — driven by SettingsStore.
  void RestoreSession(DryerPhase phase, uint32_t phase_elapsed_s, uint32_t total_elapsed_s);

  // Push a settings record into the live managers, and read the current values
  // back out for persistence. ECO is only applied when an RTC is present.
  void ApplySettings(const DryerSettings &settings, bool rtc_available);
  void CaptureSettings(DryerSettings &settings) const;

  // Current session progress, for SettingsStore.
  void CaptureSession(SessionSnapshot &session) const;

private:
  ElectricHeater     electric_heater_;
  AirDamper          air_damper_;
  TemperatureManager temperature_manager_;
  HumidityManager    humidity_manager_;
  SessionManager     session_manager_;

  float inlet_temperature_;
  float outlet_temperature_;
  float inlet_humidity_;
  float outlet_humidity_;

  uint32_t last_control_update_ms_;
  static constexpr uint32_t kControlIntervalMs = CONTROL_LOOP_INTERVAL;

  void UpdateControl();
};

#endif // DRYER_H
