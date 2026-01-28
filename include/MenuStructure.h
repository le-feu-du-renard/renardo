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

  static float GetTemperatureDeadband(Dryer* dryer) { return dryer->GetTemperatureDeadband(); }
  static void SetTemperatureDeadband(Dryer* dryer, float value) { dryer->SetTemperatureDeadband(value); }

  static float GetHeatingActionMinWait(Dryer* dryer) { return dryer->GetHeatingActionMinWait(); }
  static void SetHeatingActionMinWait(Dryer* dryer, float value) { dryer->SetHeatingActionMinWait(value); }

  static float GetHeaterStepMin(Dryer* dryer) { return dryer->GetHeaterStepMin(); }
  static void SetHeaterStepMin(Dryer* dryer, float value) { dryer->SetHeaterStepMin(value); }

  static float GetHeaterStepMax(Dryer* dryer) { return dryer->GetHeaterStepMax(); }
  static void SetHeaterStepMax(Dryer* dryer, float value) { dryer->SetHeaterStepMax(value); }

  static float GetHeaterFullScaleDelta(Dryer* dryer) { return dryer->GetHeaterFullScaleDelta(); }
  static void SetHeaterFullScaleDelta(Dryer* dryer, float value) { dryer->SetHeaterFullScaleDelta(value); }

  static float GetRecyclingRate(Dryer* dryer) { return dryer->GetRecyclingRate(); }
  static void SetRecyclingRate(Dryer* dryer, float value) { dryer->SetRecyclingRate(value); }

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
