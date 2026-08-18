#include "../../../src/AirDamper.cpp"
#include "../../../src/DamperFeedback.cpp"

// Backing store for the shim clock declared in test/unit/support/Arduino.h.
uint32_t g_test_millis = 0;
