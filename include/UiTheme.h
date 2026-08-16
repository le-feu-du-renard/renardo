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

constexpr uint16_t kBackground   = 0x0000; // black
constexpr uint16_t kPanel        = 0x18E3; // very dark grey, tile fill
constexpr uint16_t kBorder       = 0x39E7; // grey, tile outline
constexpr uint16_t kLabel        = 0x8410; // mid grey, captions
constexpr uint16_t kValue        = 0xFFFF; // white, measurements
constexpr uint16_t kSetpoint     = 0x07FF; // cyan, targets
constexpr uint16_t kActive       = 0x07E0; // green, running
constexpr uint16_t kInactive     = 0x4208; // dark grey, idle
constexpr uint16_t kHeat         = 0xFD20; // orange, electric heating
constexpr uint16_t kWater        = 0x049F; // blue, hydraulic
constexpr uint16_t kAlarm        = 0xF800; // red, faults
constexpr uint16_t kWarning      = 0xFFE0; // yellow, degraded

// Writes `seconds` as HH:MM:SS into `out`, which must hold at least 9 bytes.
// Fixed width by construction: the field never reflows as the session runs.
void FormatDuration(uint32_t seconds, char *out, size_t length);

// Writes a measurement with one decimal, or "--.-" when it is not a number,
// so a missing probe reads as missing instead of as zero.
void FormatValue(float value, const char *unit, char *out, size_t length);

} // namespace UiTheme

#endif // UI_THEME_H
