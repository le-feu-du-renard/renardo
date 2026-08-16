#ifndef ARDUINO_TEST_SHIM_H
#define ARDUINO_TEST_SHIM_H

// Minimal Arduino shim for the native test build.
//
// The domain classes (TemperatureManager, SessionManager, HumidityManager, ...)
// are hardware-free but include <Arduino.h> for the usual types and helpers.
// This header supplies just enough of it to compile them on the host, so the
// unit tests exercise the real production code instead of a hand-copied
// simulation that silently drifts out of sync — which is exactly what happened
// to the v3 test suite.
//
// Only the native env sees this file, through `-I test/unit/support`.

#include <cmath>
#include <cstdint>
#include <cstring>

// --- Controllable clock -----------------------------------------------------
// Tests drive time explicitly rather than sleeping.

extern uint32_t g_test_millis;

inline uint32_t millis() { return g_test_millis; }
inline void TestSetMillis(uint32_t ms) { g_test_millis = ms; }
inline void TestAdvanceMillis(uint32_t ms) { g_test_millis += ms; }

// --- Arduino helpers --------------------------------------------------------

#ifndef constrain
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

using std::fabs;
using std::fabsf;
using std::isnan;
using std::lroundf;
using std::roundf;

#endif // ARDUINO_TEST_SHIM_H
