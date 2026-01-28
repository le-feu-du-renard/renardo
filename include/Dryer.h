#ifndef DRYER_H
#define DRYER_H

#include <Arduino.h>
#include "ElectricHeater.h"
#include "HydraulicHeater.h"
#include "TemperatureManager.h"
#include "HumidityManager.h"
#include "SessionManager.h"
#include "SettingsManager.h"
#include "AirRecyclingManager.h"
#include "ProgramLoader.h"

// Callback type for settings changed notification
typedef void (*SettingsChangedCallback)();

/**
 * Main dryer controller
 * Coordinates managers and handles high-level logic
 */
class Dryer
{
public:
  Dryer(DFRobot_GP8403 *dac);

  void Begin();
  void Update();

  // Control
  void Start();
  void Stop();
  bool IsRunning() const { return session_manager_.IsRunning(); }

  // State
  const char *GetPhaseName() const;
  uint8_t GetCurrentPhaseId() const;
  uint8_t GetCurrentCycleIndex() const;
  unsigned long GetElapsedTime() const;
  unsigned long GetPhaseElapsedTime() const;

  // Temperatures
  void SetInletTemperature(float temperature) { inlet_temperature_ = temperature; }
  void SetOutletTemperature(float temperature) { outlet_temperature_ = temperature; }
  void SetWaterTemperature(float temperature) { water_temperature_ = temperature; }
  void SetTargetTemperature(float temperature);

  float GetInletTemperature() const { return inlet_temperature_; }
  float GetOutletTemperature() const { return outlet_temperature_; }
  float GetWaterTemperature() const { return water_temperature_; }
  float GetTargetTemperature() const;

  // Humidity
  void SetInletHumidity(float humidity) { inlet_humidity_ = humidity; }
  void SetOutletHumidity(float humidity) { outlet_humidity_ = humidity; }
  float GetInletHumidity() const { return inlet_humidity_; }
  float GetOutletHumidity() const { return outlet_humidity_; }

  // Outputs (0.0 to 1.0)
  float GetHeaterOutput() const;
  float GetFanOutput() const { return fan_output_; }
  float GetCirculatorOutput() const;

  // Air recycling control
  void SetRecyclingRate(float rate);
  float GetRecyclingRate() const;

  // Manager access
  TemperatureManager *GetTemperatureManager() { return &temperature_manager_; }
  HumidityManager *GetHumidityManager() { return &humidity_manager_; }
  SessionManager *GetSessionManager() { return &session_manager_; }
  SettingsManager *GetSettingsManager() { return &settings_manager_; }
  AirRecyclingManager *GetAirRecyclingManager() { return &air_recycling_manager_; }
  ProgramLoader *GetProgramLoader() { return &program_loader_; }

  // Backward compatibility aliases
  TemperatureManager *GetHeatersManager() { return &temperature_manager_; }

  // Duty time tracking
  uint32_t GetTotalDutyTime() const { return total_duty_time_s_; }

  // Settings persistence
  void SaveSettings();
  void LoadSettings();

  // Settings change notification
  void SetSettingsChangedCallback(SettingsChangedCallback callback) { settings_changed_callback_ = callback; }
  void NotifySettingsChanged();

  // Menu parameter access
  // Heating parameters
  float GetTemperatureTarget() const;
  void SetTemperatureTarget(float value);
  float GetTemperatureDeadband() const;
  void SetTemperatureDeadband(float value);
  float GetHeatingActionMinWait() const;
  void SetHeatingActionMinWait(float value);
  float GetHeaterStepMin() const;
  void SetHeaterStepMin(float value);
  float GetHeaterStepMax() const;
  void SetHeaterStepMax(float value);
  float GetHeaterFullScaleDelta() const;
  void SetHeaterFullScaleDelta(float value);

  // Energy source enable/disable
  bool GetHydraulicEnabled() const;
  void SetHydraulicEnabled(bool enabled);
  bool GetElectricEnabled() const;
  void SetElectricEnabled(bool enabled);

private:
  // Components
  ElectricHeater electric_heater_;
  HydraulicHeater hydraulic_heater_;
  TemperatureManager temperature_manager_;
  AirRecyclingManager air_recycling_manager_;
  HumidityManager humidity_manager_;
  SessionManager session_manager_;
  SettingsManager settings_manager_;
  ProgramLoader program_loader_;

  // State
  uint32_t total_duty_time_s_;
  unsigned long last_duty_time_save_;
  unsigned long start_time_;

  // Settings change callback
  SettingsChangedCallback settings_changed_callback_;

  // Sensors
  float inlet_temperature_;
  float outlet_temperature_;
  float water_temperature_;
  float inlet_humidity_;
  float outlet_humidity_;

  // Outputs
  float fan_output_;

  // Control update intervals
  unsigned long last_control_update_;
  static constexpr unsigned long kControlUpdateInterval = 1000; // 1s

  // Control logic
  void UpdateControl();
  void UpdateVentilationControl();
  void UpdateDutyTime();
};

#endif // DRYER_H
