#include <string.h>

#include "TftDisplay.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "Logger.h"

namespace
{

// createSprite() returns null when the heap cannot supply the buffer. Skipping
// the region leaves the previous pixels on screen, which is far better than
// drawing through a null pointer.
bool CreateCanvas(TFT_eSprite &canvas, int16_t width, int16_t height)
{
  if (canvas.createSprite(width, height) == nullptr)
  {
    Logger::Warning("TftDisplay: no memory for a %dx%d sprite", width, height);
    return false;
  }
  return true;
}

// True when two readings differ enough to be worth a repaint. Both NaN counts
// as unchanged, so a permanently absent probe does not repaint every frame.
bool ValueChanged(float a, float b, float epsilon = 0.05f)
{
  if (isnan(a) && isnan(b))
  {
    return false;
  }
  if (isnan(a) != isnan(b))
  {
    return true;
  }
  return fabsf(a - b) >= epsilon;
}

} // namespace

TftDisplay::TftDisplay()
    : previous_(),
      force_redraw_(true),
      fan_angle_deg_(0.0f),
      last_animation_ms_(0),
      blink_state_(false),
      last_blink_ms_(0) {}

void TftDisplay::Begin()
{
  tft_.init();
  tft_.setRotation(1); // landscape, 320 x 240
  tft_.fillScreen(UiTheme::kBackground);
  tft_.setTextColor(UiTheme::kValue, UiTheme::kPanel);

  force_redraw_ = true;
  Logger::Info("TftDisplay: ST7789 ready (%dx%d landscape)", kWidth, kHeight);
}

void TftDisplay::Invalidate()
{
  force_redraw_ = true;
}

void TftDisplay::RenderMain(const DisplayModel &model)
{
  uint32_t now = millis();

  // The fan animation and the cooldown blink run on their own clocks: they must
  // keep moving while every measured value sits still.
  bool animate = false;
  if (now - last_animation_ms_ >= kAnimationIntervalMs)
  {
    last_animation_ms_ = now;
    if (model.fan_on)
    {
      fan_angle_deg_ += 18.0f;
      if (fan_angle_deg_ >= 360.0f)
      {
        fan_angle_deg_ -= 360.0f;
      }
      animate = true;
    }
  }

  if (now - last_blink_ms_ >= kBlinkIntervalMs)
  {
    last_blink_ms_ = now;
    if (model.fan_cooling)
    {
      blink_state_ = !blink_state_;
      animate = true;
    }
    else if (blink_state_)
    {
      blink_state_ = false;
      animate = true;
    }
  }

  if (force_redraw_)
  {
    tft_.fillScreen(UiTheme::kBackground);
  }

  if (force_redraw_ || StatusBarChanged(model))
  {
    DrawStatusBar(model);
  }
  if (force_redraw_ || MeasurementChanged(model))
  {
    DrawMeasurementTile(model);
  }
  if (force_redraw_ || SetpointChanged(model))
  {
    DrawSetpointTile(model);
  }
  if (force_redraw_ || HydraulicChanged(model))
  {
    DrawHydraulicBlock(model);
  }
  if (force_redraw_ || BandChanged(model))
  {
    DrawStatusBand(model);
  }
  else if (animate)
  {
    DrawFanIcon(model);
  }

  previous_ = model;
  force_redraw_ = false;
}

// --- Change detection -------------------------------------------------------

bool TftDisplay::StatusBarChanged(const DisplayModel &model) const
{
  // Phase names are compared by content, not by pointer: the accessor happens
  // to return literals today, but a future buffer would break silently.
  return model.total_elapsed_s != previous_.total_elapsed_s ||
         strcmp(model.phase_name, previous_.phase_name) != 0 ||
         model.running != previous_.running ||
         model.lora_linked != previous_.lora_linked ||
         model.sensor_fault != previous_.sensor_fault;
}

bool TftDisplay::MeasurementChanged(const DisplayModel &model) const
{
  return ValueChanged(model.inlet_temperature, previous_.inlet_temperature) ||
         ValueChanged(model.inlet_humidity, previous_.inlet_humidity, 0.5f);
}

bool TftDisplay::SetpointChanged(const DisplayModel &model) const
{
  return ValueChanged(model.target_temperature, previous_.target_temperature) ||
         ValueChanged(model.target_humidity, previous_.target_humidity, 0.5f);
}

bool TftDisplay::HydraulicChanged(const DisplayModel &model) const
{
  return model.hydraulic_online != previous_.hydraulic_online ||
         model.hydraulic_enabled != previous_.hydraulic_enabled ||
         model.hydraulic_on != previous_.hydraulic_on ||
         ValueChanged(model.water_temperature, previous_.water_temperature) ||
         ValueChanged(model.tank_temperature, previous_.tank_temperature);
}

