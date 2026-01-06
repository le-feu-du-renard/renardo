#include "MenuStructure.h"

// ========== S1 Chauffage (Heating) Menu ==========
static NumberMenuItem heating_item_temperature_target(
  "T cible",
  MenuStructure::GetTemperatureTarget,
  MenuStructure::SetTemperatureTarget,
  20.0f, 45.0f, 0.5f
);

static NumberMenuItem heating_item_min_wait(
  "Min wait s",
  MenuStructure::GetHeatingActionMinWait,
  MenuStructure::SetHeatingActionMinWait,
  1.0f, 120.0f, 1.0f
);

static NumberMenuItem heating_item_deadband(
  "Deadband",
  MenuStructure::GetTemperatureDeadband,
  MenuStructure::SetTemperatureDeadband,
  0.0f, 2.0f, 0.1f
);

static NumberMenuItem heating_item_step_min(
  "Step min",
  MenuStructure::GetHeaterStepMin,
  MenuStructure::SetHeaterStepMin,
  1.0f, 10.0f, 0.5f
);

static NumberMenuItem heating_item_step_max(
  "Step max",
  MenuStructure::GetHeaterStepMax,
  MenuStructure::SetHeaterStepMax,
  2.0f, 10.0f, 0.5f
);

static NumberMenuItem heating_item_full_scale(
  "Full scale",
  MenuStructure::GetHeaterFullScaleDelta,
  MenuStructure::SetHeaterFullScaleDelta,
  10.0f, 30.0f, 1.0f
);

static BackMenuItem heating_back("Retour");

static MenuItem* heating_items[] = {
  &heating_item_temperature_target,
  &heating_item_min_wait,
  &heating_item_deadband,
  &heating_item_step_min,
  &heating_item_step_max,
  &heating_item_full_scale,
  &heating_back
};

static SubmenuItem heating_submenu(
  "S1 Chauffage",
  heating_items,
  sizeof(heating_items) / sizeof(heating_items[0])
);

// ========== S2 Cycle Menu ==========
static NumberMenuItem cycle_item_recycling_rate(
  "Taux recyclage",
  MenuStructure::GetRecyclingRate,
  MenuStructure::SetRecyclingRate,
  0.0f, 100.0f, 5.0f
);

static NumberMenuItem cycle_item_session_duration(
  "Duree session",
  MenuStructure::GetDryingSessionDuration,
  MenuStructure::SetDryingSessionDuration,
  3600.0f, 604800.0f, 3600.0f
);

static BackMenuItem cycle_back("Retour");

static MenuItem* cycle_items[] = {
  &cycle_item_recycling_rate,
  &cycle_item_session_duration,
  &cycle_back
};

static SubmenuItem cycle_submenu(
  "S2 Cycle",
  cycle_items,
  sizeof(cycle_items) / sizeof(cycle_items[0])
);

// ========== S3 Phases Menu ==========
static NumberMenuItem phases_item_init(
  "Init",
  MenuStructure::GetInitPhaseDuration,
  MenuStructure::SetInitPhaseDuration,
  5.0f, 7200.0f, 60.0f
);

static NumberMenuItem phases_item_extraction(
  "Extraction",
  MenuStructure::GetExtractionPhaseDuration,
  MenuStructure::SetExtractionPhaseDuration,
  5.0f, 7200.0f, 10.0f
);

static NumberMenuItem phases_item_circulation(
  "Circulation",
  MenuStructure::GetCirculationPhaseDuration,
  MenuStructure::SetCirculationPhaseDuration,
  5.0f, 3600.0f, 10.0f
);

static BackMenuItem phases_back("Retour");

static MenuItem* phases_items[] = {
  &phases_item_init,
  &phases_item_extraction,
  &phases_item_circulation,
  &phases_back
};

static SubmenuItem phases_submenu(
  "S3 Phases",
  phases_items,
  sizeof(phases_items) / sizeof(phases_items[0])
);

// ========== Root Menu ==========
static CommandMenuItem exit_command("Quitter", MenuStructure::ExitMenuCommand);

static MenuItem* root_items[] = {
  &heating_submenu,
  &cycle_submenu,
  &phases_submenu,
  &exit_command
};

static SubmenuItem root_menu(
  "Menu Principal",
  root_items,
  sizeof(root_items) / sizeof(root_items[0])
);

// ========== Menu Builder ==========

MenuItem* MenuStructure::BuildMenu() {
  return &root_menu;
}

void MenuStructure::ExitMenuCommand(MenuSystem* menu) {
  menu->Hide();
}
