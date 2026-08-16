// Only the hardware-free decoder is compiled in; RotaryEncoder itself needs
// pin reads and interrupts.
#include "../../../src/QuadratureDecoder.cpp"

uint32_t g_test_millis = 0;
