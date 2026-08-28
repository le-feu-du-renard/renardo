#include "TftDisplay.h"
#include "StatusIndicator.h"
#include "UiIcons.h"
#include "UiTheme.h"
#include "fonts/MonoFonts.h"
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

// How a phase names and colours itself. Indexed by DisplayModel::phase, whose
// values are DryerPhase's; main.cpp is where the two are tied together and
// asserts the correspondence.
//
// Initialisation is blue rather than lit: it is the warm-up, and colouring it
// like a working phase would claim the dryer is doing something to the crop
// when it is not yet. Extraction is amber because the damper is open and humid
// air is going outside — worth noticing, though nothing is wrong.
struct PhaseStyle
{
  const char *label;
  uint16_t    color;
};

const PhaseStyle kPhases[] = {
    {"ARRET",          UiTheme::kMuted},    // kStop, nothing is running
    {"INITIALISATION", UiTheme::kNeutral},  // kInit, warming up
    {"BRASSAGE",       UiTheme::kAccent},   // kBrassage, the working phase
    {"EXTRACTION",     UiTheme::kWarn},     // kExtraction, damper open
    {"CLIMAT",         UiTheme::kAccent},   // kClimat, held rather than cycled
};

const PhaseStyle &StyleFor(uint8_t phase)
{
  if (phase >= sizeof(kPhases) / sizeof(kPhases[0]))
  {
    return kPhases[0];
  }
  return kPhases[phase];
}

// Card and cell corner radius, from the mock-up.
constexpr int16_t kRadius = 5;

// Draws a panel with its outline, the shape every card and cell shares.
void DrawCard(TFT_eSprite &canvas, int16_t x, int16_t y, int16_t width,
              int16_t height, uint16_t outline)
{
  canvas.fillRoundRect(x, y, width, height, kRadius, UiTheme::kPanel);
  canvas.drawRoundRect(x, y, width, height, kRadius, outline);
}

// DrawText takes the y of the *top of the glyph cell*, which is what TFT_eSPI's
// top datums expect for a free font, and what UiLayout::CapTop() produces when
// a line has to be centred on a band.

void DrawText(TFT_eSprite &canvas, const GFXfont *font, uint16_t color,
              uint8_t datum, const char *text, int16_t x, int16_t y)
{
  canvas.setFreeFont(font);
  canvas.setTextColor(color);
  canvas.setTextDatum(datum);
  canvas.drawString(text, x, y);
}

// The longest string each cell can be asked to hold. The fonts are monospace
// with a uniform cell, so a width is a character count times an advance, and
// whether a label fits is a question the compiler can answer.
//
// This matters because the type scale is the thing most likely to be revised:
// the first attempt at this interface transposed the design mock-up's sizes
// literally and produced captions a millimetre tall. The next revision should
// fail to build rather than silently clip "HYDRAULIQUE" to "HYDRAULIQU".
constexpr int16_t kLongestStripLabel  = 11; // HYDRAULIQUE
constexpr int16_t kLongestDeviceLabel = 7;  // RECIRC.
constexpr int16_t kLongestDeviceState = 8;  // REFROID., OUV. 65%
constexpr int16_t kLongestPhaseName   = 14; // INITIALISATION
constexpr int16_t kLongestHint        = 38; // the feedback and hydraulic alarms

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
  tft_.setRotation(3); // landscape, 320 x 240, flipped 180 degrees

  // Nothing clears the splash here: it has to survive the rest of setup, where
  // the radio probe alone can block for several seconds. EndSplash() retires it
  // once the board is ready, and the first RenderMain repaints over it.
  ShowSplash();

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
  // Drawn with the built-in fonts rather than the generated ones: this is the
  // first thing that runs, and it should depend on as little as possible in
  // order to be able to report that everything else failed.
  tft_.fillScreen(UiTheme::kBackground);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(UiTheme::kAccent, UiTheme::kBackground);
  tft_.drawString("SECHOIR PAYSAN", kWidth / 2, kHeight / 2 - 12, 4);
  tft_.setTextColor(UiTheme::kMuted, UiTheme::kBackground);
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
  tft_.setTextColor(UiTheme::kMuted, UiTheme::kBackground);
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

  if (force_redraw_ || ProgressChanged(model))
  {
    DrawProgressBar(model);
  }
  if (force_redraw_ || HeaderChanged(model))
  {
    DrawHeader(model);
  }
  if (force_redraw_ || InletChanged(model))
  {
    DrawInletCard(model);
  }
  if (force_redraw_ || TargetChanged(model))
  {
    DrawTargetCard(model);
  }
  if (force_redraw_ || StripChanged(model))
  {
    DrawStrip(model);
  }
  if (force_redraw_ || DevicesChanged(model))
  {
    DrawDevices(model);
  }
  else if (animate)
  {
    DrawFanIcon(model);
  }
  if (force_redraw_ || HintChanged(model))
  {
    DrawHintBar(model);
  }

  previous_ = model;
  force_redraw_ = false;
}

