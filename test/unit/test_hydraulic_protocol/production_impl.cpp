// The wire format is hardware-free, so the whole file compiles on the host.
#include "../../../src/HydraulicProtocol.cpp"

// Backing store for the shim clock declared in test/unit/support/Arduino.h.
// The protocol never calls millis() — it has no notion of time at all — but
// config.h pulls the shim in and the symbol has to exist somewhere.
uint32_t g_test_millis = 0;
