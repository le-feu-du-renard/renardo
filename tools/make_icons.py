#!/usr/bin/env python3
"""Rasterise icon glyphs into a 4-bit alpha header for the interface.

The status icons used to be drawn procedurally, out of triangles and circles,
because the fan has to rotate and hand-authoring rotation frames as byte arrays
is miserable work. The result read as what it was: angular blades, a stepped
rim, and one single glyph doing duty for both registers because drawing a
second one by hand was not worth the trouble.

Nothing here is hand-drawn. The source is the same font the interface text
comes from -- Hack Nerd Font, already installed for tools/make_gfx_font.py --
which carries the whole Material Design Icons set, 11 696 glyphs, drawn by
people who draw icons. Picking one is a matter of naming a code point, and the
rotation frames the fan needs are generated rather than written, which is what
makes them cheap enough to have.

Two things about the rendering matter more than the choice of glyph:

  * 4 bits of alpha, not 1 bit. What made the old fan ugly was aliasing, and a
    thresholded bitmap of a rotated glyph is if anything worse -- the blades
    come apart into stair steps. The alpha is blended against the card colour
    at draw time, so the edges are smooth at no cost in flash worth counting.

  * Supersampling. Each glyph is rendered eight times larger than the box it
    will occupy, rotated at that size, and only then reduced. Rotating a 26 px
    image directly destroys it.

Note the font is HackNerdFont-Bold, *not* the -Mono cut the text fonts use: the
Mono cut squeezes every icon into one single-width text cell and flattens its
proportions. Icons are not text and do not want that treatment.

Usage:
    python3 tools/make_icons.py            # regenerate the header
    python3 tools/make_icons.py --proof    # also write a PNG proof sheet

The generated header is committed, so the firmware builds without Python.
"""

import argparse
import math
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow is required: python3 -m pip install Pillow")

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(REPO_ROOT, "include", "icons")
OUTPUT_NAME = "IconBitmaps.h"

FONT = os.path.expanduser("~/Library/Fonts/HackNerdFont-Bold.ttf")

# How much larger than the target box each glyph is rendered before being
# rotated and reduced. Eight is well past the point where more stops showing.
SUPERSAMPLE = 8

# The fan's animation, and why five frames covers it.
#
# The Material fan glyph has four-fold rotational symmetry -- turned a quarter
# turn it lands back on itself, to within a handful of pixels out of 676 -- so
# the frames only have to span 90 degrees, not 360. TftDisplay advances the
# angle by 18 degrees per animation tick, and 90 / 18 is exactly 5, so five
# frames give a seamless loop in which every frame is actually used. Six frames
# at 15 degrees would waste one: the 18 degree step would never land on it.
FAN_FRAMES = 5
FAN_SPAN_DEG = 90.0

# (C name, Nerd Font code point, box in pixels, rotation frames).
#
# The names below are the Nerd Font glyph names, which is what to search the
# cheat sheet for if one of these ever needs replacing:
#   md-fan, md-flash, md-export, md-recycle, md-leaf
#
# Box sizes come from the layout, not from the glyphs: a device cell is 74 px
# wide and its icon band runs from the top of the cell to the caption at y=40,
# so 23 to 26 px is what there is. The eco badge shares the 19 px header band
# with the clock, hence 13.
ICONS = [
    ("Fan",     0xF0210, 26, FAN_FRAMES),  # ventilation, spins
    ("Heat",    0xF0241, 24, 1),           # electric heating, a bolt
    ("Extract", 0xF0207, 23, 1),           # extraction: air leaving a box
    ("Recycle", 0xF044C, 23, 1),           # recirculation: the recycling loop
    ("Eco",     0xF032A, 13, 1),           # eco mode badge, a leaf
]

# Proof sheet only: the real colours the panel uses, so the sheet shows what
# will be on the screen rather than white on black. From include/UiTheme.h.
PROOF_PANEL = (0x12, 0x1b, 0x1d)  # kPanel
PROOF_INK = (0x5b, 0xd9, 0x7f)    # kOk


