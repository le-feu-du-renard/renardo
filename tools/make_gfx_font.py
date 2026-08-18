#!/usr/bin/env python3
"""Rasterise a TrueType font into a TFT_eSPI GFXfont header.

The built-in TFT_eSPI fonts are unusable for this panel's interface: only the
GLCD font (6x8) is monospace, and it is far too coarse, while fonts 2, 4 and 6
are proportional. A proportional font makes a measurement jitter sideways every
time it is refreshed -- 41.9 and 42.3 are not the same width -- which on a
screen read from two metres away is the most visible defect there is.

The glyphs produced here are deliberately laid out on a *uniform* box: every
glyph gets the same width, the same height and the same offset from the
baseline. That is more bytes than a tightly-cropped font, but it makes text
placement exact rather than approximate:

  * the width of a string is always len(string) * xAdvance, so a value can be
    right-aligned or centred by arithmetic instead of by measurement;
  * TFT_eSPI derives its vertical datums from the tallest glyph in the whole
    font, so making every glyph the same height is what stops a string of
    digits from sitting at a different height than a string with a bracket in
    it.

Usage:
    python3 tools/make_gfx_font.py            # regenerate every font below
    python3 tools/make_gfx_font.py --proof    # also write a PNG proof sheet

The generated headers are committed, so the firmware builds without Python.
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow is required: python3 -m pip install Pillow")

# ASCII only. A GFXfont covers one contiguous range of code points, so reaching
# Latin-1 for the three accented labels the interface would like would take the
# glyph count from 95 to 224 and force a UTF-8 to Latin-1 transcode on every
# string. The interface uses unaccented labels instead, as the rest of the
# firmware already does.
FIRST_CHAR = 0x20
LAST_CHAR = 0x7E

# The one character the interface needs and ASCII does not have is the degree
# sign. Rather than widen the range to Latin-1 for a single glyph, the tilde --
# which no label in this firmware uses -- carries it. UiTheme::kDegree is the
# only place that spells the substitution out, so no layout code contains a
# bare "\x7E".
SUBSTITUTIONS = {0x7E: "°"}

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(REPO_ROOT, "include", "fonts")

# Hack is a terminal monospace very close to the JetBrains Mono of the design
# mock-up, and it ships with macOS developer tooling, so no download is needed.
FONT_DIR = os.path.expanduser("~/Library/Fonts")
REGULAR = os.path.join(FONT_DIR, "HackNerdFontMono-Regular.ttf")
BOLD = os.path.join(FONT_DIR, "HackNerdFontMono-Bold.ttf")

# (header name, ttf path, em size in pixels).
#
# NOT the mock-up's CSS sizes. The mock-up was drawn to be looked at on a
# monitor at 2.5x, and its 7 and 8 px captions transposed literally onto the
# real panel come out about a millimetre tall: the GMT020 is a 2 inch part, so
# 320 x 240 lands on 40.6 x 30.5 mm and one pixel is 0.127 mm. ISO 9241 wants
# roughly 2.3 mm of cap height to be read comfortably at 40 cm, which is 18 px
# here, so every size below is one step up from the design and set in the bold
# cut. That buys back legibility the mock-up's own scale cannot give at this
# physical size.
#
# Bold throughout, including the captions: at 12 px Hack Bold still has open
# counters, and weight does more for legibility at this pixel count than any
# choice of typeface does.
FONTS = [
    ("Mono12B", BOLD, 12),     # captions, unit labels, hint bar
    ("Mono14", REGULAR, 14),   # menu rows at rest
    ("Mono14B", BOLD, 14),     # clock, state pills, values, selected menu row
    ("Mono18B", BOLD, 18),     # setpoint figures
    ("Mono26B", BOLD, 26),     # injection figures
]


def glyph_char(code):
    """The character actually drawn for a code point, substitutions applied."""
    return SUBSTITUTIONS.get(code, chr(code))


def measure(font, size):
    """Return (advance, box_top, box_height) for the whole character range.

    box_top is the offset of the top of the box from the baseline, negative
    upwards, in the sign convention GFXglyph.yOffset uses.
    """
    # Rendered on a canvas large enough that no glyph can be clipped, with the
    # baseline origin placed at (size, 2 * size).
    #
    # Mode "1" is what makes this legible rather than merely small: it puts
    # FreeType into monochrome mode, where the hinting instructions snap stems
    # and bowls onto the pixel grid. Rendering in greyscale and thresholding
    # instead -- which is the obvious thing to do -- leaves stems landing half
    # way across a pixel boundary, so at 12 px some come out one pixel wide and
    # others two, and letters stop looking like themselves.
    pad = size * 2
    canvas = Image.new("1", (size * 4, size * 4), 0)
    draw = ImageDraw.Draw(canvas)

    advance = 0
    ink_top = None
    ink_bottom = None

    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        char = glyph_char(code)
        advance = max(advance, int(round(font.getlength(char))))

        canvas.paste(0, (0, 0, canvas.width, canvas.height))
        draw.text((size, pad), char, font=font, fill=255, anchor="ls")
        box = canvas.getbbox()
        if box is None:
            continue  # the space, which has no ink at all
        top = box[1] - pad
        bottom = box[3] - pad
        ink_top = top if ink_top is None else min(ink_top, top)
        ink_bottom = bottom if ink_bottom is None else max(ink_bottom, bottom)

    if ink_top is None:
        raise ValueError("font produced no ink at this size")

    # Cap height, measured on a capital H. The cell is tall enough to clear
    # braces and brackets, which reach higher than any letter, so centring text
    # on the cell would sit it visibly low. Layout centres on the caps instead.
    canvas.paste(0, (0, 0, canvas.width, canvas.height))
    draw.text((size, pad), "H", font=font, fill=255, anchor="ls")
    cap_box = canvas.getbbox()
    cap_height = pad - cap_box[1]

    return advance, ink_top, ink_bottom - ink_top, cap_height


def rasterise(font, size, advance, box_top, box_height):
    """Render every glyph into the shared box, as rows of packed bits."""
    glyphs = []
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        cell = Image.new("1", (advance, box_height), 0)
        draw = ImageDraw.Draw(cell)
        # The baseline sits at -box_top inside the cell, which is what puts
        # every glyph on the same baseline within its uniform box.
        draw.text((0, -box_top), glyph_char(code), font=font, fill=255,
                  anchor="ls")

        # Already bilevel: any non-zero pixel is ink.
        bits = []
        pixels = cell.load()
        for y in range(box_height):
            for x in range(advance):
                bits.append(1 if pixels[x, y] else 0)

        # GFX bitmaps are a bit stream with no per-row padding: rows run
        # straight into each other.
        data = bytearray()
        accumulator = 0
        count = 0
        for bit in bits:
            accumulator = (accumulator << 1) | bit
            count += 1
            if count == 8:
                data.append(accumulator)
                accumulator = 0
                count = 0
        if count:
            data.append(accumulator << (8 - count))

        glyphs.append(bytes(data))

    return glyphs


def emit(name, glyphs, advance, box_top, box_height, cap_height):
    """Write the C header for one font."""
    lines = []
    lines.append("// Generated by tools/make_gfx_font.py -- do not edit by hand.")
    lines.append("//")
    lines.append("// Uniform %dx%d cells, baseline %d px from the top of the cell."
                 % (advance, box_height, -box_top))
    lines.append("")
    lines.append("#ifndef FONT_%s_H" % name.upper())
    lines.append("#define FONT_%s_H" % name.upper())
    lines.append("")
    lines.append("#include <TFT_eSPI.h>")
    lines.append("")

    blob = bytearray()
    offsets = []
    for glyph in glyphs:
        offsets.append(len(blob))
        blob.extend(glyph)

    lines.append("const uint8_t %sBitmaps[] PROGMEM = {" % name)
    for start in range(0, len(blob), 12):
        chunk = blob[start:start + 12]
        lines.append("  " + " ".join("0x%02X," % b for b in chunk))
    lines.append("};")
    lines.append("")

    lines.append("const GFXglyph %sGlyphs[] PROGMEM = {" % name)
    for index, code in enumerate(range(FIRST_CHAR, LAST_CHAR + 1)):
        char = glyph_char(code)
        shown = char if char not in ('\\', "'") else "\\" + char
        note = " (substituted)" if code in SUBSTITUTIONS else ""
        lines.append("  { %5d, %3d, %3d, %3d, %3d, %3d },  // 0x%02X '%s'%s"
                     % (offsets[index], advance, box_height, advance, 0,
                        box_top, code, shown, note))
    lines.append("};")
    lines.append("")

    lines.append("const GFXfont %s PROGMEM = {" % name)
    lines.append("  (uint8_t *)%sBitmaps," % name)
    lines.append("  (GFXglyph *)%sGlyphs," % name)
    lines.append("  0x%02X, 0x%02X, %d" % (FIRST_CHAR, LAST_CHAR, box_height))
    lines.append("};")
    lines.append("")

    # The layout code needs these to place text by arithmetic rather than by
    # calling textWidth() on every string.
    lines.append("// Cell metrics, so a caller can position text without measuring it.")
    lines.append("// Baseline is measured from the top of the cell; TFT_eSPI's TL_DATUM")
    lines.append("// puts the top of the cell on the y passed to drawString.")
    lines.append("static constexpr int16_t k%sAdvance   = %d;" % (name, advance))
    lines.append("static constexpr int16_t k%sHeight    = %d;" % (name, box_height))
    lines.append("static constexpr int16_t k%sBaseline  = %d;" % (name, -box_top))
    lines.append("static constexpr int16_t k%sCapHeight = %d;" % (name, cap_height))
    lines.append("")
    lines.append("#endif // FONT_%s_H" % name.upper())
    lines.append("")

    path = os.path.join(OUTPUT_DIR, "%s.h" % name)
    with open(path, "w") as handle:
        handle.write("\n".join(lines))

    return path, len(blob)


def proof_sheet(entries):
    """Render every generated font as it will appear, for a visual check."""
    sample = "0123456789 INJECTION 42.3~C -- OUV. 65% ARRET"
    height = sum(entry["box_height"] + 8 for entry in entries) + 8
    width = max(entry["advance"] * len(sample) for entry in entries) + 8
    sheet = Image.new("L", (width, height), 0)

    y = 4
    for entry in entries:
        for index, char in enumerate(sample):
            code = ord(char)
            if not FIRST_CHAR <= code <= LAST_CHAR:
                continue
            glyph = entry["glyphs"][code - FIRST_CHAR]
            for row in range(entry["box_height"]):
                for column in range(entry["advance"]):
                    bit_index = row * entry["advance"] + column
                    byte = glyph[bit_index // 8]
                    if byte & (0x80 >> (bit_index % 8)):
                        sheet.putpixel(
                            (4 + index * entry["advance"] + column, y + row), 255)
        y += entry["box_height"] + 8

    # Next to the script rather than in include/, which holds headers only.
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "font-proof.png")
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(path)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--proof", action="store_true",
                        help="also write a magnified PNG of every glyph set")
    arguments = parser.parse_args()

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    entries = []
    total = 0
    for name, path, size in FONTS:
        if not os.path.exists(path):
            sys.exit("missing font file: %s" % path)

        font = ImageFont.truetype(path, size)
        advance, box_top, box_height, cap_height = measure(font, size)
        glyphs = rasterise(font, size, advance, box_top, box_height)
        header, blob_size = emit(name, glyphs, advance, box_top, box_height,
                                 cap_height)
        total += blob_size

        print("%-8s %2dpx  cell %dx%-2d  baseline %2d  cap %2d  %5d bytes  %s"
              % (name, size, advance, box_height, -box_top, cap_height,
                 blob_size, os.path.relpath(header, REPO_ROOT)))

        entries.append({"advance": advance, "box_height": box_height,
                        "glyphs": glyphs})

    print("total bitmap data: %d bytes" % total)

    if arguments.proof:
        print("proof sheet: %s" % os.path.relpath(proof_sheet(entries), REPO_ROOT))


if __name__ == "__main__":
    main()
