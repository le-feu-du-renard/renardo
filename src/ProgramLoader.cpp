#include "ProgramLoader.h"

ProgramLoader::ProgramLoader()
    : program_count_(0),
      manual_program_created_(false) {
  memset(program_pointers_, 0, sizeof(program_pointers_));
  memset(&manual_program_, 0, sizeof(manual_program_));
}

bool ProgramLoader::Begin() {
  Serial.println("ProgramLoader: Initializing built-in programs...");
  return true;
}

uint8_t ProgramLoader::LoadPrograms() {
  program_count_ = 0;

  // Load built-in programs
  program_pointers_[program_count_++] = GetProgram1_Standard();
  program_pointers_[program_count_++] = GetProgram2_Doux();
  program_pointers_[program_count_++] = GetProgram3_Germination();

  Serial.print("ProgramLoader: Loaded ");
  Serial.print(program_count_);
  Serial.println(" built-in programs");

  // Print loaded programs
  for (uint8_t i = 0; i < program_count_; i++) {
    const Program* prog = program_pointers_[i];
    if (prog != nullptr) {
      Serial.print("  - Program ");
      Serial.print(prog->id);
      Serial.print(": '");
      Serial.print(prog->name);
      Serial.print("' (");
      Serial.print(prog->phase_count);
      Serial.print(" phases, ");
      Serial.print(prog->cycle_count);
      Serial.println(" cycles)");
    }
  }

  return program_count_;
}

const Program* ProgramLoader::GetProgram(uint8_t index) const {
  if (index >= program_count_) {
    return nullptr;
  }
  return program_pointers_[index];
}

const Program* ProgramLoader::GetProgramById(uint8_t id) const {
  for (uint8_t i = 0; i < program_count_; i++) {
    if (program_pointers_[i] != nullptr && program_pointers_[i]->id == id) {
      return program_pointers_[i];
    }
  }
  return nullptr;
}

const Program* ProgramLoader::GetDefaultProgram() const {
  if (program_count_ == 0) {
    return nullptr;
  }
  return program_pointers_[0];
}

const Program* ProgramLoader::CreateManualProgram(float temperature_target, float humidity_max) {
  // Clear the manual program structure
  memset(&manual_program_, 0, sizeof(Program));

  // Set program metadata
  manual_program_.id = 255;  // Special ID for manual mode
  strncpy(manual_program_.name, "Manuel", sizeof(manual_program_.name) - 1);

  // Create Phase 1: Init
  manual_program_.phases[0].id = 1;
  strncpy(manual_program_.phases[0].name, "Init", sizeof(manual_program_.phases[0].name) - 1);
  manual_program_.phases[0].temperature_target = temperature_target;
  manual_program_.phases[0].transition_on_temperature = true;
  manual_program_.phases[0].humidity_max = humidity_max;
  manual_program_.phases[0].transition_on_humidity = false;
  manual_program_.phases[0].duration_s = 3600;

  // Create Phase 2: Extraction
  manual_program_.phases[1].id = 2;
  strncpy(manual_program_.phases[1].name, "Extraction", sizeof(manual_program_.phases[1].name) - 1);
  manual_program_.phases[1].temperature_target = temperature_target;
  manual_program_.phases[1].transition_on_temperature = false;
  manual_program_.phases[1].humidity_max = -1.0f;
  manual_program_.phases[1].transition_on_humidity = false;
  manual_program_.phases[1].duration_s = 120;

  // Create Phase 3: Circulation
  manual_program_.phases[2].id = 3;
  strncpy(manual_program_.phases[2].name, "Circulation", sizeof(manual_program_.phases[2].name) - 1);
  manual_program_.phases[2].temperature_target = temperature_target;
  manual_program_.phases[2].transition_on_temperature = false;
  manual_program_.phases[2].humidity_max = humidity_max;
  manual_program_.phases[2].transition_on_humidity = true;  // Transition when HR_max reached
  manual_program_.phases[2].duration_s = 300;

  manual_program_.phase_count = 3;

  // Create Cycle 1: Init
  manual_program_.cycles[0].id = 1;
  manual_program_.cycles[0].phase_ids[0] = 1;
  manual_program_.cycles[0].phase_count = 1;
  manual_program_.cycles[0].repeat_duration_s = 0;

  // Create Cycle 2: Drying (Extraction + Circulation loop)
  manual_program_.cycles[1].id = 2;
  manual_program_.cycles[1].phase_ids[0] = 2;
  manual_program_.cycles[1].phase_ids[1] = 3;
  manual_program_.cycles[1].phase_count = 2;
  manual_program_.cycles[1].repeat_duration_s = -1;  // Infinite loop

  manual_program_.cycle_count = 2;

  manual_program_created_ = true;

  Serial.println("ProgramLoader: Manual program created");
  Serial.print("  Temperature target: ");
  Serial.println(temperature_target);
  Serial.print("  Humidity max: ");
  Serial.println(humidity_max);

  return &manual_program_;
}

const Program* ProgramLoader::GetManualProgram() const {
  if (!manual_program_created_) {
    return nullptr;
  }
  return &manual_program_;
}

void ProgramLoader::UpdateManualProgram(float temperature_target, float humidity_max) {
  if (!manual_program_created_) {
    Serial.println("ProgramLoader: Cannot update manual program - not created");
    return;
  }

  // Update all phases with new values
  manual_program_.phases[0].temperature_target = temperature_target;
  manual_program_.phases[0].humidity_max = humidity_max;
  manual_program_.phases[1].temperature_target = temperature_target;
  manual_program_.phases[2].temperature_target = temperature_target;
  manual_program_.phases[2].humidity_max = humidity_max;

  Serial.println("ProgramLoader: Manual program updated");
  Serial.print("  New temperature target: ");
  Serial.println(temperature_target);
  Serial.print("  New humidity max: ");
  Serial.println(humidity_max);
}
