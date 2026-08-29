#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "DisplayModel.h"
#include "UiIcons.h"

// Main screen on the GMT020-02-7P (ST7789 240x320), used in landscape.
//
// Laid out from the design mock-up in design_handoff_sechoir_tft, transposed
// from CSS to drawing primitives. Six horizontal bands, 320 x 240:
//
//   progress   y   0..  3   phase progress, full width, phase-coloured
//   header     y   4.. 23   phase pill left, clock centred
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

  // One gutter below the header, like every other gap between bands. The
  // progress bar above it is the deliberate exception: it is a full-bleed
  // hairline in the phase colour, and sitting flush is what makes it read as
  // the top edge of the header rather than a band of its own.
  static constexpr int16_t kCardY = kHeaderY + kHeaderH + kGutter;

  // The measurement card is the wider of the two: it carries four figures and
  // two gauges against the setpoint card's two figures.
  static constexpr int16_t kInletX  = kMargin;
  static constexpr int16_t kInletW  = 172;
  static constexpr int16_t kTargetX = kInletX + kInletW + kGutter;
  static constexpr int16_t kTargetW = kWidth - kMargin - kTargetX;

  static constexpr int16_t kStripCells = 3;
  static constexpr int16_t kStripCellW =
      (kWidth - 2 * kMargin - (kStripCells - 1) * kGutter) / kStripCells;

  static constexpr int16_t kDeviceCells = 4;
  static constexpr int16_t kDeviceCellW =
      (kWidth - 2 * kMargin - (kDeviceCells - 1) * kGutter) / kDeviceCells;

  // --- Vertical layout, in two variants -------------------------------------
  //
  // A dryer with no hydraulic module has no water loop to report, and the
  // middle strip would be a third of the usable height spent on three cells
  // saying nothing. So the strip is not blanked, it is *gone*, and the 38 px it
  // occupied are given back to the two cards and the device row.
  //
  // Only the vertical figures vary. Every width — the cards, the cell pitch,
  // the margins — is the same under both, which is what leaves every
  // static_assert about text fitting its cell true as written, with nothing to
  // duplicate per variant.
  //
  // The figures inside a card and a device cell are derived from its height
  // rather than fixed, so growing one moves its contents with it. That is the
  // difference between two layouts and two copies of a layout.
  struct Layout
  {
    bool    has_strip;
    int16_t card_h;
    int16_t strip_y;
    int16_t strip_h;
    int16_t device_y;
    int16_t device_h;

    // Card contents, card-relative. The caption sits near the top under both
    // variants; the figures centre in whatever is left below it, which is what
    // makes a taller card read as a roomier one rather than a top-heavy one.
    constexpr int16_t CardLabelY() const { return 20; }
    constexpr int16_t FigureCenterY() const { return (26 + card_h) / 2; }

    // Device cell contents, cell-relative. The extra height is shared out down
    // the cell rather than pooled at the bottom, so the icon, the caption and
    // the state word stay evenly spread instead of huddling at the top over a
    // gap.
    // Every gap between bands is exactly one gutter — not merely "they do not
    // overlap", which is what this used to check and what let the header sit
    // eight pixels off the cards while every other gap was four. Asserting the
    // rhythm rather than the absence of a collision is the difference between a
    // layout that is even and one that happens not to be broken.
    //
    // Takes its bounds as arguments because the enclosing class is still
    // incomplete where the assertions below are written.
    constexpr bool Fits(int16_t card_y, int16_t hint_y, int16_t gutter) const
    {
      return (has_strip ? (card_y + card_h + gutter == strip_y &&
                           strip_y + strip_h + gutter == device_y)
                        : (card_y + card_h + gutter == device_y)) &&
             device_y + device_h + gutter == hint_y;
    }

    constexpr int16_t Extra() const { return device_h - 72; }
    constexpr int16_t IconCy() const { return 22 + Extra() / 4; }
    constexpr int16_t DeviceLabelY() const { return 40 + Extra() / 2; }
    constexpr int16_t DeviceStateY() const { return 61 + (Extra() * 3) / 4; }
  };

  //   progress   0..  3
  //   header     4.. 23
  //   cards     28..107 | 28..126
  //   strip    112..145 | -
  //   devices  150..221 | 131..221
  //   hint     226..239
  static constexpr Layout kLayoutHydraulic{true, 80, 112, 34, 150, 72};
  static constexpr Layout kLayoutNoHydraulic{false, 99, 0, 0, 131, 91};

  // Both are checked, in RenderMain: a nested type's constexpr members are not
  // usable in a constant expression until the enclosing class is complete, and
  // every other assertion in this file already lives in the function that
  // depends on it.

  static constexpr const Layout &LayoutFor(const DisplayModel &model)
  {
    return model.hydraulic_enabled ? kLayoutHydraulic : kLayoutNoHydraulic;
  }

  // The variant this frame is being drawn in. Held rather than threaded through
  // every Draw* signature because DrawFanIcon is also reached from the
  // animation path, which has no model of its own to ask.
  const Layout *layout_ = &kLayoutHydraulic;

  // Inside a card: text starts here, and the same inset is the top of the
  // caption line.
  static constexpr int16_t kPad = 8;

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

  // Fan disc size, as its own canvas. Two pixels of margin around the icon: the
  // sprite is filled with the card colour before the icon goes down, and the
  // margin is what guarantees the previous frame's blades are painted out
  // wherever the new ones are not. Its cell-relative centre comes from the
  // layout, since a taller cell puts it lower.
  static constexpr int16_t kFanBox  = UiIcons::kFanBox + 4;

  // Radii for the slash overlay, which is drawn rather than generated and so
  // has to be told how far to reach. Half of each icon's box.
  static constexpr int16_t kHeatSlashR    = 12;
  static constexpr int16_t kRecycleSlashR = 11;

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
