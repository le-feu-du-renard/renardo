#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <Arduino.h>
#include "TimeManager.h"
#include "Display.h"
#include <CHT8305.h>

// Forward declarations
class DFRobot_GP8403;
class DallasTemperature;

/**
 * System status access for menu display
 * Provides external references to global system components
 */
class SystemStatus {
public:
  // Set component pointers (called from main)
  static void SetTimeManager(TimeManager* tm) { time_manager_ = tm; }
  static void SetDisplay(Display* disp) { display_ = disp; }
  static void SetInletSensor(CHT8305* sensor) { inlet_sensor_ = sensor; }
  static void SetOutletSensor(CHT8305* sensor) { outlet_sensor_ = sensor; }
  static void SetWaterSensor(DallasTemperature* sensor) { water_sensor_ = sensor; }
  static void SetDAC(DFRobot_GP8403* dac) { dac_ = dac; }
  static void SetSDAvailable(bool available) { sd_available_ = available; }
  static void SetSDLoggingEnabled(bool enabled) { sd_logging_enabled_ = enabled; }
  static void SetSessionMonitorReady(bool ready) { session_monitor_ready_ = ready; }

  // Status getters
  static bool IsRTCOK();
  static String GetRTCStatus();
  static String GetDateTime();
  static String GetDate();
  static String GetTime();
  static String GetOLEDStatus();
  static String GetInletSensorStatus();
  static String GetOutletSensorStatus();
  static String GetWaterSensorStatus();
  static String GetDACStatus();
  static String GetSDCardStatus();
  static String GetSDLoggingStatus();
  static String GetSessionMonitorStatus();

private:
  static TimeManager* time_manager_;
  static Display* display_;
  static CHT8305* inlet_sensor_;
  static CHT8305* outlet_sensor_;
  static DallasTemperature* water_sensor_;
  static DFRobot_GP8403* dac_;
  static bool sd_available_;
  static bool sd_logging_enabled_;
  static bool session_monitor_ready_;
};

#endif // SYSTEM_STATUS_H
