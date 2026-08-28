#include "UiIcons.h"

#include "icons/IconBitmaps.h"

namespace
{

// One icon's bitmap, resolved from the generated constants. Rows are packed
// two pixels to a byte, high nibble first, and padded to a whole byte, so the
// stride is not derivable from the width alone for an odd-width icon.
struct Icon
{
  const uint8_t *alpha;
  uint8_t        box;
  uint8_t        stride;
};

// Mix `color` into `background` at `alpha`/15, channel by channel in RGB565.
//
// Rounding is deliberately left as truncation: the alternative costs three
// additions per pixel to move an edge pixel half a level, on a palette whose
// darkest step is already below what the panel resolves.
uint16_t Blend(uint16_t background, uint16_t color, uint8_t alpha)
{
  const uint8_t inverse = 15 - alpha;

  const uint16_t red = (((background >> 11) & 0x1F) * inverse +
                        ((color >> 11) & 0x1F) * alpha) / 15;
  const uint16_t green = (((background >> 5) & 0x3F) * inverse +
                          ((color >> 5) & 0x3F) * alpha) / 15;
  const uint16_t blue = ((background & 0x1F) * inverse +
                         (color & 0x1F) * alpha) / 15;

  return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

void Draw(TFT_eSprite &canvas, const Icon &icon, int16_t cx, int16_t cy,
          uint16_t color, uint16_t background)
{
  const int16_t left = cx - icon.box / 2;
  const int16_t top  = cy - icon.box / 2;

  for (uint8_t row = 0; row < icon.box; row++)
  {
    const uint8_t *line = icon.alpha + row * icon.stride;

    for (uint8_t column = 0; column < icon.box; column++)
    {
      const uint8_t packed = pgm_read_byte(line + column / 2);
      const uint8_t alpha =
          (column & 1) ? (packed & 0x0F) : (packed >> 4);

      // Fully transparent pixels are the majority of every icon, and skipping
      // them rather than writing the background back is both faster and what
      // lets the slash overlay sit on top without a box around it.
      if (alpha == 0)
      {
        continue;
      }

      canvas.drawPixel(left + column, top + row,
                       alpha == 0x0F ? color
                                     : Blend(background, color, alpha));
    }
  }
}

} // namespace

namespace UiIcons
{

void DrawFan(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
             uint16_t background, float angle_deg)
{
  static_assert(kFanBox == kIconFanBox,
                "the fan sprite size no longer matches the generated bitmap");

  // The glyph has four-fold rotational symmetry, so the frames span a quarter
  // turn and every angle folds into it — which is what makes five frames
  // enough for a fan that turns forever. See tools/make_icons.py.
  constexpr float kSpanDeg = 90.0f;

  float folded = fmodf(angle_deg, kSpanDeg);
  if (folded < 0.0f)
  {
    folded += kSpanDeg;
  }

  uint8_t frame = static_cast<uint8_t>(folded * kIconFanFrames / kSpanDeg);
  if (frame >= kIconFanFrames)
  {
    frame = kIconFanFrames - 1;
  }

  const Icon icon = {kIconFanAlpha + frame * kIconFanBox * kIconFanStride,
                     kIconFanBox, kIconFanStride};
  Draw(canvas, icon, cx, cy, color, background);
}

void DrawHeat(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
              uint16_t background)
{
  const Icon icon = {kIconHeatAlpha, kIconHeatBox, kIconHeatStride};
  Draw(canvas, icon, cx, cy, color, background);
}

void DrawExtraction(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
                    uint16_t background)
{
  const Icon icon = {kIconExtractAlpha, kIconExtractBox, kIconExtractStride};
  Draw(canvas, icon, cx, cy, color, background);
}

void DrawRecycling(TFT_eSprite &canvas, int16_t cx, int16_t cy, uint16_t color,
                   uint16_t background)
{
  const Icon icon = {kIconRecycleAlpha, kIconRecycleBox, kIconRecycleStride};
  Draw(canvas, icon, cx, cy, color, background);
}

void DrawSlash(TFT_eSprite &canvas, int16_t cx, int16_t cy, int16_t radius,
               uint16_t color)
{
  canvas.drawLine(cx - radius, cy + radius, cx + radius, cy - radius, color);
  canvas.drawLine(cx - radius + 1, cy + radius, cx + radius, cy - radius + 1, color);
}

} // namespace UiIcons
