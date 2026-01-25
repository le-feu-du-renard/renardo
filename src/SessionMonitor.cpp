#include "SessionMonitor.h"

SessionMonitor::SessionMonitor(Dryer *dryer, TimeManager *time_manager)
    : dryer_(dryer),
      time_manager_(time_manager),
      sd_initialized_(false),
      logging_active_(false),
      current_filename_(""),
      last_log_time_(0),
      log_interval_(DATA_LOG_INTERVAL)
{
}

bool SessionMonitor::Begin()
{
  Serial.println("SessionMonitor::Begin() - START");
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

bool SessionMonitor::StartSession()
{
  if (!sd_initialized_)
  {
    Serial.println("ERROR: Cannot start session - SD card not initialized");
    return false;
  }

  // Generate filename with batch number and timestamp
  // Format: /sessions/YYYY/MM/YYMMXXXX.csv where YYYY=year, MM=month, XXXX=sequential batch number (4 digits)
  String timestamp = time_manager_->GetTimestampFilename();

  // Extract year and month from timestamp (YYYYMMDD_HHMMSS)
  String year = timestamp.substring(0, 4);      // YYYY
  String month = timestamp.substring(4, 6);     // MM
  String year_month = timestamp.substring(2, 4) + month; // YYMM

  // Create directory structure: /sessions/YYYY/MM/
  String sessions_dir = "/sessions";
  String year_dir = sessions_dir + "/" + year;
  String month_dir = year_dir + "/" + month;

  // Create directories if they don't exist
  if (!SD.exists(sessions_dir.c_str()))
  {
    Serial.print("Creating directory: ");
    Serial.println(sessions_dir);
    if (!SD.mkdir(sessions_dir.c_str()))
    {
      Serial.println("ERROR: Failed to create sessions directory!");
      return false;
    }
  }

  if (!SD.exists(year_dir.c_str()))
  {
    Serial.print("Creating directory: ");
    Serial.println(year_dir);
    if (!SD.mkdir(year_dir.c_str()))
    {
      Serial.println("ERROR: Failed to create year directory!");
      return false;
    }
  }

  if (!SD.exists(month_dir.c_str()))
  {
    Serial.print("Creating directory: ");
    Serial.println(month_dir);
    if (!SD.mkdir(month_dir.c_str()))
    {
      Serial.println("ERROR: Failed to create month directory!");
      return false;
    }
  }

  // Scan files in month directory to find the highest batch number
  int max_batch_number = 0;
  SDFile month_folder = SD.open(month_dir.c_str());
  if (month_folder)
  {
    SDFile entry = month_folder.openNextFile();
    while (entry)
    {
      String filename = String(entry.name());
      entry.close();

      // Check if filename matches pattern: YYMMXXXX.csv (12 chars)
      if (filename.length() == 12 && filename.endsWith(".csv"))
      {
        // Extract batch number (4 digits after YYMM)
        String batch_str = filename.substring(4, 8);
        int batch_num = batch_str.toInt();
        if (batch_num > max_batch_number)
        {
          max_batch_number = batch_num;
        }
      }
      entry = month_folder.openNextFile();
    }
    month_folder.close();
  }

  // Next batch number is max + 1
  int batch_number = max_batch_number + 1;

  // Format batch number with 4 digits (0001, 0002, etc.)
  char batch_str[5];
  snprintf(batch_str, sizeof(batch_str), "%04d", batch_number);
  String filename = year_month + String(batch_str) + ".csv";

  current_filename_ = month_dir + "/" + filename;

  Serial.print("Creating log file: ");
  Serial.println(current_filename_);
  Serial.print("Batch #");
  Serial.print(batch_number);
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

void SessionMonitor::StopSession()
{
  if (logging_active_)
  {
    Serial.println("Stopping data logging session");
    logging_active_ = false;
    current_filename_ = "";
  }
}

void SessionMonitor::Update()
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

void SessionMonitor::WriteHeader(SDFile &file)
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

void SessionMonitor::WriteDataRow(SDFile &file)
{
  String dataRow = GetDataRowString();
  file.println(dataRow);
}

String SessionMonitor::GetDataRowString()
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
