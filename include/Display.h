#ifndef DISPLAY_H
#define DISPLAY_H

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "DisplayHelpers.h"

// Include XBM icon files directly
#include "../assets/icons/electric.xbm"
#include "../assets/icons/fan.xbm"
#include "../assets/icons/recycling.xbm"
#include "../assets/icons/water.xbm"

// Include custom fonts
#include "../assets/fonts/Chicago3pt7b.h"
#include "../assets/fonts/Chicago4pt7b.h"
#include "../assets/fonts/Chicago5pt7b.h"
#include "../assets/fonts/Chicago6pt7b.h"
#include "../assets/fonts/Chicago7pt7b.h"
#include "../assets/fonts/Chicago8pt7b.h"

class Display
{
public:
  Display(Adafruit_SSD1306 *oled);

  bool Begin();
  void Update();
  void Clear();

  // Home screen data setters
  void SetTotalDutyTime(uint32_t seconds);
  void SetPhaseDutyTime(uint32_t seconds);
  void SetCycleDuration(uint32_t seconds);
  void SetPhaseDuration(uint32_t seconds);
  void SetPhase(const char *phase_name);
  void SetProgramName(const char *program_name);

  void SetInletAirData(float temperature, float humidity);
  void SetOutletAirData(float temperature, float humidity);

  void SetRecyclingRate(float percent);
  void SetVentilationState(bool state);
  void SetHydraulicHeaterPower(float percent);
  void SetWaterTemperature(float temperature);
  void SetElectricHeaterPower(bool state);
  void SetHydraulicHeaterEnabled(bool enabled);
  void SetElectricHeaterEnabled(bool enabled);

  // Menu rendering
  void ClearMenuArea();
  void DrawMenuHeader(const char *title);
  void DrawMenuLine(uint8_t line_index, const char *text, bool selected);
  void UpdateMenuArea();

private:
  Adafruit_SSD1306 *oled_;

  // Display data - Home page
  uint32_t total_duty_time_;
  uint32_t phase_duty_time_;
  uint32_t cycle_duration_;
  uint32_t phase_duration_;
  String phase_name_;
  String program_name_;

  float inlet_air_temperature_;
  float inlet_air_humidity_;
  float outlet_air_temperature_;
  float outlet_air_humidity_;

  float recycling_rate_;
  bool ventilation_state_;
  float hydraulic_heater_power_;
  float water_temperature_;
  bool electric_heater_state_;
  bool hydraulic_heater_enabled_;
  bool electric_heater_enabled_;

  void PrintAlignedRight(int16_t x, int16_t y, const char *text);
  void RenderHomePage();
};

#endif // DISPLAY_H
