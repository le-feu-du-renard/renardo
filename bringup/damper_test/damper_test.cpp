// Air damper bring-up — built by `pio run -e damper_test -t upload -t monitor`.
//
// Switches the damper command every 30 s and reports what the actuator does
// about it, so the whole chain — GPIO, BC337 stage, relay, Belimo, feedback
// wire — can be watched on one screen.
//
// Answers, in the order the faults actually happen:
//   idle level wrong at boot     -> the relay is energised before pinMode()
//                                   runs; the BC337 stage is wired the other
//                                   way round and the damper travels on every
//                                   reset
//   relay never clicks           -> no base current: check the ~1 kO base
//                                   resistor, the emitter to the common ground,
//                                   and that the module's ground is the Pico's
//   relay clicks the wrong way   -> OUT_DAMPER_ACTIVE_LOW in config.h does not
//                                   match the stage; the printed GPIO level
//                                   says which way it is being driven
//   raw ADC frozen               -> the 2-10 V feedback wire is absent or its
//                                   divider is not fed; the firmware would
//                                   report "no position" rather than 0 %
//   raw ADC moves, percent NAN   -> the calibration in config.h is degenerate
//                                   for this actuator: capture both end stops
//                                   from the menu
//
// It drives the *production* OutputDriver and AirDamper, so the polarity
// applied here and the position printed here are the ones the firmware would
// use. What it adds is what they hide: the resting level of the command pin
// before it is ever driven, the raw ADC behind the percentage, and a running
// check that the feedback actually moved during the last cycle.
//
// Note on the 30 s period: the LM24A-SR takes about 150 s end to end, so the
// vane deliberately never reaches a stop here — this test proves the command
// and the feedback, not the travel. Press 'l' for a long cycle when the end
// stops are what you want to see.

#include <Arduino.h>

#include "config.h"
#include "AirDamper.h"
#include "Logger.h"
#include "OutputDriver.h"

namespace
{

OutputDriver g_damper_output(OUT_DAMPER_PIN, OUT_DAMPER_ACTIVE_LOW, "damper");
AirDamper    g_damper;

// What the user asked for: one switch every 30 s.
constexpr uint32_t kShortIntervalMs = 30000;
// Long enough for the vane to reach its stop, so the calibration values can be
// read off the raw column.
constexpr uint32_t kLongIntervalMs  = 180000;
constexpr uint32_t kSampleMs        = 1000;

// Below this the ADC has not moved at all between two switches, which is the
// signature of a feedback wire that is not connected.
constexpr uint16_t kFeedbackDeadband = 20;

uint32_t g_interval_ms  = kShortIntervalMs;
uint32_t g_last_toggle  = 0;
uint32_t g_last_sample  = 0;
bool     g_auto         = true;

// Range seen since the last switch — a span of nothing means a dead feedback.
uint16_t g_cycle_min = 0xFFFF;
uint16_t g_cycle_max = 0;
bool     g_cycle_reported = true;

uint16_t ReadRaw()
{
  // Same averaging as main.cpp: the RP2040 ADC is noisy and this only feeds a
  // display, so a slow, smooth value is what we want.
  constexpr uint8_t kSamples = 8;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < kSamples; i++)
  {
    sum += analogRead(DAMPER_FEEDBACK_PIN);
  }
  return static_cast<uint16_t>(sum / kSamples);
}

// The level the pin is driven to for a given command, straight out of the
// polarity rule in OutputDriver. Printed rather than assumed: the point of the
// test is to catch a config.h that disagrees with the board.
const char *GpioLevelFor(bool active)
{
  return (active != OUT_DAMPER_ACTIVE_LOW) ? "HIGH" : "LOW";
}

void PrintWiring()
{
  Serial.println();
  Serial.printf("Command      -> GP%u   (BC337 base through ~1 kOhm)\n", OUT_DAMPER_PIN);
  Serial.printf("Feedback     -> GP%u   (ADC2, Belimo 2-10 V through the divider)\n",
                DAMPER_FEEDBACK_PIN);
  Serial.println("Ground       -> common with the 24 V supply, mandatory");
  Serial.println();
  Serial.printf("config.h says OUT_DAMPER_ACTIVE_LOW = %s\n",
                OUT_DAMPER_ACTIVE_LOW ? "true" : "false");
  Serial.printf("  extraction    (open)  -> GP%u %s\n", OUT_DAMPER_PIN, GpioLevelFor(true));
  Serial.printf("  recirculation (closed)-> GP%u %s\n", OUT_DAMPER_PIN, GpioLevelFor(false));
  Serial.printf("Calibration: raw %u closed .. %u open\n",
                DAMPER_RAW_CLOSED_DEFAULT, DAMPER_RAW_OPEN_DEFAULT);
  Serial.println();
}

void PrintHelp()
{
  Serial.println("Keys: o = extraction   c = recirculation   t = toggle now");
  Serial.println("      a = auto on/off  s = 30 s cycle      l = 180 s cycle");
  Serial.println("      h = wiring + help");
  Serial.println();
}

