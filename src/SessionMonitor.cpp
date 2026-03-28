#include "SessionMonitor.h"

SessionMonitor::SessionMonitor(Dryer *dryer, TimeManager *time_manager)
    : dryer_(dryer),
      time_manager_(time_manager),
      sd_initialized_(false),
      logging_active_(false),
      current_filename_(""),
      last_log_time_(0),
      log_interval_(DATA_LOG_INTERVAL),
      consecutive_failures_(0),
      last_init_attempt_(0)
{
}

bool SessionMonitor::Begin()
{
  Logger::Info("SessionMonitor::Begin() - START");

  // Configure CS pin
  pinMode(SD_CARD_CS_PIN, OUTPUT);
  digitalWrite(SD_CARD_CS_PIN, HIGH);

  // Configure SPI pins explicitly
  Logger::Info("Configuring SPI pins (MISO=%d, MOSI=%d, SCK=%d)",
               SD_CARD_MISO_PIN, SD_CARD_MOSI_PIN, SD_CARD_SCK_PIN);

  SPI.setRX(SD_CARD_MISO_PIN);
  SPI.setTX(SD_CARD_MOSI_PIN);
  SPI.setSCK(SD_CARD_SCK_PIN);
  SPI.begin();

  Logger::Info("Attempting SD.begin() with CS pin %d", SD_CARD_CS_PIN);

  bool sd_init_result = SD.begin(SD_CARD_CS_PIN);

  if (!sd_init_result)
  {
    Logger::Error("SD card initialization failed!");
    Logger::Warning("Possible causes: no SD card, wrong wiring (MISO=%d MOSI=%d SCK=%d CS=%d), not FAT16/FAT32",
                    SD_CARD_MISO_PIN, SD_CARD_MOSI_PIN, SD_CARD_SCK_PIN, SD_CARD_CS_PIN);
    Logger::Warning("Data logging will be disabled.");
    sd_initialized_ = false;
    return false;
  }

  Logger::Info("SD card initialized successfully");

  // Try to open root directory to verify SD is working
  SDFile root = SD.open("/");
  if (!root)
  {
    Logger::Warning("Cannot open root directory");
    sd_initialized_ = false;
    return false;
  }

  Logger::Info("Root directory accessible");

  // List existing files
  Logger::Info("Listing files on SD card:");
  SDFile entry = root.openNextFile();
  int file_count = 0;
  while (entry)
  {
    Logger::Info("  - %s (%u bytes)", entry.name(), (unsigned)entry.size());
    entry.close();
    entry = root.openNextFile();
    file_count++;
  }
  root.close();

  if (file_count == 0)
  {
    Logger::Info("  (no files found - SD card is empty)");
  }

  // Try to create a test file
  Logger::Info("Testing write access with test.txt...");
  SDFile testFile = SD.open("test.txt", FILE_WRITE);
  if (!testFile)
  {
    Logger::Error("Cannot create test file - SD may be read-only or full");
    sd_initialized_ = false;
    return false;
  }
  testFile.println("Test write");
  testFile.close();
  Logger::Info("Test file created successfully");

  sd_initialized_ = true;
  return true;
}

