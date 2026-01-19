#include "DataLogger.h"

DataLogger::DataLogger(Dryer *dryer, TimeManager *time_manager)
    : dryer_(dryer),
      time_manager_(time_manager),
      sd_initialized_(false),
      logging_active_(false),
      current_filename_(""),
      last_log_time_(0),
      log_interval_(DATA_LOG_INTERVAL)
{
}

bool DataLogger::Begin()
{
  Serial.println("DataLogger::Begin() - START");
  Serial.flush();

  // Configure CS pin
  pinMode(SD_CARD_CS_PIN, OUTPUT);
  digitalWrite(SD_CARD_CS_PIN, HIGH);

  // Configure SPI pins explicitly
  Serial.print("Configuring SPI pins (MISO=");
  Serial.print(SD_CARD_MISO_PIN);
  Serial.print(", MOSI=");
  Serial.print(SD_CARD_MOSI_PIN);
  Serial.print(", SCK=");
  Serial.print(SD_CARD_SCK_PIN);
  Serial.println(")");

  SPI.setRX(SD_CARD_MISO_PIN);
  SPI.setTX(SD_CARD_MOSI_PIN);
  SPI.setSCK(SD_CARD_SCK_PIN);
  SPI.begin();

  Serial.print("Attempting SD.begin() with CS pin ");
  Serial.println(SD_CARD_CS_PIN);
  Serial.flush();

  bool sd_init_result = SD.begin(SD_CARD_CS_PIN);

  Serial.println("SD.begin() returned");
  Serial.flush();

  if (!sd_init_result)
  {
    Serial.println("ERROR: SD card initialization failed!");
    Serial.println("Possible causes:");
    Serial.println("  - No SD card inserted");
    Serial.println("  - Wrong wiring (MISO=GPIO16, MOSI=GPIO19, SCK=GPIO18, CS=GPIO17)");
    Serial.println("  - Card not formatted as FAT16/FAT32");
    Serial.println("Data logging will be disabled.");
    sd_initialized_ = false;
    return false;
  }

  Serial.println("SUCCESS: SD card initialized!");

  // Try to open root directory to verify SD is working
  SDFile root = SD.open("/");
  if (!root)
  {
    Serial.println("WARNING: Cannot open root directory!");
    sd_initialized_ = false;
    return false;
  }

  Serial.println("Root directory accessible");

  // List existing files
  Serial.println("Listing files on SD card:");
  SDFile entry = root.openNextFile();
  int file_count = 0;
  while (entry)
  {
    Serial.print("  - ");
    Serial.print(entry.name());
    Serial.print(" (");
    Serial.print(entry.size());
    Serial.println(" bytes)");
    entry.close();
    entry = root.openNextFile();
    file_count++;
  }
  root.close();

  if (file_count == 0)
  {
    Serial.println("  (no files found - SD card is empty)");
  }

  // Try to create a test file
  Serial.println("Testing write access with test.txt...");
  SDFile testFile = SD.open("test.txt", FILE_WRITE);
  if (!testFile)
  {
    Serial.println("ERROR: Cannot create test file - SD may be read-only or full");
    sd_initialized_ = false;
    return false;
  }
  testFile.println("Test write");
  testFile.close();
  Serial.println("Test file created successfully!");

  sd_initialized_ = true;
  return true;
}

bool DataLogger::StartSession()
{
  if (!sd_initialized_)
  {
    Serial.println("ERROR: Cannot start session - SD card not initialized");
    return false;
  }

  // Generate filename with counter and timestamp
  // Format: X_YYMM.CSV where X is sequential counter, YY=year, MM=month
  String timestamp = time_manager_->GetTimestampFilename();

  // Extract year and month from timestamp (YYYYMMDD_HHMMSS)
  String year_month = timestamp.substring(2, 4) + timestamp.substring(4, 6); // YYMM

  // Find next available counter
  int counter = 1;
  String test_filename;
  while (counter < 1000) // Max 999 sessions
  {
    test_filename = String(counter) + "_" + year_month + ".csv";

    // Check if file exists
    SDFile test_file = SD.open(test_filename.c_str(), FILE_READ);
    if (!test_file)
    {
      // File doesn't exist, we can use this name
      break;
    }
    test_file.close();
    counter++;
  }

  current_filename_ = test_filename;

  Serial.print("Creating log file: ");
  Serial.println(current_filename_);
  Serial.print("Session #");
  Serial.print(counter);
  Serial.print(" - Full timestamp: ");
  Serial.println(timestamp);

  // Create and open file
  SDFile file = SD.open(current_filename_.c_str(), FILE_WRITE);
  if (!file)
  {
    Serial.println("ERROR: Failed to create log file!");
    Serial.println("Trying to list root directory contents:");

    // Try to list files to debug
    SDFile root = SD.open("/");
    if (root)
    {
      SDFile entry = root.openNextFile();
      while (entry)
      {
        Serial.print("  - ");
        Serial.println(entry.name());
        entry.close();
        entry = root.openNextFile();
      }
      root.close();
    }

    return false;
  }

  Serial.println("File opened successfully, writing header...");

  // Write header
  WriteHeader(file);

  file.close();
  Serial.println("SUCCESS: Log file created with header");

  logging_active_ = true;
  last_log_time_ = millis();

  return true;
}

