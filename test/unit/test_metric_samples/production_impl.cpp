#include "../../../src/ExtensionProtocol.cpp"
#include "../../../src/MetricSamples.cpp"

// Backing store for the shim clock declared in test/unit/support/Arduino.h.
uint32_t g_test_millis = 0;
