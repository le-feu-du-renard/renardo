#ifndef MENU_STRUCTURE_H
#define MENU_STRUCTURE_H

#include "MenuSystem.h"
#include "Dryer.h"

/**
 * Menu structure builder
 * Defines all menu items and hierarchy
 */
class MenuStructure {
 public:
  static MenuItem* BuildMenu();

  // Getter/Setter wrappers for heating parameters
  static float GetTemperatureTarget(Dryer* dryer) { return dryer->GetTemperatureTarget(); }
  static void SetTemperatureTarget(Dryer* dryer, float value) { dryer->SetTemperatureTarget(value); }

  // PID parameters - Hydraulic
  static float GetHydraulicKp(Dryer* dryer) { return dryer->GetHydraulicKp(); }
  static void SetHydraulicKp(Dryer* dryer, float value) { dryer->SetHydraulicKp(value); }
  static float GetHydraulicKi(Dryer* dryer) { return dryer->GetHydraulicKi(); }
  static void SetHydraulicKi(Dryer* dryer, float value) { dryer->SetHydraulicKi(value); }
  static float GetHydraulicKd(Dryer* dryer) { return dryer->GetHydraulicKd(); }
  static void SetHydraulicKd(Dryer* dryer, float value) { dryer->SetHydraulicKd(value); }

  // PID parameters - Electric
  static float GetElectricKp(Dryer* dryer) { return dryer->GetElectricKp(); }
  static void SetElectricKp(Dryer* dryer, float value) { dryer->SetElectricKp(value); }
  static float GetElectricKi(Dryer* dryer) { return dryer->GetElectricKi(); }
  static void SetElectricKi(Dryer* dryer, float value) { dryer->SetElectricKi(value); }
  static float GetElectricKd(Dryer* dryer) { return dryer->GetElectricKd(); }
  static void SetElectricKd(Dryer* dryer, float value) { dryer->SetElectricKd(value); }

  // PID advanced parameters
  static float GetPidIntegralMax(Dryer* dryer) { return dryer->GetPidIntegralMax(); }
  static void SetPidIntegralMax(Dryer* dryer, float value) { dryer->SetPidIntegralMax(value); }
  static float GetPidDerivativeFilter(Dryer* dryer) { return dryer->GetPidDerivativeFilter(); }
  static void SetPidDerivativeFilter(Dryer* dryer, float value) { dryer->SetPidDerivativeFilter(value); }
  static float GetWaterTempMargin(Dryer* dryer) { return dryer->GetWaterTempMargin(); }
  static void SetWaterTempMargin(Dryer* dryer, float value) { dryer->SetWaterTempMargin(value); }

  static float GetRecyclingRate(Dryer* dryer) { return dryer->GetRecyclingRate(); }
  static void SetRecyclingRate(Dryer* dryer, float value) { dryer->SetRecyclingRate(value); }

  // Humidity parameters
  static float GetHumidityMax(Dryer* dryer) { return dryer->GetHumidityMax(); }
  static void SetHumidityMax(Dryer* dryer, float value) { dryer->SetHumidityMax(value); }

  // Energy source enable/disable
  static bool GetHydraulicEnabled(Dryer* dryer) { return dryer->GetHydraulicEnabled(); }
  static void SetHydraulicEnabled(Dryer* dryer, bool value) { dryer->SetHydraulicEnabled(value); }
  static bool GetElectricEnabled(Dryer* dryer) { return dryer->GetElectricEnabled(); }
  static void SetElectricEnabled(Dryer* dryer, bool value) { dryer->SetElectricEnabled(value); }

  // Command handlers
  static void ExitMenuCommand(MenuSystem* menu);
  static void StopDryerCommand(MenuSystem* menu);
  static void RestartDryerCommand(MenuSystem* menu);
};

#endif  // MENU_STRUCTURE_H