void DataLogger::StopSession()
{
  if (logging_active_)
  {
    Serial.println("Stopping data logging session");
    logging_active_ = false;
    current_filename_ = "";
  }
}

void DataLogger::Update()
{
  if (!logging_active_ || !sd_initialized_)
  {
    return;
  }

  unsigned long now = millis();

  // Check if it's time to log
  if (now - last_log_time_ >= log_interval_)
  {
    last_log_time_ = now;

    // Open file in append mode
    SDFile file = SD.open(current_filename_.c_str(), FILE_WRITE);
    if (!file)
    {
      Serial.println("ERROR: Failed to open log file for writing!");
      return;
    }

    // Write data row
    WriteDataRow(file);
    file.close();

    Serial.println("Data logged to SD card");
  }
}

void DataLogger::WriteHeader(SDFile &file)
{
  file.print("timestamp,");
  file.print("inlet_temperature,");
  file.print("inlet_hr,");
  file.print("outlet_temperature,");
  file.print("outlet_hr,");
  file.print("fan_state,");
  file.print("hydraulic_heater_state,");
  file.print("hydraulic_heater_power,");
  file.print("electric_heater_state,");
  file.print("recycling_rate,");
  file.print("target_temperature,");
  file.print("target_hr,");
  file.print("program_id,");
  file.print("cycle_id,");
  file.print("phase_id,");
  file.println("phase_name");
}

void DataLogger::WriteDataRow(SDFile &file)
{
  String dataRow = GetDataRowString();
  file.println(dataRow);
}

String DataLogger::GetDataRowString()
{
  String row = "";

  // Timestamp (human-readable)
  row += time_manager_->GetDateTimeString();
  row += ",";

  // Inlet temperature and humidity
  row += String(dryer_->GetInletTemperature(), 2);
  row += ",";
  row += String(dryer_->GetInletHumidity(), 2);
  row += ",";

  // Outlet temperature and humidity
  row += String(dryer_->GetOutletTemperature(), 2);
  row += ",";
  row += String(dryer_->GetOutletHumidity(), 2);
  row += ",";

  // Fan state (0 or 1)
  row += String(dryer_->GetFanOutput() > 0.0 ? 1 : 0);
  row += ",";

  // Hydraulic heater state and power
  float hydraulic_power = dryer_->GetTemperatureManager()->GetHydraulicHeater()->GetPower();
  row += String(hydraulic_power > 0.0 ? 1 : 0);
  row += ",";
  row += String(hydraulic_power, 2);
  row += ",";

  // Electric heater state
  row += String(dryer_->GetHeaterOutput() > 0.5 ? 1 : 0);
  row += ",";

  // Recycling rate
  row += String(dryer_->GetRecyclingRate(), 2);
  row += ",";

  // Target temperature
  row += String(dryer_->GetTargetTemperature(), 2);
  row += ",";

  // Target humidity (from humidity manager)
  row += String(dryer_->GetHumidityManager()->GetTargetHumidity(), 2);
  row += ",";

  // Program information
  const Program *program = dryer_->GetSessionManager()->GetProgram();
  if (program != nullptr)
  {
    row += String(program->id);
  }
  else
  {
    row += "0";
  }
  row += ",";

  // Cycle ID
  uint8_t cycle_index = dryer_->GetSessionManager()->GetCurrentCycleIndex();
  if (program != nullptr && cycle_index < program->cycle_count)
  {
    const Cycle *cycle = &program->cycles[cycle_index];
    row += String(cycle->id);
  }
  else
  {
    row += "0";
  }
  row += ",";

  // Phase ID and name
  uint8_t phase_id = dryer_->GetCurrentPhaseId();
  row += String(phase_id);
  row += ",";
  row += dryer_->GetPhaseName();

  return row;
}
