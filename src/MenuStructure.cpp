#include "MenuStructure.h"

// ========== S1 Opérations Menu ==========
// We need to dynamically build this menu based on dryer state
// For now, we'll use a custom submenu class or rebuild logic

class DynamicOperationsSubmenu : public SubmenuItem {
public:
  DynamicOperationsSubmenu()
    : SubmenuItem("S1 Operations", nullptr, 0),
      start_item_("Demarrer", MenuStructure::StartDryerCommand),
      stop_item_("Arreter", MenuStructure::StopDryerCommand),
      restart_item_("Redemarrer", MenuStructure::RestartDryerCommand),
      back_item_("Retour") {}

  void OnEnter(MenuSystem* menu) override {
    // Rebuild menu items based on dryer state
    Dryer* dryer = menu->GetDryer();
    bool running = dryer->IsRunning();

    items_buffer_[0] = nullptr;
    items_buffer_[1] = nullptr;
    items_buffer_[2] = nullptr;
    items_buffer_[3] = nullptr;

    uint8_t count = 0;

    if (!running) {
      // Show only "Demarrer" when stopped
      items_buffer_[count++] = &start_item_;
    } else {
      // Show "Arreter" and "Redemarrer" when running
      items_buffer_[count++] = &stop_item_;
      items_buffer_[count++] = &restart_item_;
    }

    items_buffer_[count++] = &back_item_;

    // Update parent class members
    items_ = items_buffer_;
    item_count_ = count;

    // Call parent OnEnter
    menu->EnterSubmenu(items_, item_count_);
  }

private:
  CommandMenuItem start_item_;
  CommandMenuItem stop_item_;
  CommandMenuItem restart_item_;
  BackMenuItem back_item_;
  MenuItem* items_buffer_[4];
};

static DynamicOperationsSubmenu operations_submenu;

// ========== S2 Chauffage (Heating) Menu ==========
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
  "S2 Chauffage",
  heating_items,
  sizeof(heating_items) / sizeof(heating_items[0])
);

// ========== S3 Cycle Menu ==========
static NumberMenuItem cycle_item_recycling_rate(
  "Taux recyclage",
  MenuStructure::GetRecyclingRate,
  MenuStructure::SetRecyclingRate,
  0.0f, 100.0f, 5.0f
);

static BackMenuItem cycle_back("Retour");

static MenuItem* cycle_items[] = {
  &cycle_item_recycling_rate,
  &cycle_back
};

static SubmenuItem cycle_submenu(
  "S3 Cycle",
  cycle_items,
  sizeof(cycle_items) / sizeof(cycle_items[0])
);

// ========== Root Menu ==========
static CommandMenuItem exit_command("Quitter", MenuStructure::ExitMenuCommand);

static MenuItem* root_items[] = {
  &operations_submenu,
  &heating_submenu,
  &cycle_submenu,
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

void MenuStructure::StartDryerCommand(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  if (!dryer->IsRunning()) {
    Serial.println("Menu: Starting dryer...");
    dryer->Start();
    menu->Hide();  // Exit menu after starting
  }
}

void MenuStructure::StopDryerCommand(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  if (dryer->IsRunning()) {
    Serial.println("Menu: Stopping dryer...");
    dryer->Stop();
    menu->Hide();  // Exit menu after stopping
  }
}

void MenuStructure::RestartDryerCommand(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  if (dryer->IsRunning()) {
    Serial.println("Menu: Restarting dryer...");
    dryer->Stop();
    dryer->Start();
    menu->Hide();  // Exit menu after restarting
  }
}
