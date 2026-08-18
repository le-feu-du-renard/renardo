#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "DisplayModel.h"

// Main screen on the GMT020-02-7P (ST7789 240x320), used in landscape.
//
// Laid out from the design mock-up in design_handoff_sechoir_tft, transposed
// from CSS to drawing primitives. Six horizontal bands, 320 x 240:
//
//   progress   y   0..  3   phase progress, full width, phase-coloured
//   header     y   4.. 23   phase pill left, clock centred, LoRa bars right
//   cards      y  32..107   INJECTION (with gauges) | CONSIGNE
//   strip      y 112..145   hydraulic state, circuit water, tank water
//   devices    y 150..221   fan | electric | extraction | recycling
//   hint       y 226..239   empty, or the sensor alarm
//
// The clock is centred and the phase name is pinned left, the opposite of what
// the layout used to do: with a monospace font the clock never changes width,
// so it is now the one that can be centred without the bar jittering, and the
// phase reads better next to its own coloured dot.
//
// Rendering is region-based: each frame is compared against the previous model
// and only the regions whose contents changed are redrawn, each through a
// sprite so it appears in one push with no flicker. A full-screen sprite would
// need 150 KB and a blocking repaint long enough to matter against the 8 s
// watchdog, so the sprite is created and released per region instead: only one
// region's worth of memory is ever live.
class TftDisplay
{
public:
  TftDisplay();

  void Begin();

  // Redraws whatever changed since the last call. Cheap when nothing moved.
  void RenderMain(const DisplayModel &model);

  // Forces every region to be redrawn on the next RenderMain, after the menu
  // or any other screen has taken over the panel.
  void Invalidate();

  // Rewrites the splash's bottom line with the start-up stage in progress. A
  // stage that hangs — the radio probe above all — then names itself on the
  // panel instead of only in the serial log. No-op once the splash is over.
  void ShowBootStage(const char *stage);

  // Holds the splash for the rest of its minimum on-screen time, then retires
  // it. Nothing is cleared: the first RenderMain repaints the whole panel, so
  // the splash gives way to the interface with no black frame between them.
  void EndSplash();

  TFT_eSPI &GetTft() { return tft_; }

  // Panel geometry in landscape.
  static constexpr int16_t kWidth  = 320;
  static constexpr int16_t kHeight = 240;

  // The hint bar at the foot of the screen belongs to whichever screen is up,
  // so the menu draws its own into the same band.
  static constexpr int16_t kHintY = 226;
  static constexpr int16_t kHintH = 14;

private:
  TFT_eSPI tft_;

  DisplayModel previous_;
  bool         force_redraw_;

  float    fan_angle_deg_;
  uint32_t last_animation_ms_;
  bool     blink_state_;
  uint32_t last_blink_ms_;

  uint32_t splash_started_ms_;
  bool     splash_active_;

  // --- Region geometry ---
  //
  // Transposed from the mock-up's flexbox: a 6 px margin round the content and
  // 4 px gutters between cells, with the cell widths that leaves.

  static constexpr int16_t kMargin = 6;
  static constexpr int16_t kGutter = 4;

  static constexpr int16_t kProgressY = 0;
  static constexpr int16_t kProgressH = 4;

  static constexpr int16_t kHeaderY = 4;
  static constexpr int16_t kHeaderH = 20;

  static constexpr int16_t kCardY = 32;
  static constexpr int16_t kCardH = 76;
  // The measurement card is the wider of the two: it carries four figures and
  // two gauges against the setpoint card's two figures.
  static constexpr int16_t kInletX  = kMargin;
  static constexpr int16_t kInletW  = 172;
  static constexpr int16_t kTargetX = kInletX + kInletW + kGutter;
  static constexpr int16_t kTargetW = kWidth - kMargin - kTargetX;

  // 34 rather than the mock-up's 30: the type is a step larger than the design
  // called for, and a caption over a value needs the extra four pixels not to
  // read as one crowded block.
  static constexpr int16_t kStripY = 112;
  static constexpr int16_t kStripH = 34;
  static constexpr int16_t kStripCells = 3;
  static constexpr int16_t kStripCellW =
      (kWidth - 2 * kMargin - (kStripCells - 1) * kGutter) / kStripCells;

