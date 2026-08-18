#ifndef MONO_FONTS_H
#define MONO_FONTS_H

// The interface's typefaces, in one include.
//
// All five are monospace bitmaps generated from Hack Bold by
// tools/make_gfx_font.py. They exist because the fonts built into TFT_eSPI do
// not work for this panel: only the GLCD font is monospace and it is far too
// coarse, while fonts 2, 4 and 6 are proportional, so a measurement shifts
// sideways every time its last digit changes — the most distracting defect
// there is on a screen that is glanced at.
//
// The scale is one step above the design mock-up's, and bold throughout. The
// mock-up was drawn to be read on a monitor at 2.5x; transposed literally onto
// a 2 inch panel its 7 and 8 px captions come out about a millimetre tall,
// which is below the size anyone can read at arm's length. See
// tools/make_gfx_font.py for the arithmetic.
//
// Every glyph in a given font occupies the same cell, so the width of a string
// is exactly its length times k<Font>Advance and nothing needs measuring at
// runtime. Each header also carries k<Font>Height, k<Font>Baseline and
// k<Font>CapHeight; UiLayout::CapTop() below is what turns those into a y that
// optically centres a line of capitals and digits.
//
// Where each is used:
//   Mono12B  captions, unit labels, the hint bar
//   Mono14   menu rows at rest
//   Mono14B  the clock, state pills, strip values, the selected menu row
//   Mono18B  the setpoint figures
//   Mono26B  the injection figures

#include <TFT_eSPI.h>

#include "fonts/Mono12B.h"
#include "fonts/Mono14.h"
#include "fonts/Mono14B.h"
#include "fonts/Mono18B.h"
#include "fonts/Mono26B.h"

namespace UiLayout
{

// y to pass to drawString with TL_DATUM so that a line of capitals and digits
// sits optically centred on `center_y`.
//
// Centring on the cell instead would sit the text visibly low: the cell is tall
// enough to clear braces and brackets, which reach higher than any letter or
// digit the interface ever prints.
constexpr int16_t CapTop(int16_t center_y, int16_t baseline, int16_t cap_height)
{
  return static_cast<int16_t>(center_y + cap_height / 2 - baseline);
}

} // namespace UiLayout

#endif // MONO_FONTS_H
