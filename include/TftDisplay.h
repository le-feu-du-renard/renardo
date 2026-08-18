#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "DisplayModel.h"

// Main screen on the GMT020-02-7P (ST7789 240x320), used in landscape.
//
// Layout, 320 x 240:
//   status bar   y   0..27   elapsed time left at fixed width, phase centred,
//                            LoRa icon right
//   tiles        y  28..115  measurement | setpoint, split at x = 160
//   hydraulic    y 116..167  circulator state and the two water temperatures
//   status band  y 168..239  fan, electric, and the two registers' openings
//
// Elapsed time sits on the left because HH:MM:SS never changes width, while the
// phase name does — centring the one that moves keeps the bar from jittering.
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

  // Region geometry
  static constexpr int16_t kStatusY = 0;
  static constexpr int16_t kStatusH = 28;
  static constexpr int16_t kTileY   = 28;
  static constexpr int16_t kTileH   = 88;
  static constexpr int16_t kTileW   = 160;
  static constexpr int16_t kHydroY  = 116;
  static constexpr int16_t kHydroH  = 52;
  static constexpr int16_t kBandY   = 168;
  static constexpr int16_t kBandH   = 72;

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

  void DrawStatusBar(const DisplayModel &model);
  void DrawMeasurementTile(const DisplayModel &model);
  void DrawSetpointTile(const DisplayModel &model);
  void DrawHydraulicBlock(const DisplayModel &model);
  void DrawStatusBand(const DisplayModel &model);

  // Animation frames repaint only the fan disc, not the whole band: redrawing
  // 320x72 at 12 fps would churn 46 KB of heap per frame and hold the shared
  // SPI bus far longer than the radio can tolerate.
  void DrawFanIcon(const DisplayModel &model);

  // Fan disc position, band-relative and as its own little canvas.
  static constexpr int16_t kFanCx   = 40;
  static constexpr int16_t kFanCy   = 26;
  static constexpr int16_t kFanR    = 18;
  static constexpr int16_t kFanBox  = 2 * kFanR + 4;

  // Electric heating column, band-relative.
  static constexpr int16_t kHeatCx  = 120;

  // Register panel: two stacked rows, each "LABEL [bar] nn%".
  //
  // The two registers are asymmetric, so both openings are on screen at once and
  // as numbers — which one leads the other is the useful reading, and a bar
  // alone cannot be compared to a second bar precisely enough. The bar stays
  // beside each number for a glance, and is drawn permanently rather than only
  // during travel: an opening with no bar would look like a missing reading.
  //
  // 150 px wide, which is what is left once the fan and the electric heating
  // have their 80 px columns.
  static constexpr int16_t kRegX      = 164; // left edge, labels start here
  static constexpr int16_t kRegRight  = 314; // right edge, percentages end here
  static constexpr int16_t kRegBarX   = 234;
  static constexpr int16_t kRegBarW   = 40;
  static constexpr int16_t kRegBarH   = 8;
  static constexpr int16_t kRegRow1Cy = 22;  // band-relative row centres
  static constexpr int16_t kRegRow2Cy = 50;

  // One register's row. `should_be_open` is where the single command wants this
  // register to end up, which is what colours the label and the bar.
  void DrawRegisterRow(TFT_eSprite &canvas, int16_t cy, const char *label,
                       float position, bool moving, bool should_be_open);

  // Change detection, one predicate per region.
  bool StatusBarChanged(const DisplayModel &model) const;
  bool MeasurementChanged(const DisplayModel &model) const;
  bool SetpointChanged(const DisplayModel &model) const;
  bool HydraulicChanged(const DisplayModel &model) const;
  bool BandChanged(const DisplayModel &model) const;
};

#endif // TFT_DISPLAY_H