// --- Change detection -------------------------------------------------------

bool TftDisplay::ProgressChanged(const DisplayModel &model) const
{
  // A third of a percent is about one pixel of a 320 px bar, so the region
  // repaints once per pixel of travel and not once per sample.
  return model.phase != previous_.phase ||
         ValueChanged(model.phase_progress, previous_.phase_progress, 0.003f);
}

bool TftDisplay::HeaderChanged(const DisplayModel &model) const
{
  return model.total_elapsed_s != previous_.total_elapsed_s ||
         model.phase != previous_.phase ||
         model.running != previous_.running ||
         model.eco_enabled != previous_.eco_enabled ||
         model.eco_window != previous_.eco_window;
}

bool TftDisplay::InletChanged(const DisplayModel &model) const
{
  return ValueChanged(model.inlet_temperature, previous_.inlet_temperature) ||
         ValueChanged(model.inlet_humidity, previous_.inlet_humidity, 0.5f);
}

bool TftDisplay::TargetChanged(const DisplayModel &model) const
{
  return ValueChanged(model.target_temperature, previous_.target_temperature) ||
         ValueChanged(model.target_humidity, previous_.target_humidity, 0.5f);
}

bool TftDisplay::StripChanged(const DisplayModel &model) const
{
  return model.hydraulic_online != previous_.hydraulic_online ||
         model.hydraulic_enabled != previous_.hydraulic_enabled ||
         model.hydraulic_demand != previous_.hydraulic_demand ||
         ValueChanged(model.water_temperature, previous_.water_temperature) ||
         ValueChanged(model.tank_temperature, previous_.tank_temperature);
}

bool TftDisplay::DevicesChanged(const DisplayModel &model) const
{
  return model.fan_on != previous_.fan_on ||
         model.fan_cooling != previous_.fan_cooling ||
         model.electric_on != previous_.electric_on ||
         model.electric_enabled != previous_.electric_enabled ||
         model.heat_source != previous_.heat_source ||
         model.damper_open != previous_.damper_open ||
         model.extraction_moving != previous_.extraction_moving ||
         model.recycling_moving != previous_.recycling_moving ||
         model.damper_count != previous_.damper_count ||
         // The openings are printed as whole percents, so the row has to
         // repaint on every step of the last digit — one repaint per 1.5 s over
         // the actuator's 150 s travel, which the row can well afford.
         ValueChanged(model.extraction_position, previous_.extraction_position, 0.5f) ||
         ValueChanged(model.recycling_position, previous_.recycling_position, 0.5f);
}

bool TftDisplay::HintChanged(const DisplayModel &model) const
{
  return model.fault != previous_.fault;
}

// --- Regions ----------------------------------------------------------------