bool TftDisplay::BandChanged(const DisplayModel &model) const
{
  return model.fan_on != previous_.fan_on ||
         model.fan_cooling != previous_.fan_cooling ||
         model.electric_on != previous_.electric_on ||
         model.electric_enabled != previous_.electric_enabled ||
         model.damper_open != previous_.damper_open ||
         model.damper_moving != previous_.damper_moving ||
         ValueChanged(model.damper_position, previous_.damper_position, 2.0f);
}

// --- Regions ----------------------------------------------------------------

void TftDisplay::DrawStatusBar(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kWidth, kStatusH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kPanel);

  char duration[16];
  UiTheme::FormatDuration(model.total_elapsed_s, duration, sizeof(duration));

  // Elapsed time: fixed width, pinned left.
  canvas.setTextDatum(ML_DATUM);
  canvas.setTextColor(model.running ? UiTheme::kValue : UiTheme::kLabel,
                      UiTheme::kPanel);
  canvas.drawString(duration, 6, kStatusH / 2, 4);

  // Phase name: variable width, so it is the one that gets centred.
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(model.running ? UiTheme::kActive : UiTheme::kLabel,
                      UiTheme::kPanel);
  canvas.drawString(model.phase_name, kWidth / 2, kStatusH / 2, 4);

  // A stale probe blocks all heating, so it outranks the link icon.
  if (model.sensor_fault)
  {
    canvas.setTextDatum(MR_DATUM);
    canvas.setTextColor(UiTheme::kAlarm, UiTheme::kPanel);
    canvas.drawString("SONDE", kWidth - 6, kStatusH / 2, 2);
  }
  else
  {
    int16_t icon_x = kWidth - 18;
    UiIcons::DrawAntenna(canvas, icon_x, kStatusH / 2, 9,
                         model.lora_linked ? UiTheme::kActive : UiTheme::kInactive);
    if (!model.lora_linked)
    {
      UiIcons::DrawSlash(canvas, icon_x, kStatusH / 2, 9, UiTheme::kInactive);
    }
  }

  canvas.pushSprite(0, kStatusY);
  canvas.deleteSprite();
}

void TftDisplay::DrawMeasurementTile(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kTileW, kTileH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kBackground);
  canvas.fillRoundRect(2, 2, kTileW - 4, kTileH - 4, 4, UiTheme::kPanel);
  canvas.drawRoundRect(2, 2, kTileW - 4, kTileH - 4, 4, UiTheme::kBorder);

  canvas.setTextDatum(TC_DATUM);
  canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
  canvas.drawString("INJECTION", kTileW / 2, 8, 2);

  char text[16];
  canvas.setTextColor(UiTheme::kValue, UiTheme::kPanel);
  UiTheme::FormatValue(model.inlet_temperature, "", text, sizeof(text));
  canvas.drawString(text, kTileW / 2, 28, 6);

  UiTheme::FormatValue(model.inlet_humidity, " %HR", text, sizeof(text));
  canvas.drawString(text, kTileW / 2, 66, 4);

  canvas.pushSprite(0, kTileY);
  canvas.deleteSprite();
}

void TftDisplay::DrawSetpointTile(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kTileW, kTileH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kBackground);
  canvas.fillRoundRect(2, 2, kTileW - 4, kTileH - 4, 4, UiTheme::kPanel);
  canvas.drawRoundRect(2, 2, kTileW - 4, kTileH - 4, 4, UiTheme::kBorder);

  canvas.setTextDatum(TC_DATUM);
  canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
  canvas.drawString("CONSIGNE", kTileW / 2, 8, 2);

  char text[16];
  canvas.setTextColor(UiTheme::kSetpoint, UiTheme::kPanel);
  UiTheme::FormatValue(model.target_temperature, "", text, sizeof(text));
  canvas.drawString(text, kTileW / 2, 28, 6);

  UiTheme::FormatValue(model.target_humidity, " %HR", text, sizeof(text));
  canvas.drawString(text, kTileW / 2, 66, 4);

  canvas.pushSprite(kTileW, kTileY);
  canvas.deleteSprite();
}