bool SessionMonitor::StartSession()
{
  if (!sd_initialized_)
  {
    Logger::Error("Cannot start session - SD card not initialized");
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
  bool use_root_fallback = false;

  if (!SD.exists(sessions_dir.c_str()))
  {
    Logger::Info("Creating directory: %s", sessions_dir.c_str());

    if (!SD.mkdir(sessions_dir.c_str()))
    {
      // Check if it failed because directory already exists (race condition)
      if (SD.exists(sessions_dir.c_str()))
      {
        Logger::Info("Sessions directory already exists");
      }
      else
      {
        Logger::Error("Failed to create sessions directory (SD full, write-protected, or corrupted)");
        Logger::Warning("Will use root directory as fallback");
        use_root_fallback = true;
      }
    }
    else
    {
      Logger::Info("Sessions directory created successfully");
    }
  }
  else
  {
    Logger::Info("Sessions directory already exists");
  }

  if (!use_root_fallback && !SD.exists(year_dir.c_str()))
  {
    Logger::Info("Creating directory: %s", year_dir.c_str());
    if (!SD.mkdir(year_dir.c_str()))
    {
      if (SD.exists(year_dir.c_str()))
      {
        Logger::Info("Year directory already exists");
      }
      else
      {
        Logger::Error("Failed to create year directory");
        use_root_fallback = true;
      }
    }
  }
  else if (!use_root_fallback)
  {
    Logger::Info("Year directory already exists");
  }

  if (!use_root_fallback && !SD.exists(month_dir.c_str()))
  {
    Logger::Info("Creating directory: %s", month_dir.c_str());
    if (!SD.mkdir(month_dir.c_str()))
    {
      if (SD.exists(month_dir.c_str()))
      {
        Logger::Info("Month directory already exists");
      }
      else
      {
        Logger::Error("Failed to create month directory");
        use_root_fallback = true;
      }
    }
  }
  else if (!use_root_fallback)
  {
    Logger::Info("Month directory already exists");
  }

  // Scan files in appropriate directory to find the highest batch number
  int max_batch_number = 0;

  if (!use_root_fallback)
  {
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
  }
  else
  {
    // Scan root directory for fallback files
    SDFile root = SD.open("/");
    if (root)
    {
      SDFile entry = root.openNextFile();
      while (entry)
      {
        String filename = String(entry.name());
        entry.close();

        if (filename.length() == 12 && filename.endsWith(".csv"))
        {
          String batch_str = filename.substring(4, 8);
          int batch_num = batch_str.toInt();
          if (batch_num > max_batch_number)
          {
            max_batch_number = batch_num;
          }
        }
        entry = root.openNextFile();
      }
      root.close();
    }
  }

  // Next batch number is max + 1
  int batch_number = max_batch_number + 1;

  // Format batch number with 4 digits (0001, 0002, etc.)
  char batch_str[5];
  snprintf(batch_str, sizeof(batch_str), "%04d", batch_number);
  String filename = year_month + String(batch_str) + ".csv";

  if (use_root_fallback)
  {
    current_filename_ = "/" + filename;
    Logger::Warning("Using root directory, filename: %s", current_filename_.c_str());
  }
  else
  {
    current_filename_ = month_dir + "/" + filename;
  }

  Logger::Info("Creating log file: %s (batch #%d, timestamp %s)",
               current_filename_.c_str(), batch_number, timestamp.c_str());

  // Create and open file
  SDFile file = SD.open(current_filename_.c_str(), FILE_WRITE);
  if (!file)
  {
    Logger::Error("Failed to create log file: %s", current_filename_.c_str());

    // List root directory for debug
    SDFile root = SD.open("/");
    if (root)
    {
      Logger::Info("Root directory contents:");
      SDFile entry = root.openNextFile();
      while (entry)
      {
        Logger::Info("  - %s", entry.name());
        entry.close();
        entry = root.openNextFile();
      }
      root.close();
    }

    return false;
  }

  Logger::Info("File opened successfully, writing header...");

  // Write header
  WriteHeader(file);

  file.close();
  Logger::Info("Log file created with header");

  logging_active_ = true;
  last_log_time_ = millis();

  return true;
}

void SessionMonitor::StopSession()
{
  if (logging_active_)
  {
    Logger::Info("Stopping data logging session");
    logging_active_ = false;
    current_filename_ = "";
  }
}

void SessionMonitor::Update()
{
  // Debug: Log state every minute
  static unsigned long last_debug = 0;
  unsigned long now = millis();
  if (now - last_debug >= 60000)
  {
    last_debug = now;
    Logger::Info("Status: SD=%s, Logging=%s",
                 sd_initialized_ ? "OK" : "NOT_INIT",
                 logging_active_ ? "ACTIVE" : "INACTIVE");
  }

  if (!logging_active_ || !sd_initialized_)
  {
    return;
  }

  // Check if it's time to log
  if (now - last_log_time_ >= log_interval_)
  {
    last_log_time_ = now;

    unsigned long write_start = millis();

    SDFile file = SD.open(current_filename_.c_str(), FILE_WRITE);
    if (file)
    {
      String log_data = GetDataRowString();
      file.println(log_data);
      file.close();

      unsigned long write_time = millis() - write_start;
      Logger::Info("Log written in %lums", write_time);

      // Reset consecutive failures on success
      consecutive_failures_ = 0;
    }
    else
    {
      consecutive_failures_++;
      Logger::Error("Failed to open log file (consecutive failures: %d/%d)",
                    consecutive_failures_, MAX_CONSECUTIVE_FAILURES);

      // Disable logging after too many failures
      if (consecutive_failures_ >= MAX_CONSECUTIVE_FAILURES)
      {
        Logger::Error("Too many failures - disabling SD logging");
        logging_active_ = false;
        sd_initialized_ = false;
      }
    }
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
  file.print("air_damper_open,");
  file.print("target_temperature,");
  file.print("phase_name,");
  file.print("total_elapsed_s,");
  file.println("phase_elapsed_s");
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

  // Air damper state (open=1 / closed=0)
  row += String(dryer_->GetHumidityManager()->IsHumidityTargetReached() ? 0 : 1);
  row += ",";

  // Target temperature
  row += String(dryer_->GetTargetTemperature(), 2);
  row += ",";

  // Phase name
  row += dryer_->GetPhaseName();
  row += ",";

  // Elapsed times
  row += String(dryer_->GetTotalElapsedTime());
  row += ",";
  row += String(dryer_->GetPhaseElapsedTime());

  return row;
}

bool SessionMonitor::RetryInitialization()
{
  unsigned long now = millis();

  // Don't retry too frequently
  if (now - last_init_attempt_ < INIT_RETRY_INTERVAL)
  {
    return false;
  }

  last_init_attempt_ = now;

  Logger::Info("Attempting to reinitialize SD card...");

  // Reset consecutive failures
  consecutive_failures_ = 0;

  // Try to initialize SD card
  return Begin();
}
