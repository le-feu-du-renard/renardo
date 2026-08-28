#include "../../../src/SessionManager.cpp"
#include "../../../src/TemperatureManager.cpp"
#include "../../../src/HumidityManager.cpp"
#include "../../../src/ElectricHeater.cpp"
#include "../../../src/AirDamper.cpp"
#include "../../../src/DamperFeedback.cpp"

// Backing store for the shim clock declared in test/unit/support/Arduino.h.
uint32_t g_test_millis = 0;