// The command pin before pinMode() ever runs. Between power-up and setup() the
// pad is a plain input, and whatever the BC337 stage does with that level is
// what the damper does for the first two seconds of every boot. It must be the
// resting state, recirculation.
void CheckIdleLevel()
{
  pinMode(OUT_DAMPER_PIN, INPUT);
  delay(5);
  bool high = digitalRead(OUT_DAMPER_PIN) == HIGH;

  const char *idle_level = high ? "HIGH" : "LOW";
  bool idle_active = high != static_cast<bool>(OUT_DAMPER_ACTIVE_LOW);

  Serial.printf("Boot idle level on GP%u: %s -> damper %s\n",
                OUT_DAMPER_PIN, idle_level,
                idle_active ? "EXTRACTION" : "recirculation");
  if (idle_active)
  {
    Serial.println("  ** The relay is energised before the firmware drives the pin.");
    Serial.println("  ** The damper travels on every reset. Re-check the BC337 stage:");
    Serial.println("  ** with an NPN in common emitter the pad's pull-down must leave");
    Serial.println("  ** the base at 0 V, hence OUT_DAMPER_ACTIVE_LOW = false.");
  }
  Serial.println();
}

void Switch(bool open)
{
  // Report the cycle that is ending before starting the next one: the useful
  // question is whether the feedback moved while the command was held.
  if (!g_cycle_reported && g_cycle_max >= g_cycle_min)
  {
    uint16_t span = g_cycle_max - g_cycle_min;
    Serial.printf("  cycle feedback: raw %u..%u (span %u)%s\n",
                  g_cycle_min, g_cycle_max, span,
                  span < kFeedbackDeadband ? "  ** frozen: feedback wire?" : "");
  }
  g_cycle_min = 0xFFFF;
  g_cycle_max = 0;
  g_cycle_reported = false;

  if (open)
  {
    g_damper.Open();
  }
  else
  {
    g_damper.Close();
  }

  // The click, if there is one, happens here.
  bool changed = g_damper_output.Set(open);

  Serial.printf("[%6lu s] %-13s  GP%u %s%s\n",
                (unsigned long)(millis() / 1000),
                open ? "EXTRACTION" : "recirculation",
                OUT_DAMPER_PIN, GpioLevelFor(open),
                changed ? "" : "  (no change)");

  g_last_toggle = millis();
}

void Sample()
{
  uint16_t raw = ReadRaw();
  g_damper.SetRawPosition(raw);

  if (raw < g_cycle_min) g_cycle_min = raw;
  if (raw > g_cycle_max) g_cycle_max = raw;

  float position = g_damper.GetPositionPercent();
  uint32_t held_s = (millis() - g_last_toggle) / 1000;

  if (isnan(position))
  {
    Serial.printf("    +%2lus  raw %4u  pos --.-%%  (calibration unusable)\n",
                  (unsigned long)held_s, raw);
  }
  else
  {
    Serial.printf("    +%2lus  raw %4u  pos %5.1f%%  %s\n",
                  (unsigned long)held_s, raw, position,
                  g_damper.IsMoving() ? "moving" : "at stop");
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  Air damper — bring-up");
  Serial.println("========================================");

  PrintWiring();

  // Before anything is driven: what the stage does with an undriven pin.
  CheckIdleLevel();

  Logger::Init();
  analogReadResolution(12);
  g_damper_output.Begin();

  Serial.println();
  Serial.printf("Switching every %lu s. Listen for the relay and watch the raw column:\n",
                (unsigned long)(g_interval_ms / 1000));
  Serial.println("it must drift one way, then the other. A 30 s cycle is shorter than");
  Serial.println("the ~150 s travel, so the vane stays mid-course by design — press 'l'");
  Serial.println("for a 180 s cycle when the end-stop values are what you are after.");
  Serial.println();
  PrintHelp();

  Switch(true);
}

void loop()
{
  if (Serial.available() > 0)
  {
    int key = Serial.read();
    while (Serial.available() > 0)
    {
      Serial.read();
    }

    switch (key)
    {
      case 'o': Switch(true); break;
      case 'c': Switch(false); break;
      case 't': Switch(!g_damper.IsOpen()); break;
      case 'a':
        g_auto = !g_auto;
        Serial.printf("Auto switching %s\n", g_auto ? "on" : "off");
        g_last_toggle = millis();
        break;
      case 's':
        g_interval_ms = kShortIntervalMs;
        Serial.println("Cycle: 30 s");
        break;
      case 'l':
        g_interval_ms = kLongIntervalMs;
        Serial.println("Cycle: 180 s — long enough to reach both stops");
        break;
      case 'h': PrintWiring(); PrintHelp(); break;
      default: break;
    }
    return;
  }

  uint32_t now = millis();

  if (now - g_last_sample >= kSampleMs)
  {
    g_last_sample = now;
    Sample();
  }

  if (g_auto && now - g_last_toggle >= g_interval_ms)
  {
    Switch(!g_damper.IsOpen());
  }
}
