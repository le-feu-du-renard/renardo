#include "SystemStatus.h"
#include "Logger.h"
#include <DFRobot_GP8403.h>
#include <DallasTemperature.h>

// Initialize static members
TimeManager* SystemStatus::time_manager_ = nullptr;
Display* SystemStatus::display_ = nullptr;
CHT8305* SystemStatus::inlet_sensor_ = nullptr;
CHT8305* SystemStatus::outlet_sensor_ = nullptr;
DallasTemperature* SystemStatus::water_sensor_ = nullptr;
DFRobot_GP8403* SystemStatus::dac_ = nullptr;
bool SystemStatus::sd_available_ = false;
bool SystemStatus::session_monitor_ready_ = false;

bool SystemStatus::IsRTCOK() {
  return time_manager_ != nullptr && time_manager_->IsRunning();
}

String SystemStatus::GetRTCStatus() {
  if (time_manager_ == nullptr) {
    return "N/A";
  }
  return time_manager_->IsRunning() ? "OK" : "NO";
}

String SystemStatus::GetDateTime() {
  if (time_manager_ == nullptr || !time_manager_->IsRunning()) {
    return "N/A";
  }
  DateTime now = time_manager_->GetNow();
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%02d/%02d %02d:%02d",
           now.day(), now.month(), now.hour(), now.minute());
  return String(buffer);
}

String SystemStatus::GetDate() {
  if (time_manager_ == nullptr || !time_manager_->IsRunning()) {
    return "N/A";
  }
  DateTime now = time_manager_->GetNow();
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d",
           now.day(), now.month(), now.year());
  return String(buffer);
}

String SystemStatus::GetTime() {
  if (time_manager_ == nullptr || !time_manager_->IsRunning()) {
    return "N/A";
  }
  DateTime now = time_manager_->GetNow();
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
           now.hour(), now.minute(), now.second());
  return String(buffer);
}

String SystemStatus::GetOLEDStatus() {
  // OLED is considered OK if display pointer is set
  // (we assume if Begin() failed, the pointer wouldn't be set)
  return display_ != nullptr ? "OK" : "NO";
}

String SystemStatus::GetInletSensorStatus() {
  if (inlet_sensor_ == nullptr) {
    return "N/A";
  }
  return inlet_sensor_->isConnected() ? "OK" : "NO";
}

String SystemStatus::GetOutletSensorStatus() {
  if (outlet_sensor_ == nullptr) {
    return "N/A";
  }
  return outlet_sensor_->isConnected() ? "OK" : "NO";
}

String SystemStatus::GetWaterSensorStatus() {
  if (water_sensor_ == nullptr) {
    return "N/A";
  }
  // Check if at least one DS18B20 device is found
  uint8_t device_count = water_sensor_->getDeviceCount();
  if (device_count > 0) {
    return "OK";
  }
  return "NO";
}

String SystemStatus::GetDACStatus() {
  // DAC is considered OK if pointer is set
  // (we assume if initialization failed, we still have the pointer)
  // For a more accurate check, we could add a test write/read
  return dac_ != nullptr ? "OK" : "NO";
}

String SystemStatus::GetSDCardStatus() {
  return sd_available_ ? "OK" : "NO";
}

String SystemStatus::GetSessionMonitorStatus() {
  return session_monitor_ready_ ? "OK" : "NO";
}
