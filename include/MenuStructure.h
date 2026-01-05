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

  // Getter/Setter wrappers for phase parameters
  static float GetInitPhaseDuration(Dryer* dryer) { return dryer->GetInitPhaseDuration(); }
  static void SetInitPhaseDuration(Dryer* dryer, float value) { dryer->SetInitPhaseDuration(value); }

  static float GetExtractionPhaseDuration(Dryer* dryer) { return dryer->GetExtractionPhaseDuration(); }
  static void SetExtractionPhaseDuration(Dryer* dryer, float value) { dryer->SetExtractionPhaseDuration(value); }

  static float GetCirculationPhaseDuration(Dryer* dryer) { return dryer->GetCirculationPhaseDuration(); }
  static void SetCirculationPhaseDuration(Dryer* dryer, float value) { dryer->SetCirculationPhaseDuration(value); }

  // Command handlers
  static void ExitMenuCommand(MenuSystem* menu);
};

#endif  // MENU_STRUCTURE_H
