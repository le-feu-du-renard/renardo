#include "ProgramLoader.h"

ProgramLoader::ProgramLoader()
    : program_count_(0),
      fs_available_(false) {
  memset(programs_, 0, sizeof(programs_));
}

bool ProgramLoader::Begin() {
  Serial.println("ProgramLoader: Initializing LittleFS...");

  if (!LittleFS.begin()) {
    Serial.println("ProgramLoader: LittleFS mount failed!");
    fs_available_ = false;
    return false;
  }

  fs_available_ = true;
  Serial.println("ProgramLoader: LittleFS mounted successfully");

  // List files in /programs directory
  File root = LittleFS.open("/programs", "r");
  if (!root || !root.isDirectory()) {
    Serial.println("ProgramLoader: /programs directory not found");
    return true;  // Not an error, just no programs
  }
  root.close();

  return true;
}

uint8_t ProgramLoader::LoadPrograms() {
  if (!fs_available_) {
    Serial.println("ProgramLoader: Filesystem not available");
    return 0;
  }

  program_count_ = 0;

  File root = LittleFS.open("/programs", "r");
  if (!root || !root.isDirectory()) {
    Serial.println("ProgramLoader: Cannot open /programs directory");
    return 0;
  }

  File file = root.openNextFile();
  while (file && program_count_ < MAX_PROGRAMS) {
    String filename = file.name();

    // Only process .json files
    if (filename.endsWith(".json")) {
      String fullPath = "/programs/" + filename;
      Serial.print("ProgramLoader: Loading ");
      Serial.println(fullPath);

      if (LoadProgramFromFile(fullPath.c_str(), programs_[program_count_])) {
        Serial.print("ProgramLoader: Loaded program '");
        Serial.print(programs_[program_count_].name);
        Serial.print("' (ID: ");
        Serial.print(programs_[program_count_].id);
        Serial.println(")");
        program_count_++;
      } else {
        Serial.print("ProgramLoader: Failed to load ");
        Serial.println(fullPath);
      }
    }

    file.close();
    file = root.openNextFile();
  }

  root.close();

  Serial.print("ProgramLoader: Loaded ");
  Serial.print(program_count_);
  Serial.println(" programs");

  return program_count_;
}

bool ProgramLoader::LoadProgramFromFile(const char* path, Program& program) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.print("ProgramLoader: Cannot open file: ");
    Serial.println(path);
    return false;
  }

  // ArduinoJson v7 document
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("ProgramLoader: JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  // Clear program structure
  memset(&program, 0, sizeof(Program));

  // Parse program metadata
  program.id = doc["id"] | 0;
  if (program.id == 0) {
    Serial.println("ProgramLoader: Invalid program ID");
    return false;
  }

  const char* name = doc["name"] | "Unknown";
  strncpy(program.name, name, sizeof(program.name) - 1);
  program.name[sizeof(program.name) - 1] = '\0';

  // Parse phases
  JsonArray phases = doc["phases"];
  program.phase_count = 0;

  for (JsonObject phase : phases) {
    if (program.phase_count >= MAX_PHASES_PER_PROGRAM) {
      Serial.println("ProgramLoader: Too many phases, truncating");
      break;
    }

    if (ParsePhase(phase, program.phases[program.phase_count])) {
      program.phase_count++;
    }
  }

  // Parse cycles
  JsonArray cycles = doc["cycles"];
  program.cycle_count = 0;

  for (JsonObject cycle : cycles) {
    if (program.cycle_count >= MAX_CYCLES_PER_PROGRAM) {
      Serial.println("ProgramLoader: Too many cycles, truncating");
      break;
    }

    if (ParseCycle(cycle, program.cycles[program.cycle_count])) {
      program.cycle_count++;
    }
  }

  return program.phase_count > 0 && program.cycle_count > 0;
}

bool ProgramLoader::ParsePhase(JsonObject& json, Phase& phase) {
  phase.id = json["id"] | 0;
  if (phase.id == 0) {
    return false;
  }

  const char* name = json["name"] | "Phase";
  strncpy(phase.name, name, sizeof(phase.name) - 1);
  phase.name[sizeof(phase.name) - 1] = '\0';

  phase.temperature_target = json["temperature_target"] | 0.0f;
  phase.transition_on_temperature = json["transition_on_temperature"] | false;
  phase.humidity_max = json["humidity_max"] | 0.0f;
  phase.transition_on_humidity = json["transition_on_humidity"] | false;
  phase.duration_s = json["duration_s"] | 0;

  return true;
}

bool ProgramLoader::ParseCycle(JsonObject& json, Cycle& cycle) {
  cycle.id = json["id"] | 0;
  if (cycle.id == 0) {
    return false;
  }

  cycle.repeat_duration_s = json["repeat_duration_s"] | 0;

  // Parse phase_ids array
  JsonArray phase_ids = json["phase_ids"];
  cycle.phase_count = 0;

  for (JsonVariant id : phase_ids) {
    if (cycle.phase_count >= MAX_PHASES_PER_CYCLE) {
      break;
    }
    cycle.phase_ids[cycle.phase_count++] = id.as<uint8_t>();
  }

  return cycle.phase_count > 0;
}

const Program* ProgramLoader::GetProgram(uint8_t index) const {
  if (index >= program_count_) {
    return nullptr;
  }
  return &programs_[index];
}

const Program* ProgramLoader::GetProgramById(uint8_t id) const {
  for (uint8_t i = 0; i < program_count_; i++) {
    if (programs_[i].id == id) {
      return &programs_[i];
    }
  }
  return nullptr;
}

const Program* ProgramLoader::GetDefaultProgram() const {
  if (program_count_ == 0) {
    return nullptr;
  }
  return &programs_[0];
}