def ink_radius(image):
    """Distance from the centre to the farthest inked pixel."""
    cx = image.width / 2.0
    cy = image.height / 2.0
    pixels = image.load()
    largest = 0.0
    for y in range(image.height):
        for x in range(image.width):
            if pixels[x, y] > 8:
                largest = max(largest, math.hypot(x + 0.5 - cx, y + 0.5 - cy))
    return largest


def master(code_point, box, rotates):
    """Render one glyph, supersampled, centred on a square canvas.

    A glyph that will be rotated is scaled by its inked *radius* rather than by
    its bounding box: the fan's ink is a disc, so fitting the disc to the box
    keeps it full size, where fitting the diagonal of its bounding box would
    shrink it by a factor of 1.4 for corners that hold nothing.
    """
    side = box * SUPERSAMPLE
    em = side * 2

    font = ImageFont.truetype(FONT, em)
    canvas = Image.new("L", (em * 3, em * 3), 0)
    ImageDraw.Draw(canvas).text((em, em * 2), chr(code_point), font=font,
                                fill=255, anchor="ls")
    bounds = canvas.getbbox()
    if bounds is None:
        raise ValueError("U+%05X produced no ink" % code_point)
    glyph = canvas.crop(bounds)

    if rotates:
        radius = ink_radius(glyph)
        if radius <= 0:
            raise ValueError("U+%05X has no measurable radius" % code_point)
        scale = (side / 2.0) / radius
    else:
        scale = float(side) / max(glyph.width, glyph.height)

    glyph = glyph.resize((max(1, round(glyph.width * scale)),
                          max(1, round(glyph.height * scale))),
                         Image.LANCZOS)

    sheet = Image.new("L", (side, side), 0)
    sheet.paste(glyph, ((side - glyph.width) // 2, (side - glyph.height) // 2))
    return sheet


def frames(sheet, box, count):
    """Reduce the supersampled master to `count` rotated frames of `box` px."""
    step = FAN_SPAN_DEG / count if count > 1 else 0.0
    out = []
    for index in range(count):
        image = sheet
        if step:
            # Negative: Pillow turns anticlockwise, and the fan should read as
            # turning the way a fan turns.
            image = sheet.rotate(-index * step, resample=Image.BICUBIC)
        out.append(image.resize((box, box), Image.LANCZOS))
    return out


def pack(image):
    """One frame as 4-bit alpha, high nibble first, rows padded to a byte."""
    pixels = image.load()
    data = bytearray()
    for y in range(image.height):
        row = bytearray()
        pending = None
        for x in range(image.width):
            alpha = (pixels[x, y] * 15 + 127) // 255
            if pending is None:
                pending = alpha
            else:
                row.append((pending << 4) | alpha)
                pending = None
        if pending is not None:
            row.append(pending << 4)
        data.extend(row)
    return bytes(data)


def emit(entries):
    """Write the C header holding every icon."""
    lines = []
    lines.append("// Generated by tools/make_icons.py -- do not edit by hand.")
    lines.append("//")
    lines.append("// 4 bits of alpha per pixel, high nibble first, each row")
    lines.append("// padded to a whole byte. Blended against the card colour by")
    lines.append("// UiIcons::DrawIcon, which is the only thing that reads this.")
    lines.append("")
    lines.append("#ifndef ICON_BITMAPS_H")
    lines.append("#define ICON_BITMAPS_H")
    lines.append("")
    lines.append("#include <Arduino.h>")
    lines.append("")
    lines.append("namespace UiIcons")
    lines.append("{")
    lines.append("")

    total = 0
    for entry in entries:
        name = entry["name"]
        blob = bytearray()
        for frame in entry["packed"]:
            blob.extend(frame)
        total += len(blob)

        lines.append("// U+%05X, %d x %d%s" %
                     (entry["code_point"], entry["box"], entry["box"],
                      ", %d rotation frames" % len(entry["packed"])
                      if len(entry["packed"]) > 1 else ""))
        lines.append("const uint8_t kIcon%sAlpha[] PROGMEM = {" % name)
        for start in range(0, len(blob), 12):
            chunk = blob[start:start + 12]
            lines.append("  " + " ".join("0x%02X," % b for b in chunk))
        lines.append("};")
        lines.append("")

    lines.append("// Every icon's geometry, so the drawing code holds no literals.")
    width = max(len(entry["name"]) for entry in entries) + len("kIconStride")
    for entry in entries:
        name = entry["name"]

        def declare(suffix, value):
            symbol = "kIcon%s%s" % (name, suffix)
            lines.append("static constexpr uint8_t %-*s = %d;"
                         % (width, symbol, value))

        declare("Box", entry["box"])
        declare("Stride", entry["stride"])
        if len(entry["packed"]) > 1:
            declare("Frames", len(entry["packed"]))
    lines.append("")
    lines.append("} // namespace UiIcons")
    lines.append("")
    lines.append("#endif // ICON_BITMAPS_H")
    lines.append("")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    path = os.path.join(OUTPUT_DIR, OUTPUT_NAME)
    with open(path, "w") as handle:
        handle.write("\n".join(lines))

    return path, total


def proof_sheet(entries):
    """Composite every frame over the card colour, magnified, for a look."""
    zoom = 6
    gap = 6
    width = gap + max(len(entry["frames"]) * (entry["box"] * zoom + gap)
                      for entry in entries)
    height = gap + sum(entry["box"] * zoom + gap for entry in entries)
    sheet = Image.new("RGB", (width, height), PROOF_PANEL)

    y = gap
    for entry in entries:
        x = gap
        for frame in entry["frames"]:
            tile = Image.new("RGB", (entry["box"], entry["box"]), PROOF_PANEL)
            pixels = frame.load()
            for row in range(entry["box"]):
                for column in range(entry["box"]):
                    alpha = pixels[column, row] / 255.0
                    tile.putpixel((column, row), tuple(
                        int(PROOF_PANEL[c] + (PROOF_INK[c] - PROOF_PANEL[c]) * alpha)
                        for c in range(3)))
            tile = tile.resize((entry["box"] * zoom, entry["box"] * zoom),
                               Image.NEAREST)
            sheet.paste(tile, (x, y))
            x += entry["box"] * zoom + gap
        y += entry["box"] * zoom + gap

    # Next to the script rather than in include/, which holds headers only.
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "icon-proof.png")
    sheet.save(path)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--proof", action="store_true",
                        help="also write a magnified PNG of every icon")
    arguments = parser.parse_args()

    if not os.path.exists(FONT):
        sys.exit("missing font file: %s" % FONT)

    entries = []
    for name, code_point, box, count in ICONS:
        rendered = frames(master(code_point, box, count > 1), box, count)
        entries.append({
            "name": name,
            "code_point": code_point,
            "box": box,
            "stride": (box + 1) // 2,
            "frames": rendered,                          # images, for the proof
            "packed": [pack(frame) for frame in rendered],  # bytes, for the header
        })

    header, total = emit(entries)
    for entry in entries:
        count = len(entry["packed"])
        print("%-8s U+%05X  %2d x %-2d  %d frame%s  %5d bytes"
              % (entry["name"], entry["code_point"], entry["box"], entry["box"],
                 count, "s" if count > 1 else " ",
                 count * len(entry["packed"][0])))
    print("total alpha data: %d bytes  %s"
          % (total, os.path.relpath(header, REPO_ROOT)))

    if arguments.proof:
        print("proof sheet: %s" % os.path.relpath(proof_sheet(entries),
                                                  REPO_ROOT))


if __name__ == "__main__":
    main()
