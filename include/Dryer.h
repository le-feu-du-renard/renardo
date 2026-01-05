#ifndef DRYER_H
#define DRYER_H

#include <Arduino.h>
#include <Wire.h>
#include "ElectricHeater.h"
#include "HydraulicHeater.h"
#include "HeatersManager.h"
#include "PhasesManager.h"
#include "SettingsManager.h"
#include "GP8403.h"
#include "AirRecyclingManager.h"

/**
 * Main dryer controller
 * Coordinates managers and handles high-level logic
 */
class Dryer {
 public:
  Dryer();

  void Begin();
  void Update();

  // Control
  void Start();
  void Stop();
  bool IsRunning() const { return running_; }

  // State
  const char* GetPhaseName() const;
  unsigned long GetElapsedTime() const;

  // Temperatures
  void SetInletTemperature(float temperature) { inlet_temperature_ = temperature; }
  void SetOutletTemperature(float temperature) { outlet_temperature_ = temperature; }
  void SetWaterTemperature(float temperature) { water_temperature_ = temperature; }
  void SetTargetTemperature(float temperature);

  float GetInletTemperature() const { return inlet_temperature_; }
  float GetOutletTemperature() const { return outlet_temperature_; }
  float GetTargetTemperature() const;

  // Humidity
  void SetInletHumidity(float humidity) { inlet_humidity_ = humidity; }
  void SetOutletHumidity(float humidity) { outlet_humidity_ = humidity; }

  // Outputs (0.0 to 1.0)
  float GetHeaterOutput() const;
  float GetFanOutput() const { return fan_output_; }
  float GetCirculatorOutput() const;

  // Air recycling control
  void SetRecyclingRate(float rate);
  float GetRecyclingRate() const;

  // Manager access
  HeatersManager* GetHeatersManager() { return &heaters_manager_; }
  PhasesManager* GetPhasesManager() { return &phases_manager_; }
  SettingsManager* GetSettingsManager() { return &settings_manager_; }
  AirRecyclingManager* GetAirRecyclingManager() { return &air_recycling_manager_; }

  // Duty time tracking
  uint32_t GetTotalDutyTime() const { return total_duty_time_s_; }

  // Settings persistence
  void SaveSettings();
  void LoadSettings();

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

  // Phase parameters
  float GetInitPhaseDuration() const;
  void SetInitPhaseDuration(float value);
  float GetExtractionPhaseDuration() const;
  void SetExtractionPhaseDuration(float value);
  float GetCirculationPhaseDuration() const;
  void SetCirculationPhaseDuration(float value);

 private:
  // Components
  ElectricHeater electric_heater_;
  HydraulicHeater hydraulic_heater_;
  HeatersManager heaters_manager_;
  PhasesManager phases_manager_;
  SettingsManager settings_manager_;

  // I2C and DAC
  TwoWire i2c_bus_2_;
  GP8403 dac_;
  AirRecyclingManager air_recycling_manager_;

  // State
  bool running_;
  unsigned long start_time_;
  uint32_t total_duty_time_s_;
  unsigned long last_duty_time_save_;
  static constexpr unsigned long kDutyTimeSaveInterval = 60000;  // Save every 60s

  // Sensors
  float inlet_temperature_;
  float outlet_temperature_;
  float water_temperature_;
  float inlet_humidity_;
  float outlet_humidity_;

  // Outputs
  float fan_output_;

  // Control update intervals
  unsigned long last_heating_update_;
  static constexpr unsigned long kHeatingUpdateInterval = 1000;  // 1s

  // Control logic
  void UpdateHeatingControl();
  void UpdateVentilationControl();
  void UpdateDutyTime();
};

#endif  // DRYER_H
