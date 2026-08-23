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
#include "StatusIndicator.h"

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

  // Sensor inputs — the inlet probe is the only measurement the control loop
  // has ever used.
  void  SetInletTemperature(float t) { inlet_temperature_ = t; }
  void  SetInletHumidity(float h)    { inlet_humidity_ = h; }

  float GetInletTemperature() const { return inlet_temperature_; }
  float GetInletHumidity()    const { return inlet_humidity_; }

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
  float GetHeaterOutput()      const;  // electric 0.0/1.0
  bool  GetHydraulicDemand()   const { return temperature_manager_.GetHydraulicDemand(); }
  float GetFanOutput()      const { return session_manager_.IsFanActive() ? 1.0f : 0.0f; }
  bool  GetDamperOutput()   const { return air_damper_.IsOpen(); }

  // Air path faults, for the radio telemetry, which reports the two conditions
  // separately rather than as one ranked reason.
  bool GetAirflowBlocked() const { return air_damper_.IsAirflowBlocked(); }
  bool GetDamperFeedbackFault() const { return !air_damper_.IsFeedbackUsable(); }

  // The single worst thing wrong with the dryer, or kNone.
  //
  // One ranked reason rather than a set of flags, because everything downstream
  // wants exactly one: the LED blinks or it does not, the alarm band has room
  // for one line, and Start() refuses with one message. Ranking them here is
  // what keeps the panel, the screen and the interlock from disagreeing about
  // which of two simultaneous faults the operator is being told about.
  //
  // A missing hydraulic module only counts when the source is enabled in the
  // menu — a dryer fitted with no hydraulic at all would otherwise blink red
  // for ever, and the way out of that fault is the menu toggle, not a spanner.
  //
  // Graced through ApplyFaultGrace, so this is safe to call from the first loop:
  // during the boot window the probe and the module have not been asked yet.
  DryerFault FaultReason() const;

  bool HasFault() const { return FaultReason() != DryerFault::kNone; }

  // True while a session is being ridden out under a fault that will end it:
  // heat off, extraction held open, phase transitions suspended, and the stop
  // coming at the end of FaultStopHoldoffMs(). Only a silent inlet probe gets
  // one — see FaultStopHoldoffMs() for why the other two do not.
  bool IsPurging() const { return purging_; }

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
  float inlet_humidity_;

  uint32_t last_control_update_ms_;
  static constexpr uint32_t kControlIntervalMs = CONTROL_LOOP_INTERVAL;

  // The hold-off in progress, if any. `purge_fault_` is kept so that a second,
  // different fault arriving mid-purge restarts the clock against its own
  // deadline rather than inheriting the first one's.
  bool       purging_;
  uint32_t   purge_since_ms_;
  DryerFault purge_fault_;

  void UpdateControl();
  void UpdateFaultResponse();
  void EndPurge();
};

#endif // DRYER_H