void TftDisplay::DrawHydraulicBlock(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kWidth, kHydroH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kBackground);
  canvas.fillRoundRect(2, 2, kWidth - 4, kHydroH - 4, 4, UiTheme::kPanel);
  canvas.drawRoundRect(2, 2, kWidth - 4, kHydroH - 4, 4, UiTheme::kBorder);

  bool usable = model.hydraulic_online && model.hydraulic_enabled;
  uint16_t accent = !usable ? UiTheme::kInactive
                            : (model.hydraulic_on ? UiTheme::kWater : UiTheme::kInactive);

  UiIcons::DrawPump(canvas, 22, kHydroH / 2, 12, accent);
  if (!usable)
  {
    UiIcons::DrawSlash(canvas, 22, kHydroH / 2, 12, UiTheme::kInactive);
  }

  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
  canvas.drawString("HYDRAULIQUE", 42, 8, 2);

  canvas.setTextDatum(BL_DATUM);
  if (!model.hydraulic_online)
  {
    canvas.setTextColor(UiTheme::kWarning, UiTheme::kPanel);
    canvas.drawString("MODULE ABSENT", 42, kHydroH - 8, 2);
  }
  else if (!model.hydraulic_enabled)
  {
    canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
    canvas.drawString("DESACTIVE", 42, kHydroH - 8, 2);
  }
  else
  {
    canvas.setTextColor(model.hydraulic_on ? UiTheme::kActive : UiTheme::kLabel,
                        UiTheme::kPanel);
    canvas.drawString(model.hydraulic_on ? "CIRCULATEUR MARCHE" : "CIRCULATEUR ARRET",
                      42, kHydroH - 8, 2);
  }

  // Water temperatures only mean something while the module is answering.
  char text[24];
  canvas.setTextDatum(TR_DATUM);
  canvas.setTextColor(model.hydraulic_online ? UiTheme::kValue : UiTheme::kInactive,
                      UiTheme::kPanel);
  UiTheme::FormatValue(model.water_temperature, " C", text, sizeof(text));
  canvas.drawString(text, kWidth - 10, 8, 4);

  canvas.setTextDatum(BR_DATUM);
  canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
  UiTheme::FormatValue(model.tank_temperature, " C ballon", text, sizeof(text));
  canvas.drawString(text, kWidth - 10, kHydroH - 8, 2);

  canvas.pushSprite(0, kHydroY);
  canvas.deleteSprite();
}

void TftDisplay::DrawStatusBand(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kWidth, kBandH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kBackground);
  canvas.fillRoundRect(2, 2, kWidth - 4, kBandH - 4, 4, UiTheme::kPanel);
  canvas.drawRoundRect(2, 2, kWidth - 4, kBandH - 4, 4, UiTheme::kBorder);

  const int16_t icon_y = 26;

  // --- Ventilation: spins while running, blinks during the post-stop cooldown
  bool fan_visible = model.fan_on || (model.fan_cooling && blink_state_);
  uint16_t fan_color = fan_visible ? UiTheme::kActive : UiTheme::kInactive;
  UiIcons::DrawFan(canvas, kFanCx, kFanCy, kFanR, fan_color,
                   model.fan_on ? fan_angle_deg_ : 0.0f);
  canvas.setTextDatum(TC_DATUM);
  canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
  canvas.drawString(model.fan_cooling ? "REFROID." : "VENTIL.", kFanCx, kBandH - 22, 2);

  // --- Electric heating
  uint16_t heat_color = !model.electric_enabled ? UiTheme::kInactive
                        : (model.electric_on ? UiTheme::kHeat : UiTheme::kInactive);
  UiIcons::DrawLightning(canvas, 140, icon_y, 18, heat_color);
  if (!model.electric_enabled)
  {
    UiIcons::DrawSlash(canvas, 140, icon_y, 18, UiTheme::kInactive);
  }
  canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
  canvas.drawString("CHAUFFAGE", 140, kBandH - 22, 2);

  // --- Air damper: named state, never a percentage. The measured position only
  // appears as a travel bar while the vane is still moving.
  uint16_t damper_color = model.damper_open ? UiTheme::kActive : UiTheme::kWater;
  UiIcons::DrawDamper(canvas, 250, icon_y, 18, damper_color, model.damper_open);
  canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
  canvas.drawString(model.damper_open ? "EXTRACTION" : "RECIRCULATION",
                    250, kBandH - 22, 2);

  if (model.damper_moving && !isnan(model.damper_position))
  {
    const int16_t bar_x = 196;
    const int16_t bar_w = 108;
    const int16_t bar_y = kBandH - 8;
    canvas.drawRect(bar_x, bar_y, bar_w, 5, UiTheme::kBorder);
    int16_t filled = static_cast<int16_t>((model.damper_position / 100.0f) * (bar_w - 2));
    if (filled > 0)
    {
      canvas.fillRect(bar_x + 1, bar_y + 1, filled, 3, UiTheme::kWarning);
    }
  }

  canvas.pushSprite(0, kBandY);
  canvas.deleteSprite();
}

void TftDisplay::DrawFanIcon(const DisplayModel &model)
{
  // Just the disc: a 40x40 canvas costs 3 KB and a negligible slice of the
  // shared SPI bus, against 46 KB and ~9 ms for the whole band.
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kFanBox, kFanBox))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kPanel);

  bool fan_visible = model.fan_on || (model.fan_cooling && blink_state_);
  uint16_t fan_color = fan_visible ? UiTheme::kActive : UiTheme::kInactive;
  UiIcons::DrawFan(canvas, kFanBox / 2, kFanBox / 2, kFanR, fan_color,
                   model.fan_on ? fan_angle_deg_ : 0.0f);

  canvas.pushSprite(kFanCx - kFanBox / 2, kBandY + kFanCy - kFanBox / 2);
  canvas.deleteSprite();
}
