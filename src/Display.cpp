#include "Display.h"
#include "config.h"

Display::Display(Adafruit_SSD1306 *oled)
    : oled_(oled),
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
      electric_heater_state_(false)
{
}

bool Display::Begin()
{
  oled_->clearDisplay();
  oled_->setFont(&Chicago5pt7b);
  oled_->setTextColor(SSD1306_WHITE);
  oled_->setCursor(0, 15);
  oled_->println("DRYER");
  oled_->setCursor(0, 35);
  oled_->println("INIT...");

  Serial.println("About to refresh display with init message...");
  oled_->display();
  Serial.println("Display refreshed!");

  // Force delay to keep message visible
  delay(3000);
  Serial.println("Init delay complete");

  Serial.println("Display initialized");
  return true;
}

void Display::Clear()
{
  oled_->clearDisplay();
}

void Display::Update()
{
  RenderHomePage();
}

void Display::RenderHomePage()
{
  oled_->clearDisplay();

  // Layout constants (from ESPHome)
  const uint32_t padding = 1U;
  const uint32_t icon_size = 14U;
  const uint32_t icon_vertical_margin = 2U;
  const uint32_t progress_bar_width = 65U;
  const uint32_t progress_bar_height = 3U;

  // Init y position
  uint32_t y = padding;

  // Init
  oled_->setTextColor(SSD1306_WHITE);

  // Global elapsed time
  oled_->setFont(&Chicago5pt7b);
  oled_->setCursor(padding, y + 10);
  oled_->print(FormatDuration(total_duty_time_));

  // Progress bar global
  y = y + 13U;
  oled_->drawRect(padding, y, progress_bar_width, progress_bar_height,
                  SSD1306_WHITE);
  y = y + 1U;

  float global_progress = GetProgress(total_duty_time_, cycle_duration_);
  int16_t bar_end = GetProgressBarPosition(padding + 1, progress_bar_width - 2,
                                           global_progress);
  oled_->drawLine(padding + 1, y, bar_end, y, SSD1306_WHITE);

  // Phase name
  y = y + 1U;
  oled_->setFont(&Chicago5pt7b);
  oled_->setCursor(padding, y + 11);
  oled_->print(phase_name_);

  // Progress bar phase
  y = y + 15U;
  oled_->drawRect(padding, y, progress_bar_width, progress_bar_height,
                  SSD1306_WHITE);
  y = y + 1U;

  float phase_progress = GetProgress(phase_duty_time_, phase_duration_);
  bar_end = GetProgressBarPosition(padding + 1, progress_bar_width - 2,
                                   phase_progress);
  oled_->drawLine(padding + 1, y, bar_end, y, SSD1306_WHITE);

  // Inlet air data
  y = y + 4U;
  oled_->setFont(&Chicago6pt7b);
  oled_->setCursor(padding, y + 11);
  oled_->print(
      FormatTemperaturePercent(inlet_air_temperature_, inlet_air_humidity_));

  // Outlet air data
  y = y + 15U;
  oled_->setFont(&Chicago6pt7b);
  oled_->setCursor(padding, y + 11);
  oled_->print(
      FormatTemperaturePercent(outlet_air_temperature_, outlet_air_humidity_));

  // ===== RIGHT SIDE ICONS AND VALUES =====

  // Reset y position
  y = padding;

  // Recycling icon and rate
  uint32_t x_recycling = oled_->width() - padding - icon_recycling_width;
  oled_->drawXBitmap(x_recycling, y, icon_recycling_bits, icon_recycling_width,
                     icon_recycling_height, SSD1306_WHITE);
  oled_->setFont(&Chicago5pt7b);
  PrintAlignedRight(x_recycling - 2U, y + 11, FormatPercent(recycling_rate_));

  // Ventilation icon and state
  y = y + icon_size + icon_vertical_margin;
  uint32_t x_fan = oled_->width() - padding - icon_fan_width;
  oled_->drawXBitmap(x_fan, y, icon_fan_bits, icon_fan_width, icon_fan_height,
                     SSD1306_WHITE);
  oled_->setFont(&Chicago4pt7b);
  PrintAlignedRight(x_fan - 2U, y + 10, GetStateStr(ventilation_state_));

  // Hydraulic heater icon, power, and temperature
  y = y + icon_size + icon_vertical_margin;
  uint32_t x_water = oled_->width() - padding - icon_water_width;
  oled_->drawXBitmap(x_water, y, icon_water_bits, icon_water_width,
                     icon_water_height, SSD1306_WHITE);

  // Hydraulic power percentage
  oled_->setFont(&Chicago5pt7b);
  PrintAlignedRight(x_water - 1U, y + 6, FormatPercent(hydraulic_heater_power_));

  // Water temperature
  oled_->setFont(&Chicago5pt7b);
  PrintAlignedRight(x_water - 2U, y + 17, FormatTemperature(water_temperature_));

  // Electric heater icon and state
  y = y + icon_size + icon_vertical_margin;
  uint32_t x_electric = oled_->width() - padding - icon_electric_width;
  oled_->drawXBitmap(x_electric, y, icon_electric_bits, icon_electric_width,
                     icon_electric_height, SSD1306_WHITE);
  oled_->setFont(&Chicago4pt7b);
  PrintAlignedRight(x_electric - 2U, y + 12, GetStateStr(electric_heater_state_));

  oled_->display();
}