void TftDisplay::DrawProgressBar(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kWidth, kProgressH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kPanelSunken);

  if (!isnan(model.phase_progress))
  {
    float clamped = model.phase_progress;
    clamped = clamped < 0.0f ? 0.0f : (clamped > 1.0f ? 1.0f : clamped);
    int16_t filled = static_cast<int16_t>(clamped * kWidth);
    if (filled > 0)
    {
      canvas.fillRect(0, 0, filled, kProgressH, StyleFor(model.phase).color);
    }
  }

  canvas.pushSprite(0, kProgressY);
  canvas.deleteSprite();
}

void TftDisplay::DrawHeader(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kWidth, kHeaderH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kPanelSunken);
  canvas.drawFastHLine(0, kHeaderH - 1, kWidth, UiTheme::kBorder);

  const int16_t band   = kHeaderH - 1; // the rule at the bottom is not content
  const int16_t center = band / 2;
  const PhaseStyle &phase = StyleFor(model.phase);

  // The longest phase name must stop short of the centred clock.
  static_assert(kPad + 11 + kLongestPhaseName * kMono12BAdvance <=
                    kWidth / 2 - (8 * kMono14BAdvance) / 2,
                "phase name runs into the clock");

  // Phase: a dot in the phase colour, then the name in the same colour.
  canvas.fillCircle(kPad + 3, center, 3, phase.color);
  DrawText(canvas, &Mono12B, phase.color, TL_DATUM, phase.label, kPad + 11,
           UiLayout::CapTop(center, kMono12BBaseline, kMono12BCapHeight));

  // Elapsed time, centred. Monospace makes this safe: unlike the phase name it
  // never changes width, so nothing either side of it can be pushed about.
  char duration[16];
  UiTheme::FormatDuration(model.total_elapsed_s, duration, sizeof(duration));
  DrawText(canvas, &Mono14B, model.running ? UiTheme::kText : UiTheme::kMuted,
           TC_DATUM, duration, kWidth / 2,
           UiLayout::CapTop(center, kMono14BBaseline, kMono14BCapHeight));

  // Eco, in the corner the clock and the phase name leave empty.
  //
  // Two states rather than one, because armed and acting are different facts.
  // Green says the setpoint on the card below has actually been lowered, which
  // is the only thing on screen that explains why it dropped; blue says the
  // schedule is set and waiting for its hours. Blue and not grey: grey is this
  // interface's word for switched off in the configuration, and eco outside its
  // window is enabled and idle, which is exactly what blue means everywhere
  // else. Nothing at all is drawn when eco is off — an operator who has never
  // enabled it should never have to learn what the corner means.
  if (model.eco_enabled)
  {
    // The word, right-aligned on the margin. The phase name stops around
    // x=117 and the centred clock ends at 192, so this corner was the one
    // piece of the layout with nothing in it.
    constexpr int16_t kLabelChars = 3; // "ECO"
    constexpr int16_t kBadgeX = kWidth - kPad - kLabelChars * kMono12BAdvance;

    static_assert(kBadgeX > kWidth / 2 + (8 * kMono14BAdvance) / 2,
                  "eco badge runs into the clock");

    const uint16_t eco_color =
        model.eco_window ? UiTheme::kOk : UiTheme::kNeutral;

    DrawText(canvas, &Mono12B, eco_color, TR_DATUM, "ECO", kWidth - kPad,
             UiLayout::CapTop(center, kMono12BBaseline, kMono12BCapHeight));
  }

  canvas.pushSprite(0, kHeaderY);
  canvas.deleteSprite();
}

