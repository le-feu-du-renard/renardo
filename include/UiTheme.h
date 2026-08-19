#ifndef UI_THEME_H
#define UI_THEME_H

#include <stddef.h>
#include <stdint.h>

// Shared palette and formatting helpers for every screen.
//
// Deliberately free of any TFT_eSPI dependency: colours are plain RGB565 words
// and the formatters are string work, so the parts most likely to be wrong in a
// way the user would see are testable on the host.
//
// Dark background: the dryer sits in an unlit room and the panel is read at a
// glance from a couple of metres, so light text on dark carries further than
// the reverse and stops the screen acting as a lamp at night.
namespace UiTheme
{

// --- Palette ----------------------------------------------------------------
//
// The "cyan" colourway of the design mock-up, converted to RGB565 by keeping
// the top 5, 6 and 5 bits of each channel. The source hex is kept next to each
// value: the conversion is lossy, so the hex is the reference, not the word.
//
// Kept as flat constants rather than a palette struct. A second colourway --
// the mock-up also carries an amber one -- is a matter of editing this block,
// which is cheaper than paying for an indirection at the several hundred call
// sites that would be needed to switch themes at runtime, for a machine that
// lives in one room and is never reconfigured.

// Surfaces.
constexpr uint16_t kBackground   = 0x0882; // #0a1012, screen behind everything
constexpr uint16_t kPanel        = 0x10C3; // #121b1d, card fill
constexpr uint16_t kPanelSunken  = 0x08A3; // #0e1618, header, rails, hint bar
constexpr uint16_t kBorder       = 0x1945; // #1c2b2c, card outline
constexpr uint16_t kAccentDim    = 0x1A48; // #1d4a45, selected row fill

// State colours. Each hue means one thing and only that thing, everywhere on
// both screens — an operator should be able to learn the code once from across
// the room and never have to read the word underneath.
//
//   kDanger   something is broken and wants attention now
//   kWarn     transient or noteworthy, but nothing is wrong
//   kOk       running as intended
//   kNeutral  available and idle — not an error, not an achievement
//   kMuted    switched off in the configuration, or not applicable
//
// The distinction that matters most is kNeutral against kMuted against
// kDanger: a stopped fan, a fan disabled at the menu and a hydraulic module
// that has stopped answering are three different situations, and only the last
// is a fault. Painting all three red — which is what "not ON" invites — trains
// the operator to ignore red.
constexpr uint16_t kDanger       = 0xFACB; // #ff5a5a, faults
constexpr uint16_t kWarn         = 0xFD84; // #ffb020, in transit, editing
constexpr uint16_t kOk           = 0x5ECF; // #5bd97f, running
constexpr uint16_t kNeutral      = 0x5D5C; // #5aa9e6, idle, water readings
constexpr uint16_t kMuted        = 0x6C51; // #6f8a88, disabled, captions

// Emphasis, outside the state code.
constexpr uint16_t kAccent       = 0x3F3A; // #3fe6d4, live figures, cursor
constexpr uint16_t kText         = 0xEFBE; // #e9f4f2, setpoints, the clock

// --- Text -------------------------------------------------------------------

// The degree sign, which ASCII does not have. The generated fonts carry it on
// the tilde, a character no label in this firmware uses; this is the only place
// that spells the substitution out. See tools/make_gfx_font.py.
//
// A macro rather than a constant because most of its uses are inside a literal
// built at compile time — UI_DEGREE "C" — which only works for literals.
#define UI_DEGREE "~"

// Writes `seconds` as HH:MM:SS into `out`, which must hold at least 9 bytes.
// Fixed width by construction: the field never reflows as the session runs.
void FormatDuration(uint32_t seconds, char *out, size_t length);

// Writes a measurement with one decimal, or "--.-" when it is not a number,
// so a missing probe reads as missing instead of as zero.
void FormatValue(float value, const char *unit, char *out, size_t length);

// A temperature with its degree sign, e.g. "51.2~", or "--.-~" when absent.
//
// No "C" after it: everything this machine measures is in Celsius, the sign
// alone is unambiguous, and at the size the figures are set the letter costs a
// whole glyph cell that the cards do not have to spare.
void FormatTemperature(float celsius, char *out, size_t length);

// A value rounded to a whole number, e.g. "38", or "--" when absent. Rounded
// rather than truncated: a humidity reading of 39.8 shown as 39 would sit a
// whole point below every other readout of the same figure.
void FormatRounded(float value, char *out, size_t length);

// The same with a percent sign, e.g. "38%" or "--%".
void FormatPercent(float percent, char *out, size_t length);

// A register's opening as words: "FERME" at 0, "OUVERT" at 100, "OUV. nn%" in
// between, "--" with no usable feedback.
//
// The percentage is deliberately absent at the end stops. It is only useful
// while a register is part way, and a permanent "OUVERT 100%" is two readings
// of the same fact competing for a cell 74 px wide.
void FormatDamperState(float position, char *out, size_t length);

} // namespace UiTheme

#endif // UI_THEME_H
