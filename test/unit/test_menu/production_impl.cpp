// MenuSystem no longer depends on TFT_eSPI, so the navigation and the value
// bindings compile and run on the host.
#include "../../../src/MenuSystem.cpp"
#include "../../../src/DryerSettings.cpp"

uint32_t g_test_millis = 0;