void TftDisplay::DrawFigurePair(TFT_eSprite &canvas, int16_t card_width,
                                const GFXfont *font, int16_t advance,
                                int16_t baseline, int16_t cap_height,
                                uint16_t color, float temperature,
                                float humidity)
{
  // Temperature and humidity as two figures of the same size, each carrying its
  // own unit — the degree sign and the percent sign say which is which, so the
  // captions that used to sit above them are gone and the figures get the room.
  //
  // The pair is centred as a block whose width does not depend on the readings:
  // five cells are reserved for the temperature and four for the humidity, and
  // each figure is right-aligned inside its own slot. Both fields do change
  // width — "9.5~" against "42.3~", "38%" against "100%" — and left-aligning
  // them would slide the whole group sideways the first cold morning, or the
  // first time the inlet air saturates.
  //
  // Right-aligning rather than padding also puts the degree and percent signs
  // on fixed columns, so the units stay still even as the digits under them
  // change count.
  const int16_t gap   = advance / 2;
  const int16_t width = (kFigureCells + kPercentCells) * advance + gap;
  const int16_t left  = (card_width - width) / 2;
  const int16_t top   = UiLayout::CapTop(kFigureCenterY, baseline, cap_height);

  char text[16];

  UiTheme::FormatTemperature(temperature, text, sizeof(text));
  DrawText(canvas, font, color, TR_DATUM, text, left + kFigureCells * advance,
           top);

  UiTheme::FormatPercent(humidity, text, sizeof(text));
  DrawText(canvas, font, color, TR_DATUM, text, left + width, top);
}

void TftDisplay::DrawInletCard(const DisplayModel &model)
{
  static_assert((kFigureCells + kPercentCells) * kMono26BAdvance +
                    kMono26BAdvance / 2 <= kInletW - 2 * kPad,
                "injection figures overflow their card");

  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kInletW, kCardH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kBackground);
  // Outlined in the accent rather than the plain border: this is the card the
  // eye should land on first, and it is the only one whose figures are live.
  DrawCard(canvas, 0, 0, kInletW, kCardH, UiTheme::kAccentDim);

  DrawText(canvas, &Mono12B, UiTheme::kAccent, TC_DATUM, "INJECTION",
           kInletW / 2,
           UiLayout::CapTop(kCardLabelY, kMono12BBaseline, kMono12BCapHeight));

  DrawFigurePair(canvas, kInletW, &Mono26B, kMono26BAdvance, kMono26BBaseline,
                 kMono26BCapHeight, UiTheme::kAccent, model.inlet_temperature,
                 model.inlet_humidity);

  canvas.pushSprite(kInletX, kCardY);
  canvas.deleteSprite();
}

void TftDisplay::DrawTargetCard(const DisplayModel &model)
{
  static_assert((kFigureCells + kPercentCells) * kMono18BAdvance +
                    kMono18BAdvance / 2 <= kTargetW - 2 * kPad,
                "setpoint figures overflow their card");

  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kTargetW, kCardH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kBackground);
  DrawCard(canvas, 0, 0, kTargetW, kCardH, UiTheme::kBorder);

  DrawText(canvas, &Mono12B, UiTheme::kMuted, TC_DATUM, "CONSIGNE",
           kTargetW / 2,
           UiLayout::CapTop(kCardLabelY, kMono12BBaseline, kMono12BCapHeight));

  // Set in white rather than the accent: a setpoint is what was asked for, not
  // what is happening, and only the live card should pull the eye.
  DrawFigurePair(canvas, kTargetW, &Mono18B, kMono18BAdvance, kMono18BBaseline,
                 kMono18BCapHeight, UiTheme::kText, model.target_temperature,
                 model.target_humidity);

  canvas.pushSprite(kTargetX, kCardY);
  canvas.deleteSprite();
}

void TftDisplay::DrawStripCell(TFT_eSprite &canvas, int16_t x, const char *label,
                               const char *value, uint16_t value_color)
{
  static_assert(kLongestStripLabel * kMono12BAdvance <= kStripCellW - 2 * kPad,
                "strip caption overflows its cell");

  DrawCard(canvas, x, 0, kStripCellW, kStripH, UiTheme::kBorder);

  const int16_t center = x + kStripCellW / 2;
  DrawText(canvas, &Mono12B, UiTheme::kMuted, TC_DATUM, label, center,
           UiLayout::CapTop(11, kMono12BBaseline, kMono12BCapHeight));
  DrawText(canvas, &Mono14B, value_color, TC_DATUM, value, center,
           UiLayout::CapTop(24, kMono14BBaseline, kMono14BCapHeight));
}

