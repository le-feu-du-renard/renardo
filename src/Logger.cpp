#include "Logger.h"
#include <SD.h>

// Static member initialization
bool Logger::sd_logging_enabled = false;
void *Logger::sd_file = nullptr;

void Logger::Init(int level, bool enable_sd)
{
  // Initialize ArduinoLog
  Log.begin(level, &Serial);

  // Configure log format
  // Format: [LEVEL] message
  Log.setPrefix([](Print *_logOutput, int logLevel)
                {
    const char* levelStr;
    switch (logLevel) {
      case LOG_LEVEL_FATAL:   levelStr = "FATAL"; break;
      case LOG_LEVEL_ERROR:   levelStr = "ERROR"; break;
      case LOG_LEVEL_WARNING: levelStr = "WARN "; break;
      case LOG_LEVEL_NOTICE:  levelStr = "INFO "; break;
      case LOG_LEVEL_TRACE:   levelStr = "TRACE"; break;
      case LOG_LEVEL_VERBOSE: levelStr = "DEBUG"; break;
      default:                levelStr = "?????"; break;
    }
    _logOutput->print("[");
    _logOutput->print(levelStr);
    _logOutput->print("] "); });

  Log.setSuffix([](Print *_logOutput, int logLevel)
                { _logOutput->print('\n'); });

  sd_logging_enabled = enable_sd;

  Log.notice("Logger initialized (level=%d, SD=%T)", level, enable_sd);
}

void Logger::SetLevel(int level)
{
  Log.setLevel(level);
  Log.notice("Log level changed to %d", level);
}

void Logger::EnableSDLogging(bool enable)
{
  if (enable && !sd_logging_enabled)
  {
    Log.notice("SD logging enabled");
  }
  else if (!enable && sd_logging_enabled)
  {
    CloseSDLogFile();
    Log.notice("SD logging disabled");
  }
  sd_logging_enabled = enable;
}

bool Logger::IsSDLoggingEnabled()
{
  return sd_logging_enabled;
}

bool Logger::SetSDLogFile(const char *filename)
{
  if (!sd_logging_enabled)
  {
    Log.warning("Cannot open SD log file: SD logging is disabled");
    return false;
  }

  // Close existing file if open
  CloseSDLogFile();

  // Open new file in append mode
  File *file = new File(SD.open(filename, FILE_WRITE));
  if (!file || !(*file))
  {
    Log.error("Failed to open SD log file: %s", filename);
    delete file;
    return false;
  }

  sd_file = file;
  Log.notice("SD log file opened: %s", filename);
  return true;
}

void Logger::CloseSDLogFile()
{
  if (sd_file != nullptr)
  {
    File *file = static_cast<File *>(sd_file);
    file->close();
    delete file;
    sd_file = nullptr;
    Log.notice("SD log file closed");
  }
}

void Logger::Flush()
{
  Serial.flush();

  if (sd_logging_enabled && sd_file != nullptr)
  {
    File *file = static_cast<File *>(sd_file);
    file->flush();
  }
}

// Note: For dual output (Serial + SD), you would need to create a custom
// Print class that writes to both. This is left as a future enhancement.
// For now, logs only go to Serial. SD logging would require hooking into
// the ArduinoLog output system or creating a custom Print wrapper.
