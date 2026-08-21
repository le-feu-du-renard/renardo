#ifndef UI_ICONS_H
#define UI_ICONS_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Status icons, rasterised from the Material Design set that Hack Nerd Font
// carries — the same font file the interface text is generated from, so the
// icons cost no new asset, no new dependency and no download.
//
// They used to be built out of triangles and circles instead. The argument for
// that was the fan: it has to rotate, and authoring rotation frames as byte
// arrays by hand is miserable work that locks the animation to whatever number
// of steps one has the patience for. Generating them answers it — the five
// frames below come out of tools/make_icons.py, not out of a text editor — and
// what was paid for procedural drawing was visible: angular blades, a stepped
// rim, and one single glyph doing duty for both registers because drawing a
// second one by hand was not worth the trouble.
//
// Each icon is a 4-bit alpha mask, blended between `background` and `color` as
// it is drawn. That is where the smooth edges come from, and it is why
// `background` has to be the colour actually underneath — the card fill in the
// device row, the header fill behind the eco badge. Passing the wrong one
// leaves a halo in the wrong colour around every icon.
//
// All icons draw centred on (cx, cy). Their sizes are settled at generation, so
// unlike the procedural ones they take no radius.
namespace UiIcons
{

// Two icons' boxes, the only ones a caller has to know: the fan because the
// animation gives it a sprite of its own and has to size it, and the leaf
// because the eco badge sets a label beside it and has to place it. Both are
// checked against the generated bitmaps in the implementation, so neither can
// drift from what make_icons.py produced.
constexpr int16_t kFanBox = 26;
constexpr int16_t kEcoBox = 13;

// Ventilation. `angle_deg` picks the rotation frame — pass a fixed angle to
// show the fan stopped, or a running one to make it turn.
void DrawFan(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
             uint16_t background, float angle_deg);

// Electric heating: a lightning bolt.
void DrawHeat(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
              uint16_t background);

// The extraction register: air leaving the dryer.
void DrawExtraction(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
                    uint16_t background);

// The recirculation register: air going round again.
void DrawRecycling(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
                   uint16_t background);

// Eco mode: a leaf, for the header badge.
void DrawEco(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
             uint16_t background);

// Diagonal bar across an icon: the function exists but is unavailable or
// switched off. Drawn over whatever icon was just rendered, and the one icon
// still drawn rather than generated — two diagonal lines have nothing to gain
// from a bitmap, and being separate is what lets it cross any of the others.
void DrawSlash(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
               uint16_t color);

} // namespace UiIcons

#endif // UI_ICONS_H