void TftDisplay::DrawStrip(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kWidth, kStripH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kBackground);

  // The hydraulic cell carries four states, not two, and each takes the colour
  // its situation deserves: a module that has stopped answering is a fault and
  // is red, one switched off at the menu is grey, a module cleared to run is
  // green, and one merely idle is blue — normal, and not worth an alarm colour.
  //
  // `ON` reads "cleared to run", not "the circulator is turning". The module
  // regulates itself and the dryer never asked to be told when it fires, so
  // claiming more than the permission would be claiming what nobody measured.
  const char *state = "OFF";
  uint16_t state_color = UiTheme::kNeutral;
  if (!model.hydraulic_online)
  {
    state = "ABSENT";
    state_color = UiTheme::kDanger;
  }
  else if (!model.hydraulic_enabled)
  {
    state = "DESACT.";
    state_color = UiTheme::kMuted;
  }
  else if (model.hydraulic_demand)
  {
    state = "ON";
    state_color = UiTheme::kOk;
  }

  const int16_t x1 = kMargin;
  const int16_t x2 = x1 + kStripCellW + kGutter;
  const int16_t x3 = x2 + kStripCellW + kGutter;

  DrawStripCell(canvas, x1, "HYDRAULIQUE", state, state_color);

  // Water temperatures only mean something while the module is answering. Blue
  // while they do: they are readings to consult, not states to act on.
  uint16_t water_color = model.hydraulic_online ? UiTheme::kNeutral : UiTheme::kMuted;
  char text[16];

  UiTheme::FormatTemperature(model.water_temperature, text, sizeof(text));
  DrawStripCell(canvas, x2, "EAU CIRC.", text, water_color);

  UiTheme::FormatTemperature(model.tank_temperature, text, sizeof(text));
  DrawStripCell(canvas, x3, "BALLON", text, water_color);

  canvas.pushSprite(0, kStripY);
  canvas.deleteSprite();
}

void TftDisplay::DrawDeviceCell(TFT_eSprite &canvas, int16_t x, const char *label,
                                const char *state, uint16_t pill_color)
{
  DrawCard(canvas, x, 0, kDeviceCellW, kDeviceH, UiTheme::kBorder);

  static_assert(kLongestDeviceLabel * kMono12BAdvance <= kDeviceCellW - 4,
                "device caption overflows its cell");
  static_assert(kLongestDeviceState * kMono14BAdvance <= kDeviceCellW - 4,
                "device state word overflows its cell");

  const int16_t center = x + kDeviceCellW / 2;
  DrawText(canvas, &Mono12B, UiTheme::kMuted, TC_DATUM, label, center, 40);
  DrawText(canvas, &Mono14B, pill_color, TC_DATUM, state, center,
           UiLayout::CapTop(61, kMono14BBaseline, kMono14BCapHeight));
}

