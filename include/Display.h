#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "DisplayHelpers.h"
#include "DisplayIcons.h"

/**
 * Manages OLED 128x64 display
 * Implements exact ESPHome layout
 */
class Display {
 public:
  Display();

  bool Begin();
  void Update();
  void Clear();

  // Home screen data setters
  void SetTotalDutyTime(uint32_t seconds);
  void SetPhaseDutyTime(uint32_t seconds);
  void SetCycleDuration(uint32_t seconds);
  void SetPhaseDuration(uint32_t seconds);
  void SetPhase(const char* phase_name);

  void SetInletAirData(float temperature, float humidity);
  void SetOutletAirData(float temperature, float humidity);

  void SetRecyclingRate(float percent);
  void SetVentilationState(bool state);
  void SetHydraulicHeaterPower(float percent);
  void SetWaterTemperature(float temperature);
  void SetElectricHeaterPower(bool state);

  // Menu rendering
  void ClearMenuArea();
  void DrawMenuLine(uint8_t line_index, const char* text, bool selected);
  void UpdateMenuArea();

 private:
  Adafruit_SSD1306 display_;

  // Display data - Home page
  uint32_t total_duty_time_;
  uint32_t phase_duty_time_;
  uint32_t cycle_duration_;
  uint32_t phase_duration_;
  String phase_name_;

  float inlet_air_temperature_;
  float inlet_air_humidity_;
  float outlet_air_temperature_;
  float outlet_air_humidity_;

  float recycling_rate_;
  bool ventilation_state_;
  float hydraulic_heater_power_;
  float water_temperature_;
  bool electric_heater_state_;

  void RenderHomePage();
};

#endif  // DISPLAY_H