void Display::PrintAlignedRight(int16_t x, int16_t y, const char *text)
{
  int16_t x1, y1;
  uint16_t w, h;

  // Cette fonction calcule la boîte englobante du texte sans le dessiner
  oled_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  // On place le curseur pour que la fin du texte arrive à l'abscisse 'x'
  oled_->setCursor(x - w, y);
  oled_->print(text);
}

// Setters
void Display::SetTotalDutyTime(uint32_t seconds)
{
  total_duty_time_ = seconds;
}

void Display::SetPhaseDutyTime(uint32_t seconds)
{
  phase_duty_time_ = seconds;
}

void Display::SetCycleDuration(uint32_t seconds)
{
  cycle_duration_ = seconds;
}

void Display::SetPhaseDuration(uint32_t seconds)
{
  phase_duration_ = seconds;
}

void Display::SetPhase(const char *phase_name)
{
  phase_name_ = String(phase_name);
}

void Display::SetInletAirData(float temperature, float humidity)
{
  inlet_air_temperature_ = temperature;
  inlet_air_humidity_ = humidity;

  // Serial.print("Display Trace [Inlet]: ");
  // Serial.print(temperature, 1);
  // Serial.print("°C, ");
  // Serial.print(humidity, 1);
  // Serial.println("%");
}

void Display::SetOutletAirData(float temperature, float humidity)
{
  outlet_air_temperature_ = temperature;
  outlet_air_humidity_ = humidity;

  // Serial.print("Display Trace [Outlet]: ");
  // Serial.print(temperature, 1);
  // Serial.print("°C, ");
  // Serial.print(humidity, 1);
  // Serial.println("%");
}

void Display::SetRecyclingRate(float percent)
{
  recycling_rate_ = percent;
}

void Display::SetVentilationState(bool state)
{
  ventilation_state_ = state;
}

void Display::SetHydraulicHeaterPower(float percent)
{
  hydraulic_heater_power_ = percent;
}

void Display::SetWaterTemperature(float temperature)
{
  water_temperature_ = temperature;
}

void Display::SetElectricHeaterPower(bool state)
{
  electric_heater_state_ = state;
}

// Menu rendering
void Display::ClearMenuArea()
{
  oled_->clearDisplay();
}

void Display::DrawMenuHeader(const char *title)
{
  if (title == nullptr)
    return;

  // Draw header at line 0 without background
  oled_->setFont(&Chicago5pt7b);
  oled_->setTextColor(SSD1306_WHITE);
  oled_->setCursor(2, 10);
  oled_->print(title);

  // Draw separation line below the header
  oled_->drawLine(0, 12, oled_->width() - 1, 12, SSD1306_WHITE);
}

void Display::DrawMenuLine(uint8_t line_index, const char *text, bool selected)
{
  if (line_index >= 5)
    return; // Max 5 lines on 64px display

  uint8_t y_position = line_index * 12; // 12 pixels per line

  oled_->setFont(&Chicago5pt7b);
  oled_->setCursor(0, y_position + 10);

  if (selected)
  {
    // Draw inverted rectangle for selected item
    oled_->fillRect(0, y_position, oled_->width(), 12, SSD1306_WHITE);
    oled_->setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  }
  else
  {
    oled_->setTextColor(SSD1306_WHITE);
  }

  oled_->print(text);

  // Reset text color
  oled_->setTextColor(SSD1306_WHITE);
}

void Display::UpdateMenuArea()
{
  oled_->display();
}