void TftDisplay::DrawDevices(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kWidth, kDeviceH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kBackground);

  const int16_t x1 = kMargin;
  const int16_t x2 = x1 + kDeviceCellW + kGutter;
  const int16_t x3 = x2 + kDeviceCellW + kGutter;
  const int16_t x4 = x3 + kDeviceCellW + kGutter;

  // Each cell is laid down before its icon: DrawDeviceCell fills the card, so
  // an icon drawn first would be painted over.
  //
  // The state colours follow one rule across all four cells: green is running,
  // blue is idle and available, grey is switched off in the configuration, and
  // amber is in transit. None of them is red — a stopped fan is not a fault,
  // and spending red on ordinary idleness is how red stops meaning anything.

  // --- Ventilation: spins while running, blinks through the post-stop cooldown
  DrawDeviceCell(canvas, x1, "VENTIL.",
                 model.fan_cooling ? "REFROID." : (model.fan_on ? "ON" : "OFF"),
                 model.fan_cooling ? UiTheme::kWarn
                                   : (model.fan_on ? UiTheme::kOk : UiTheme::kNeutral));
  bool fan_visible = model.fan_on || (model.fan_cooling && blink_state_);
  UiIcons::DrawFan(canvas, x1 + kDeviceCellW / 2, kIconCy,
                   fan_visible ? UiTheme::kOk : UiTheme::kMuted,
                   UiTheme::kPanel, model.fan_on ? fan_angle_deg_ : 0.0f);

  // --- Electric heating. Green when it is on, like every other running thing:
  // amber here would read as a warning, and a heater doing its job is not one.
  DrawDeviceCell(canvas, x2,
                 model.heat_source == HEAT_SOURCE_DEHUMIDIFIER ? "DESHU." : "CHAUFF.",
                 !model.electric_enabled ? "DESACT." : (model.electric_on ? "ON" : "OFF"),
                 !model.electric_enabled ? UiTheme::kMuted
                 : (model.electric_on ? UiTheme::kOk : UiTheme::kNeutral));
  uint16_t heat_color = model.electric_enabled && model.electric_on
                            ? UiTheme::kOk
                            : UiTheme::kMuted;
  UiIcons::DrawHeat(canvas, x2 + kDeviceCellW / 2, kIconCy, heat_color,
                    UiTheme::kPanel);
  if (!model.electric_enabled)
  {
    UiIcons::DrawSlash(canvas, x2 + kDeviceCellW / 2, kIconCy, kHeatSlashR,
                       UiTheme::kMuted);
  }

  // --- The two registers.
  //
  // Each carries its own icon — air leaving a box for extraction, the recycling
  // loop for recirculation — where they used to share one drawing of a duct
  // with a vane in it. Two cells side by side showing the same picture is the
  // one thing a status row must not do: it makes the operator read the caption
  // to learn which is which, every time.
  //
  // What the vane's angle used to say, the words below say instead.
  // FormatDamperState writes FERME, OUVERT, OUV./FER. nn% or -- from the
  // measured position, so the opening is still on screen; and an unusable feedback,
  // which the vaneless duct used to signal, raises RECOPIE REGISTRE HS in the
  // hint bar, where it gets a full line rather than an absence to be noticed.
  //
  // Both openings are shown at once: the registers are asymmetric, so one of
  // them does not describe the other, and which leads which is the reading that
  // gives away a jammed vane or a dead feedback wire. The commanded air path is
  // not printed as a word — it is the register whose icon is green, the same
  // green that marks everything else in use.
  char text[16];

  UiTheme::FormatDamperState(model.extraction_position, model.damper_open, text, sizeof(text));
  DrawDeviceCell(canvas, x3, "EXTR.", text,
                 model.extraction_moving ? UiTheme::kWarn
                 : (model.damper_open ? UiTheme::kOk : UiTheme::kNeutral));
  UiIcons::DrawExtraction(canvas, x3 + kDeviceCellW / 2, kIconCy,
                          model.extraction_moving ? UiTheme::kWarn
                          : (model.damper_open ? UiTheme::kOk : UiTheme::kMuted),
                          UiTheme::kPanel);

  // On a dryer with a single register the cell says ABSENT rather than dashes,
  // the same word the hydraulic module gets when it is not answering. Dashes
  // mean "should be reading and is not"; a register the dryer does not have is
  // not a fault and must not look like one.
  bool recycling_fitted = model.damper_count >= 2;
  UiTheme::FormatDamperState(model.recycling_position, !model.damper_open, text, sizeof(text));
  DrawDeviceCell(canvas, x4, "RECIRC.", recycling_fitted ? text : "ABSENT",
                 !recycling_fitted     ? UiTheme::kMuted
                 : model.recycling_moving ? UiTheme::kWarn
                 : (!model.damper_open ? UiTheme::kOk : UiTheme::kNeutral));
  UiIcons::DrawRecycling(canvas, x4 + kDeviceCellW / 2, kIconCy,
                         !recycling_fitted        ? UiTheme::kMuted
                         : model.recycling_moving ? UiTheme::kWarn
                         : (!model.damper_open ? UiTheme::kOk : UiTheme::kMuted),
                         UiTheme::kPanel);
  if (!recycling_fitted)
  {
    UiIcons::DrawSlash(canvas, x4 + kDeviceCellW / 2, kIconCy, kRecycleSlashR,
                       UiTheme::kMuted);
  }

  canvas.pushSprite(0, kDeviceY);
  canvas.deleteSprite();
}

