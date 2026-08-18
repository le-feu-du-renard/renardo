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

// Pump symbol: a circle with an impeller triangle — the circulator.
void DrawPump(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
              uint16_t color);

// Antenna with radiating chevrons — the LoRa link.
void DrawAntenna(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
                 uint16_t color);

// Diagonal bar across an icon: the function exists but is unavailable or
// switched off. Drawn over whatever icon was just rendered.
void DrawSlash(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
               uint16_t color);

} // namespace UiIcons

#endif // UI_ICONS_H
