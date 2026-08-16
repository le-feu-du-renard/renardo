// Compile the real production sources into the native test binary.
//
// The native env sets test_build_src = no, so src/ is not built automatically;
// pulling the .cpp files in here keeps the test linked against the actual
// implementation rather than a copy of it.

#include "../../../src/TemperatureManager.cpp"
#include "../../../src/ElectricHeater.cpp"

// Backing store for the shim clock declared in test/unit/support/Arduino.h.
uint32_t g_test_millis = 0;
