// BackoffGate is header-only and hardware-free, so there is no .cpp to pull in.
// This file exists to supply the shim clock's backing store, as every other
// suite's production_impl.cpp does.

#include <cstdint>

// Backing store for the shim clock declared in test/unit/support/Arduino.h.
uint32_t g_test_millis = 0;
