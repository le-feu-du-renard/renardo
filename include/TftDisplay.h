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
//   status band  y 168..239  fan, electric, damper
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
  static constexpr int16_t kFanCx   = 46;
  static constexpr int16_t kFanCy   = 26;
  static constexpr int16_t kFanR    = 18;
  static constexpr int16_t kFanBox  = 2 * kFanR + 4;

  // Change detection, one predicate per region.
  bool StatusBarChanged(const DisplayModel &model) const;
  bool MeasurementChanged(const DisplayModel &model) const;
  bool SetpointChanged(const DisplayModel &model) const;
  bool HydraulicChanged(const DisplayModel &model) const;
  bool BandChanged(const DisplayModel &model) const;
};

#endif // TFT_DISPLAY_H
