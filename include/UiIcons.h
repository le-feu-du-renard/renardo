#ifndef UI_ICONS_H
#define UI_ICONS_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Status icons, drawn procedurally rather than stored as bitmaps.
//
// The fan has to animate, and hand-authoring rotation frames as byte arrays
// costs flash and locks the animation to a fixed number of steps. Computing the
// blades from an angle gives smooth rotation at any speed for a few hundred
// bytes of code, and the same approach keeps every icon scalable to whatever
// size the layout needs.
//
// All icons draw centred on (cx, cy) inside a box of 2*radius.
namespace UiIcons
{

// Three blades rotating with `angle_deg`. Pass a fixed angle to show it stopped.
void DrawFan(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
             uint16_t color, float angle_deg);

// Lightning bolt — electric heating.
void DrawLightning(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
                   uint16_t color);

// A register: a square duct with a vane pivoting inside it, upright when the
// register is shut and swung flat when it is wide open, so the opening is
// legible as a shape before the percentage next to it is read.
//
// `opening` is a percentage; NAN draws the duct with no vane at all, which is
// how a register with no usable feedback tells itself apart from a shut one.
void DrawDamper(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
                float opening, uint16_t frame_color, uint16_t vane_color);

// Four bars of increasing height — LoRa signal strength. `bars` of them are
// filled with `color`, the rest outlined in `dim_color`; zero bars means the
// link is down and the whole icon is dim.
void DrawSignalBars(TFT_eSprite &canvas, int16_t left, int16_t bottom,
                    uint8_t bars, uint16_t color, uint16_t dim_color);

// Diagonal bar across an icon: the function exists but is unavailable or
// switched off. Drawn over whatever icon was just rendered.
void DrawSlash(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
               uint16_t color);

} // namespace UiIcons

#endif // UI_ICONS_H
