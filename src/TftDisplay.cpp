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
      last_blink_ms_(0),
      splash_started_ms_(0),
      splash_active_(false) {}

void TftDisplay::Begin()
{
  tft_.init();
  tft_.setRotation(1); // landscape, 320 x 240

  // Nothing clears the splash here: it has to survive the rest of setup, where
  // the radio probe alone can block for several seconds. EndSplash() retires it
  // once the board is ready, and the first RenderMain repaints over it.
  ShowSplash();

  tft_.setTextColor(UiTheme::kValue, UiTheme::kPanel);

  force_redraw_ = true;
  // Note this says the initialisation sequence was *sent*, not that a panel
  // received it: TFT_eSPI never reads anything back. The splash above is the
  // only real evidence that the wiring works.
  Logger::Info("TftDisplay: ST7789 init sent (%dx%d landscape)", kWidth, kHeight);
}

void TftDisplay::ShowSplash()
{
  // A blank main screen looks exactly like a dead panel, which makes a wiring
  // fault impossible to tell from a working board with nothing to show.
  //
  // The built-in fonts carry ASCII 32..127 and nothing else, so these strings
  // are deliberately unaccented: a "é" is two UTF-8 bytes and would come out as
  // two stray glyphs.
  tft_.fillScreen(UiTheme::kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(UiTheme::kValue, UiTheme::kBackground);
  tft_.drawString("SECHOIR PAYSAN", kWidth / 2, kHeight / 2 - 12, 4);
  tft_.setTextColor(UiTheme::kLabel, UiTheme::kBackground);
  tft_.drawString("par la Forge", kWidth / 2, kHeight / 2 + 16, 2);

  splash_started_ms_ = millis();
  splash_active_ = true;
}

void TftDisplay::ShowBootStage(const char *stage)
{
  if (!splash_active_)
  {
    return;
  }

  // The band is cleared first: drawString's background only covers the width of
  // the new string, so a shorter stage name would leave the tail of the
  // previous one behind it.
  tft_.fillRect(0, kSplashStageY, kWidth, kSplashStageH, UiTheme::kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(UiTheme::kLabel, UiTheme::kBackground);
  tft_.drawString(stage, kWidth / 2, kSplashStageY + kSplashStageH / 2, 2);
}

void TftDisplay::EndSplash()
{
  uint32_t elapsed = millis() - splash_started_ms_;
  if (elapsed < kSplashMinMs)
  {
    delay(kSplashMinMs - elapsed);
  }
  splash_active_ = false;
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
         model.extraction_moving != previous_.extraction_moving ||
         model.recycling_moving != previous_.recycling_moving ||
         // The openings are printed as whole percents now, so the band has to
         // repaint on every step of the last digit — one repaint per 1.5 s over
         // the actuator's 150 s travel, which the band can well afford.
         ValueChanged(model.extraction_position, previous_.extraction_position, 0.5f) ||
         ValueChanged(model.recycling_position, previous_.recycling_position, 0.5f);
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
  UiIcons::DrawLightning(canvas, kHeatCx, icon_y, 18, heat_color);
  if (!model.electric_enabled)
  {
    UiIcons::DrawSlash(canvas, kHeatCx, icon_y, 18, UiTheme::kInactive);
  }
  canvas.setTextColor(UiTheme::kLabel, UiTheme::kPanel);
  canvas.drawString("CHAUFFAGE", kHeatCx, kBandH - 22, 2);

  // --- The two registers, one row each.
  //
  // The commanded air path is not printed as a word any more: it is the row
  // whose label is lit. Extraction and recycling are complementary, so exactly
  // one of them is lit at any time, and the two bars show how far each actually
  // is — which is where an actuator that never arrives gives itself away.
  DrawRegisterRow(canvas, kRegRow1Cy, "EXTRACT.", model.extraction_position,
                  model.extraction_moving, model.damper_open);
  DrawRegisterRow(canvas, kRegRow2Cy, "RECYCL.", model.recycling_position,
                  model.recycling_moving, !model.damper_open);

  canvas.pushSprite(0, kBandY);
  canvas.deleteSprite();
}

void TftDisplay::DrawRegisterRow(TFT_eSprite &canvas, int16_t cy, const char *label,
                                 float position, bool moving, bool should_be_open)
{
  // Lit while this is the register the command wants open, dim otherwise. Green
  // rather than white so it reads as "this is the air path in use", matching the
  // fan's own active colour.
  canvas.setTextDatum(ML_DATUM);
  canvas.setTextColor(should_be_open ? UiTheme::kActive : UiTheme::kLabel,
                      UiTheme::kPanel);
  canvas.drawString(label, kRegX, cy, 2);

  // Yellow while travelling, matching the degraded/transient colour used
  // everywhere else; then the steady colour of whichever path this register is.
  uint16_t fill = moving ? UiTheme::kWarning
                         : (should_be_open ? UiTheme::kActive : UiTheme::kWater);

  int16_t bar_y = cy - kRegBarH / 2;
  canvas.drawRect(kRegBarX, bar_y, kRegBarW, kRegBarH, UiTheme::kBorder);

  char text[8];
  if (isnan(position))
  {
    // No usable feedback: an empty bar and dashes, never a 0 % that would read
    // as a closed register.
    snprintf(text, sizeof(text), "--%%");
    canvas.setTextColor(UiTheme::kInactive, UiTheme::kPanel);
  }
  else
  {
    int16_t filled = static_cast<int16_t>((position / 100.0f) * (kRegBarW - 2));
    if (filled > 0)
    {
      canvas.fillRect(kRegBarX + 1, bar_y + 1, filled, kRegBarH - 2, fill);
    }
    snprintf(text, sizeof(text), "%d%%", static_cast<int>(position + 0.5f));
    canvas.setTextColor(UiTheme::kValue, UiTheme::kPanel);
  }

  canvas.setTextDatum(MR_DATUM);
  canvas.drawString(text, kRegRight, cy, 2);
  canvas.setTextDatum(TC_DATUM);
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
