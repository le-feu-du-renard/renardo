#include "MenuStructure.h"
#include "SystemStatus.h"

// ========== Program Selection Submenu ==========
// Dynamically builds a menu of available programs

// Default values for manual mode
constexpr float MANUAL_DEFAULT_TEMP = 40.0f;
constexpr float MANUAL_DEFAULT_HR = 70.0f;

// Handler for manual mode
static void SelectManualMode(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();

  // Create manual program with default values
  const Program* program = dryer->GetProgramLoader()->CreateManualProgram(
    MANUAL_DEFAULT_TEMP,
    MANUAL_DEFAULT_HR
  );

  if (program != nullptr) {
    Serial.println("Starting manual mode...");
    Serial.print("  Temperature: ");
    Serial.print(MANUAL_DEFAULT_TEMP);
    Serial.println("°C");
    Serial.print("  Humidity max: ");
    Serial.print(MANUAL_DEFAULT_HR);
    Serial.println("%");

    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  } else {
    Serial.println("ERROR: Failed to create manual program");
  }
}

// Static handlers for program selection (one per program slot)
static void SelectProgram0(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(0);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram1(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(1);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram2(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(2);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram3(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(3);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram4(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(4);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram5(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(5);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram6(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(6);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram7(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(7);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram8(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(8);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

static void SelectProgram9(MenuSystem* menu) {
  Dryer* dryer = menu->GetDryer();
  const Program* program = dryer->GetProgramLoader()->GetProgram(9);
  if (program != nullptr) {
    dryer->GetSessionManager()->SetProgram(program);
    dryer->Start();
    menu->Hide();
  }
}

// Array of program selection handlers
static CommandMenuItem::CommandFunc program_handlers[MAX_PROGRAMS] = {
  SelectProgram0, SelectProgram1, SelectProgram2, SelectProgram3, SelectProgram4,
  SelectProgram5, SelectProgram6, SelectProgram7, SelectProgram8, SelectProgram9
};

class ProgramSelectSubmenu : public SubmenuItem {
public:
  ProgramSelectSubmenu()
    : SubmenuItem("Demarrer", nullptr, 0),
      manual_item_("Manuel", SelectManualMode),
      back_item_("Retour") {
    // Initialize program items storage
    for (uint8_t i = 0; i < MAX_PROGRAMS; i++) {
      program_item_storage_[i].item = nullptr;
    }
  }

  void OnEnter(MenuSystem* menu) override {
    // Build menu with available programs
    Dryer* dryer = menu->GetDryer();
    ProgramLoader* loader = dryer->GetProgramLoader();

    uint8_t program_count = loader->GetProgramCount();
    uint8_t item_count = 0;

    // Add manual mode first
    items_buffer_[item_count++] = &manual_item_;

    // Create menu items for each program
    for (uint8_t i = 0; i < program_count; i++) {
      const Program* program = loader->GetProgram(i);
      if (program != nullptr) {
        // Create or reuse command item
        if (program_item_storage_[i].item == nullptr) {
          program_item_storage_[i].item = new CommandMenuItem(
            program->name,
            program_handlers[i]
          );
        }
        items_buffer_[item_count++] = program_item_storage_[i].item;
      }
    }

    // Add back button
    items_buffer_[item_count++] = &back_item_;

    // Update parent class members
    items_ = items_buffer_;
    item_count_ = item_count;

    // Call parent OnEnter with title
    menu->EnterSubmenu(items_, item_count_, text_);
  }

  ~ProgramSelectSubmenu() {
    // Clean up dynamically created items
    for (uint8_t i = 0; i < MAX_PROGRAMS; i++) {
      if (program_item_storage_[i].item != nullptr) {
        delete program_item_storage_[i].item;
      }
    }
  }

private:
  struct ProgramItemStorage {
    CommandMenuItem* item;
  };

  CommandMenuItem manual_item_;
  ProgramItemStorage program_item_storage_[MAX_PROGRAMS];
  BackMenuItem back_item_;
  MenuItem* items_buffer_[MAX_PROGRAMS + 2]; // +1 for manual, +1 for back button
};

static ProgramSelectSubmenu program_select_submenu;

// ========== S1 Opérations Menu ==========
// We need to dynamically build this menu based on dryer state
// For now, we'll use a custom submenu class or rebuild logic

class DynamicOperationsSubmenu : public SubmenuItem {
public:
  DynamicOperationsSubmenu()
    : SubmenuItem("S1 Operations", nullptr, 0),
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
      // Show "Demarrer" (program selection) when stopped
      items_buffer_[count++] = &program_select_submenu;
    } else {
      // Show "Arreter" and "Redemarrer" when running
      items_buffer_[count++] = &stop_item_;
      items_buffer_[count++] = &restart_item_;
    }

    items_buffer_[count++] = &back_item_;

    // Update parent class members
    items_ = items_buffer_;
    item_count_ = count;

    // Call parent OnEnter with title
    menu->EnterSubmenu(items_, item_count_, text_);
  }

private:
  CommandMenuItem stop_item_;
  CommandMenuItem restart_item_;
  BackMenuItem back_item_;
  MenuItem* items_buffer_[4];
};

static DynamicOperationsSubmenu operations_submenu;

// ========== S2 Chauffage (Heating) Menu ==========
static BoolMenuItem heating_item_hydraulic_enabled(
  "Hydraulic",
  MenuStructure::GetHydraulicEnabled,
  MenuStructure::SetHydraulicEnabled
);

static BoolMenuItem heating_item_electric_enabled(
  "Electric",
  MenuStructure::GetElectricEnabled,
  MenuStructure::SetElectricEnabled
);

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

static NumberMenuItem heating_item_humidity_max(
  "HR max %",
  MenuStructure::GetHumidityMax,
  MenuStructure::SetHumidityMax,
  40.0f, 90.0f, 1.0f
);

static BackMenuItem heating_back("Retour");

static MenuItem* heating_items[] = {
  &heating_item_hydraulic_enabled,
  &heating_item_electric_enabled,
  &heating_item_temperature_target,
  &heating_item_humidity_max,
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

// ========== S4 Système Menu ==========

// Info getters for system status
static String GetRTCStatusInfo(Dryer* dryer) {
  return SystemStatus::GetRTCStatus();
}

static String GetDateInfo(Dryer* dryer) {
  return SystemStatus::GetDate();
}

static String GetTimeInfo(Dryer* dryer) {
  return SystemStatus::GetTime();
}

static String GetOLEDStatusInfo(Dryer* dryer) {
  return SystemStatus::GetOLEDStatus();
}

static String GetInletSensorStatusInfo(Dryer* dryer) {
  return SystemStatus::GetInletSensorStatus();
}

static String GetOutletSensorStatusInfo(Dryer* dryer) {
  return SystemStatus::GetOutletSensorStatus();
}

static String GetWaterSensorStatusInfo(Dryer* dryer) {
  return SystemStatus::GetWaterSensorStatus();
}

static String GetDACStatusInfo(Dryer* dryer) {
  return SystemStatus::GetDACStatus();
}

static String GetSDCardStatusInfo(Dryer* dryer) {
  return SystemStatus::GetSDCardStatus();
}

static String GetSessionMonitorStatusInfo(Dryer* dryer) {
  return SystemStatus::GetSessionMonitorStatus();
}

static InfoMenuItem system_rtc_status("RTC", GetRTCStatusInfo);
static InfoMenuItem system_date("Date", GetDateInfo);
static InfoMenuItem system_time("Heure", GetTimeInfo);
static InfoMenuItem system_oled_status("OLED", GetOLEDStatusInfo);
static InfoMenuItem system_inlet_status("Sensor inlet", GetInletSensorStatusInfo);
static InfoMenuItem system_outlet_status("Sensor outlet", GetOutletSensorStatusInfo);
static InfoMenuItem system_water_status("Sensor water", GetWaterSensorStatusInfo);
static InfoMenuItem system_dac_status("DAC", GetDACStatusInfo);
static InfoMenuItem system_sd_status("SD card", GetSDCardStatusInfo);
static InfoMenuItem system_monitor_status("Session mon", GetSessionMonitorStatusInfo);
static BackMenuItem system_back("Retour");

static MenuItem* system_items[] = {
  &system_rtc_status,
  &system_date,
  &system_time,
  &system_oled_status,
  &system_inlet_status,
  &system_outlet_status,
  &system_water_status,
  &system_dac_status,
  &system_sd_status,
  &system_monitor_status,
  &system_back
};

static SubmenuItem system_submenu(
  "S4 Systeme",
  system_items,
  sizeof(system_items) / sizeof(system_items[0])
);

// ========== Root Menu ==========
static CommandMenuItem exit_command("Quitter", MenuStructure::ExitMenuCommand);

static MenuItem* root_items[] = {
  &operations_submenu,
  &heating_submenu,
  &cycle_submenu,
  &system_submenu,
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