void TftDisplay::DrawHintBar(const DisplayModel &model)
{
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kWidth, kHintH))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kPanelSunken);
  canvas.drawFastHLine(0, 0, kWidth, UiTheme::kBorder);

  // The mock-up leaves this band empty on the dashboard — it only carries the
  // key hints on the menu. Giving it the alarms costs nothing and buys them a
  // full 320 px line of their own, where the header corner they used to share
  // could only ever hold the shortest of them.
  static_assert(kLongestHint * kMono12BAdvance <= kWidth,
                "hint bar text is wider than the screen");

  // One band, one alarm: Dryer::FaultReason() has already ranked them, so the
  // renderer only translates. Each line names the cause and what it costs,
  // because a red LED on its own has the operator guessing — and every one of
  // these now refuses a start, which is exactly the thing that needs saying.
  const char *hint = nullptr;
  switch (static_cast<DryerFault>(model.fault))
  {
    case DryerFault::kAirflowBlocked:
      hint = "REGISTRES FERMES - PAS DE CIRCULATION";
      break;
    case DryerFault::kDamperFeedback:
      hint = "RECOPIE REGISTRE HS - DEMARRAGE BLOQUE";
      break;
    // Was "CHAUFFAGE BLOQUE", which is still true and no longer the headline:
    // the heat going off is now the opening move of a purge that ends the batch
    // at SENSOR_SESSION_TIMEOUT_MS. The operator has under a minute to reseat a
    // connector, and the line has to say that rather than describe a steady
    // state the dryer is not in.
    case DryerFault::kSensorStale:
      hint = "SONDE INJECTION HS - ARRET IMMINENT";
      break;
    // The way out of this one is the menu, not a spanner, so the line says so:
    // a dryer with no hydraulic fitted is meant to have the source switched off
    // under Sources, and until it is, the module counts as missing.
    case DryerFault::kHydraulicOffline:
      hint = "HYDRAULIQUE INJOIGNABLE - VOIR SOURCES";
      break;
    case DryerFault::kNone:
      break;
  }

  if (hint != nullptr)
  {
    DrawText(canvas, &Mono12B, UiTheme::kDanger, TC_DATUM, hint, kWidth / 2,
             UiLayout::CapTop(kHintH / 2, kMono12BBaseline, kMono12BCapHeight));
  }

  canvas.pushSprite(0, kHintY);
  canvas.deleteSprite();
}

void TftDisplay::DrawFanIcon(const DisplayModel &model)
{
  // Just the disc: a 30x30 canvas costs 1.8 KB and a negligible slice of the
  // shared SPI bus, against 46 KB and ~9 ms for the whole device row.
  TFT_eSprite canvas(&tft_);
  if (!CreateCanvas(canvas, kFanBox, kFanBox))
  {
    return;
  }
  canvas.fillSprite(UiTheme::kPanel);

  bool fan_visible = model.fan_on || (model.fan_cooling && blink_state_);
  uint16_t fan_color = fan_visible ? UiTheme::kOk : UiTheme::kMuted;
  UiIcons::DrawFan(canvas, kFanBox / 2, kFanBox / 2, fan_color, UiTheme::kPanel,
                   model.fan_on ? fan_angle_deg_ : 0.0f);

  canvas.pushSprite(kMargin + kDeviceCellW / 2 - kFanBox / 2,
                    kDeviceY + kIconCy - kFanBox / 2);
  canvas.deleteSprite();
}
