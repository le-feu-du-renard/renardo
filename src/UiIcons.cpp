#include "UiIcons.h"

namespace
{

constexpr float kDegToRad = 3.14159265f / 180.0f;

struct Point
{
  int16_t x;
  int16_t y;
};

// Polar offset from a centre, angles measured clockwise from twelve o'clock so
// the drawing code reads the way the icon looks on screen.
Point Polar(int16_t cx, int16_t cy, float radius, float angle_deg)
{
  float a = angle_deg * kDegToRad;
  return Point{static_cast<int16_t>(cx + radius * sinf(a)),
               static_cast<int16_t>(cy - radius * cosf(a))};
}

} // namespace

namespace UiIcons
{

void DrawFan(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
             uint16_t color, float angle_deg)
{
  // Three blades 120 degrees apart. Each is a triangle running from the hub to
  // a chord near the rim, swept back so the direction of rotation reads.
  for (uint8_t blade = 0; blade < 3; blade++)
  {
    float base = angle_deg + blade * 120.0f;
    Point hub  = Polar(cx, cy, radius * 0.18f, base);
    Point tip1 = Polar(cx, cy, radius * 0.95f, base - 20.0f);
    Point tip2 = Polar(cx, cy, radius * 0.75f, base + 22.0f);
    canvas.fillTriangle(hub.x, hub.y, tip1.x, tip1.y, tip2.x, tip2.y, color);
  }

  canvas.fillCircle(cx, cy, radius * 0.20f, color);
  canvas.drawCircle(cx, cy, radius, color);
}

void DrawLightning(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
                   uint16_t color)
{
  float w = radius * 0.55f;
  float h = radius * 0.95f;

  // Two triangles meeting at the waist give the classic bolt without needing a
  // polygon fill.
  canvas.fillTriangle(cx + w * 0.6f, cy - h,
                      cx - w, cy + h * 0.15f,
                      cx + w * 0.1f, cy + h * 0.15f, color);
  canvas.fillTriangle(cx - w * 0.6f, cy + h,
                      cx + w, cy - h * 0.15f,
                      cx - w * 0.1f, cy - h * 0.15f, color);
}

void DrawDamper(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
                float opening, uint16_t frame_color, uint16_t vane_color)
{
  canvas.drawRect(cx - radius, cy - radius, 2 * radius, 2 * radius, frame_color);

  // No feedback: an empty duct. Drawing a vane at some default angle would be
  // asserting a position nothing has measured, and a shut register is exactly
  // what an operator would read it as.
  if (isnan(opening))
  {
    return;
  }

  // Shut is upright, wide open is flat: the vane sweeps a quarter turn, and the
  // eye reads the angle long before it reads the percentage beside it.
  float clamped = opening < 0.0f ? 0.0f : (opening > 100.0f ? 100.0f : opening);
  float angle   = clamped * 0.9f; // 0..100 % over 0..90 degrees

  float span = radius - 2;
  Point a = Polar(cx, cy, span, angle);
  Point b = Polar(cx, cy, span, angle + 180.0f);

  // Two parallel lines rather than one, so the vane keeps its weight against
  // the frame at every angle; a single-pixel diagonal all but disappears.
  canvas.drawLine(a.x, a.y, b.x, b.y, vane_color);
  canvas.drawLine(a.x + 1, a.y, b.x + 1, b.y, vane_color);
}

void DrawSlash(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
               uint16_t color)
{
  canvas.drawLine(cx - radius, cy + radius, cx + radius, cy - radius, color);
  canvas.drawLine(cx - radius + 1, cy + radius, cx + radius, cy - radius + 1, color);
}

} // namespace UiIcons
