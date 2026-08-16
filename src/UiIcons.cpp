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

void DrawPump(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
              uint16_t color)
{
  canvas.drawCircle(cx, cy, radius, color);
  canvas.drawCircle(cx, cy, radius - 1, color);

  // Impeller: a triangle pointing along the flow.
  Point a = Polar(cx, cy, radius * 0.62f, 0.0f);
  Point b = Polar(cx, cy, radius * 0.62f, 130.0f);
  Point c = Polar(cx, cy, radius * 0.62f, 230.0f);
  canvas.fillTriangle(a.x, a.y, b.x, b.y, c.x, c.y, color);
}

void DrawDamper(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
                uint16_t color, bool open)
{
  // Duct walls.
  canvas.drawFastVLine(cx - radius, cy - radius, radius * 2, color);
  canvas.drawFastVLine(cx + radius, cy - radius, radius * 2, color);

  // Three louvres: flat across the duct when shut, tilted open when extracting.
  float tilt = open ? 55.0f : 0.0f;
  for (int8_t i = -1; i <= 1; i++)
  {
    int16_t y = cy + i * (radius * 0.7f);
    Point left  = Polar(cx, y, radius * 0.85f, 270.0f + tilt);
    Point right = Polar(cx, y, radius * 0.85f, 90.0f + tilt);
    canvas.drawLine(left.x, left.y, right.x, right.y, color);
  }
}

void DrawAntenna(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
                 uint16_t color)
{
  // Mast and base.
  canvas.drawFastVLine(cx, cy - radius * 0.2f, radius * 1.2f, color);
  canvas.drawFastHLine(cx - radius * 0.4f, cy + radius, radius * 0.8f, color);

  // Two chevrons each side, suggesting radiation.
  for (uint8_t ring = 1; ring <= 2; ring++)
  {
    int16_t spread = radius * 0.35f * ring;
    int16_t height = radius * 0.45f * ring;
    canvas.drawLine(cx - spread, cy - radius * 0.2f - height,
                    cx - spread * 0.4f, cy - radius * 0.2f, color);
    canvas.drawLine(cx + spread, cy - radius * 0.2f - height,
                    cx + spread * 0.4f, cy - radius * 0.2f, color);
  }
}

void DrawSlash(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
               uint16_t color)
{
  canvas.drawLine(cx - radius, cy + radius, cx + radius, cy - radius, color);
  canvas.drawLine(cx - radius + 1, cy + radius, cx + radius, cy - radius + 1, color);
}

} // namespace UiIcons