  static constexpr int16_t kDeviceY = 150;
  static constexpr int16_t kDeviceH = 72;
  static constexpr int16_t kDeviceCells = 4;
  static constexpr int16_t kDeviceCellW =
      (kWidth - 2 * kMargin - (kDeviceCells - 1) * kGutter) / kDeviceCells;

  // Inside a card: text starts here, and the same inset is the top of the
  // caption line.
  static constexpr int16_t kPad = 8;

  // Card contents, card-relative. Both cards centre their label on one line and
  // their figures on the other; the two cards set their figures at different
  // sizes, so sharing an optical centre rather than a top edge is what makes
  // the readings look level side by side.
  static constexpr int16_t kCardLabelY  = 20;
  static constexpr int16_t kFigureCenterY = 50;

  // Glyph cells each figure is allowed. A temperature is always "nn.n~" or
  // "--.-~"; a humidity is right-aligned inside the width of "100%".
  static constexpr int16_t kFigureCells  = 5;
  static constexpr int16_t kPercentCells = 4;

  static constexpr uint32_t kAnimationIntervalMs = 80;
  static constexpr uint32_t kBlinkIntervalMs     = 500;

  // One still screen at start-up, held until the interface's first frame, so a
  // wiring fault is distinguishable from a working panel that simply has
  // nothing to display yet. Drawn once and never redrawn: only the stage line
  // moves, everything else stays put for as long as the boot takes.
  void ShowSplash();

  // Splash geometry and timing. The floor only ever comes into play when the
  // whole boot is fast — a radio that answers makes setup short enough for the
  // splash to flash by otherwise.
  static constexpr uint32_t kSplashMinMs = 1500;
  static constexpr int16_t  kSplashStageY = kHeight - 26;
  static constexpr int16_t  kSplashStageH = 18;

  void DrawProgressBar(const DisplayModel &model);
  void DrawHeader(const DisplayModel &model);
  void DrawInletCard(const DisplayModel &model);
  void DrawTargetCard(const DisplayModel &model);
  void DrawStrip(const DisplayModel &model);
  void DrawDevices(const DisplayModel &model);
  void DrawHintBar(const DisplayModel &model);

  // Animation frames repaint only the fan disc, not the whole row: redrawing
  // 320x72 at 12 fps would churn 46 KB of heap per frame and hold the shared
  // SPI bus far longer than the radio can tolerate.
  void DrawFanIcon(const DisplayModel &model);

  // Fan disc position within the first device cell, and as its own canvas.
  static constexpr int16_t kIconCy  = 22; // cell-relative centre of every icon
  static constexpr int16_t kFanR    = 12;
  static constexpr int16_t kFanBox  = 2 * kFanR + 4;

  // One device cell: icon, caption, and a state word under it. `pill_color`
  // carries the meaning — green running, amber transient, red stopped, grey
  // switched off — so the caller decides what the state means and this only
  // lays it out.
  void DrawDeviceCell(TFT_eSprite &canvas, int16_t x, const char *label,
                      const char *state, uint16_t pill_color);

  // One strip cell: caption above, value below.
  void DrawStripCell(TFT_eSprite &canvas, int16_t x, const char *label,
                     const char *value, uint16_t value_color);

  // A card's temperature and humidity, centred as one block of fixed width.
  void DrawFigurePair(TFT_eSprite &canvas, int16_t card_width,
                      const GFXfont *font, int16_t advance, int16_t baseline,
                      int16_t cap_height, uint16_t color, float temperature,
                      float humidity);

  // Change detection, one predicate per region.
  bool ProgressChanged(const DisplayModel &model) const;
  bool HeaderChanged(const DisplayModel &model) const;
  bool InletChanged(const DisplayModel &model) const;
  bool TargetChanged(const DisplayModel &model) const;
  bool StripChanged(const DisplayModel &model) const;
  bool DevicesChanged(const DisplayModel &model) const;
  bool HintChanged(const DisplayModel &model) const;
};

#endif // TFT_DISPLAY_H
