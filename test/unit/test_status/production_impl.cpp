// Only the hardware-free half is compiled in; StatusLed itself needs pin
// writes.
#include "../../../src/StatusIndicator.cpp"

// Backing store for the shim clock declared in test/unit/support/Arduino.h.
// StatusIndicator never calls millis() — it takes the instant as an argument —
// but config.h pulls the shim in and the symbol has to exist somewhere.
uint32_t g_test_millis = 0;
