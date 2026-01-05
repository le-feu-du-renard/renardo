#include "Display.h"
#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Display::Display()
  : display_(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET),
    total_duty_time_(0),
    phase_duty_time_(0),
    cycle_duration_(0),
    phase_duration_(0),
    phase_name_("STOP"),
    inlet_air_temperature_(NAN),
    inlet_air_humidity_(NAN),
    outlet_air_temperature_(NAN),
    outlet_air_humidity_(NAN),
    recycling_rate_(0.0),
    ventilation_state_(false),
    hydraulic_heater_power_(0.0),
    water_temperature_(NAN),
    electric_heater_state_(false) {
}

bool Display::Begin() {
  if (!display_.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    return false;
  }

  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.setCursor(0, 0);
  display_.println("Dryer Initializing...");
  display_.display();

  Serial.println("Display initialized");
  return true;
}

void Display::Clear() {
  display_.clearDisplay();
}

void Display::Update() {
  RenderHomePage();
}

void Display::RenderHomePage() {
  display_.clearDisplay();

  // Layout constants (from ESPHome)
  const uint32_t padding = 1U;
  const uint32_t icon_size = 14U;
  const uint32_t icon_vertical_margin = 2U;
  const uint32_t progress_bar_width = 65U;
  const uint32_t progress_bar_height = 3U;

  // Init y position
  uint32_t y = padding;

  // Global elapsed time
  display_.setTextSize(1);
  display_.setCursor(padding, y);
  display_.print(FormatDuration(total_duty_time_));

  // Progress bar global
  y = y + 13U;
  display_.drawRect(padding, y, progress_bar_width, progress_bar_height,
                    SSD1306_WHITE);
  y = y + 1U;

  float global_progress = GetProgress(total_duty_time_, cycle_duration_);
  int16_t bar_end = GetProgressBarPosition(padding + 1, progress_bar_width - 2,
                                           global_progress);
  display_.drawLine(padding + 1, y, bar_end, y, SSD1306_WHITE);

  // Phase name
  y = y + 2U;
  display_.setCursor(padding, y);
  display_.print(phase_name_);

  // Progress bar phase
  y = y + 14U;
  display_.drawRect(padding, y, progress_bar_width, progress_bar_height,
                    SSD1306_WHITE);
  y = y + 1U;

  float phase_progress = GetProgress(phase_duty_time_, phase_duration_);
  bar_end = GetProgressBarPosition(padding + 1, progress_bar_width - 2,
                                   phase_progress);
  display_.drawLine(padding + 1, y, bar_end, y, SSD1306_WHITE);

  // Inlet air data
  y = y + 4U;
  display_.setCursor(padding, y);
  display_.print(
      FormatTemperaturePercent(inlet_air_temperature_, inlet_air_humidity_));

  // Outlet air data
  y = y + 15U;
  display_.setCursor(padding, y);
  display_.print(
      FormatTemperaturePercent(outlet_air_temperature_, outlet_air_humidity_));

  // ===== RIGHT SIDE ICONS AND VALUES =====

  // Init x position
  uint32_t x = SCREEN_WIDTH - padding - icon_size;
  // Reset y position
  y = padding;

  // Recycling icon and rate
  display_.drawBitmap(x, y, icon_recycling_bits, icon_recycling_width,
                      icon_recycling_height, SSD1306_WHITE);
  display_.setCursor(x - 2U, y + 1U);
  display_.setTextSize(1);
  display_.print(FormatPercent(recycling_rate_));

  // Ventilation icon and state
  y = y + icon_size + icon_vertical_margin;
  display_.drawBitmap(x + 1U, y, icon_fan_bits, icon_fan_width, icon_fan_height,
                      SSD1306_WHITE);
  display_.setCursor(x - 2U, y + 1U);
  display_.setTextSize(1);
  display_.print(GetStateStr(ventilation_state_));

  // Hydraulic heater icon, power, and temperature
  y = y + icon_size + icon_vertical_margin;
  display_.drawBitmap(x + 2U, y, icon_water_bits, icon_water_width,
                      icon_water_height, SSD1306_WHITE);

  // Hydraulic power percentage
  display_.setCursor(x - 1U, y - 4U);
  display_.setTextSize(1);
  display_.print(FormatPercent(hydraulic_heater_power_));

  // Water temperature
  display_.setCursor(x - 2U, y - 3U + 10U);
  display_.setTextSize(1);
  display_.print(FormatTemperature(water_temperature_));

  // Electric heater icon and state
  y = y + icon_size + icon_vertical_margin;
  display_.drawBitmap(x + 2U, y, icon_electric_bits, icon_electric_width,
                      icon_electric_height, SSD1306_WHITE);
  display_.setCursor(x - 2U, y + 3U);
  display_.setTextSize(1);
  display_.print(GetStateStr(electric_heater_state_));

  display_.display();
}

// Setters
void Display::SetTotalDutyTime(uint32_t seconds) {
  total_duty_time_ = seconds;
}

void Display::SetPhaseDutyTime(uint32_t seconds) {
  phase_duty_time_ = seconds;
}

void Display::SetCycleDuration(uint32_t seconds) {
  cycle_duration_ = seconds;
}

void Display::SetPhaseDuration(uint32_t seconds) {
  phase_duration_ = seconds;
}

void Display::SetPhase(const char* phase_name) {
  phase_name_ = String(phase_name);
}

void Display::SetInletAirData(float temperature, float humidity) {
  inlet_air_temperature_ = temperature;
  inlet_air_humidity_ = humidity;
}

void Display::SetOutletAirData(float temperature, float humidity) {
  outlet_air_temperature_ = temperature;
  outlet_air_humidity_ = humidity;
}

void Display::SetRecyclingRate(float percent) {
  recycling_rate_ = percent;
}

void Display::SetVentilationState(bool state) {
  ventilation_state_ = state;
}

void Display::SetHydraulicHeaterPower(float percent) {
  hydraulic_heater_power_ = percent;
}

void Display::SetWaterTemperature(float temperature) {
  water_temperature_ = temperature;
}

void Display::SetElectricHeaterPower(bool state) {
  electric_heater_state_ = state;
}

// Menu rendering
void Display::ClearMenuArea() {
  display_.clearDisplay();
}

void Display::DrawMenuLine(uint8_t line_index, const char* text, bool selected) {
  if (line_index >= 5) return;  // Max 5 lines on 64px display

  uint8_t y_position = line_index * 12;  // 12 pixels per line

  display_.setTextSize(1);
  display_.setCursor(0, y_position);

  if (selected) {
    // Draw inverted rectangle for selected item
    display_.fillRect(0, y_position, SCREEN_WIDTH, 10, SSD1306_WHITE);
    display_.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  } else {
    display_.setTextColor(SSD1306_WHITE);
  }

  display_.print(text);

  // Reset text color
  display_.setTextColor(SSD1306_WHITE);
}

void Display::UpdateMenuArea() {
  display_.display();
}
