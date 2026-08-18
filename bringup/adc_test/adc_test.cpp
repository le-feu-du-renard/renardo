// Raw ADC readout — built by `pio run -e adc_test -t upload -t monitor`.
//
// Nothing but analogRead() on one pin, printed as a count and as the voltage
// that count means. No damper, no calibration, no channel table, and
// deliberately not a single project header: when the question is whether the
// ADC agrees with a multimeter, every layer between the pad and the number is
// a layer that could be lying.
//
// Put the meter's probes on the same node and compare the volts column. They
// should agree within a few millivolts. If they do not, one of the two
// instruments is wrong, and this sketch is small enough to be trusted first.
//
// The min and max of each burst are printed alongside because a stable average
// hides a noisy input, and a noisy input is its own diagnosis: a missing
// decoupling capacitor, a floating pin, or a source impedance far too high.

#include <Arduino.h>

// The pin to watch. GP26/27/28 are the only ADC-capable pins on the Pico, and
// GP28 carries the RTC's I2C in this build, so 26 and 27 are the useful ones.
// Hardcoded rather than taken from config.h so this file depends on nothing.
constexpr uint8_t kPin = 26;

// The ADC's full-scale reference. On the Pico this is the 3V3 rail through a
// filter, so it is nominal rather than precise — if the volts column is out by
// a percent or so, this is the first thing to suspect.
constexpr float kVref = 3.3f;

constexpr uint8_t kSamples = 16;
constexpr uint32_t kIntervalMs = 500;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  analogReadResolution(12);

  Serial.println();
  Serial.printf("Raw ADC on GP%u, 12 bits, full scale %.2f V\n", kPin, kVref);
  Serial.println("Compare the volts column against a meter on the same node.");
  Serial.println();
}

void loop()
{
  uint32_t sum = 0;
  uint16_t lo = 0xFFFF;
  uint16_t hi = 0;

  for (uint8_t i = 0; i < kSamples; i++)
  {
    uint16_t raw = analogRead(kPin);
    sum += raw;
    if (raw < lo)
      lo = raw;
    if (raw > hi)
      hi = raw;
  }

  uint16_t mean = static_cast<uint16_t>(sum / kSamples);
  Serial.printf("raw %4u   %6.3f V   (min %4u  max %4u  spread %u)\n",
                mean, mean * kVref / 4095.0f, lo, hi, hi - lo);

  delay(kIntervalMs);
}
