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
//   raw ADC frozen               -> that register's 2-10 V feedback wire is
//                                   absent or its divider is not fed; the
//                                   firmware would report "no position"
//   both openings track together -> one actuator is not wired to travel the
//                                   other way: the registers are complementary,
//                                   so their openings must mirror, not follow
//   raw ADC moves, percent NAN   -> the calibration in config.h is degenerate
//                                   for this actuator: capture both end stops
//                                   from the menu
//   percent stuck at 0 or 100    -> the raw value has run past the calibrated
//     with "off scale" beside it     end stop, which the percentage clamps and
//                                   so cannot show. Expected before the first
//                                   calibration: the defaults are computed from
//                                   the divider, not measured on this actuator.
//                                   The raw value beside it is the one to keep.
//
// It drives the *production* OutputDriver and AirDamper, so the polarity
// applied here and the position printed here are the ones the firmware would
// use. What it adds is what they hide: the resting level of the command pin
// before it is ever driven, the raw ADC behind the percentage, and a running
// check that the feedback actually moved during the last cycle.
//
// The cycle is the actuator's own travel time, 150 s, so each half ends with the
// vane against a stop and the raw column resting on the value the calibration
// wants. Press 's' for a 30 s cycle when only the relay and the wiring are in
// question — the vane then stays mid-course and never settles, which is fine for
// hearing the relay but useless for reading an end stop.

#include <Arduino.h>

#include "config.h"
#include "AirDamper.h"
#include "DamperFeedback.h"
#include "Logger.h"
#include "OutputDriver.h"

namespace
{

  OutputDriver g_damper_output(OUT_DAMPER_PIN, OUT_DAMPER_ACTIVE_LOW, "damper");
  AirDamper g_damper;

  // The LM24A-SR's measured travel, end to end. Holding each command for exactly
  // that long is what makes the last sample of each half a real end-stop reading.
  constexpr uint32_t kTravelIntervalMs = 170000;
  // Relay-and-wiring check: fast enough to be watched, too fast to reach a stop.
  constexpr uint32_t kQuickIntervalMs = 30000;
  constexpr uint32_t kSampleMs = 1000;

  // Below this the ADC has not moved at all between two switches, which is the
  // signature of a feedback wire that is not connected.
  constexpr uint16_t kFeedbackDeadband = 20;

  // A sample this close to either rail is not a measurement. A pin sitting on 0
  // or on full scale has nothing driving it at all.
  constexpr uint16_t kRailLow = 20;
  constexpr uint16_t kRailHigh = 4000;

  // Counts still free above a cycle's highest sample before the ADC clips. The
  // divider is meant to put the open stop at 2.48 V, 800 counts clear of full
  // scale; the bench reads 3.10 V because the divider's ground sits above the
  // Pico's, which leaves ~250. Below this margin the top of the travel is one
  // ground disturbance away from being flattened at 4095 — and a clipped reading
  // looks exactly like a register sitting at 100 %.
  constexpr uint16_t kClipMargin = 400;

  // A reading counts as settled once it has stayed inside this band for this
  // long. Both are chosen against the bench noise of about +-6 counts.
  //
  // This exists because a fixed hold time is not proof of arrival. A vane still
  // creeping when the sample is taken yields an end-stop value that is simply
  // wrong, and nothing about the number says so — it looks like a perfectly good
  // reading, and calibrating on it puts a permanent error into every opening the
  // dryer will ever display.
  constexpr uint16_t kSettleBand = 15;
  constexpr uint32_t kSettleMs = 20000;

  uint32_t g_interval_ms = kTravelIntervalMs;
  uint32_t g_last_toggle = 0;
  uint32_t g_last_sample = 0;
  bool g_auto = true;

  // One entry per register. The range each has seen since the last switch is what
  // tells a live feedback wire from a dead one: a span of nothing means that
  // register's divider is not fed, whatever the other one is doing.
  struct Channel
  {
    const char *name;
    uint8_t pin;
    DamperFeedback *feedback;
    bool enabled;
    uint16_t cycle_min;
    uint16_t cycle_max;
    uint16_t settle_ref;   // centre of the band the reading is holding in
    uint32_t settle_since; // when it entered that band
    bool settled;
  };

  Channel g_channels[] = {
      {"EXT", DAMPER_EXTRACTION_FEEDBACK_PIN, nullptr, true, 0xFFFF, 0, 0, 0, false},
      // Off until DAMPER_RECYCLING_FITTED says the wire exists. Press '2' to
      // sample it anyway — useful the moment it is landed, before rebuilding.
      {"REC", DAMPER_RECYCLING_FEEDBACK_PIN, nullptr, DAMPER_RECYCLING_FITTED != 0,
       0xFFFF, 0, 0, 0, false},
  };
  constexpr uint8_t kChannelCount = sizeof(g_channels) / sizeof(g_channels[0]);

  bool g_cycle_reported = true;

  uint16_t ReadRaw(uint8_t pin)
  {
    // Same averaging as main.cpp: the RP2040 ADC is noisy and this only feeds a
    // display, so a slow, smooth value is what we want.
    constexpr uint8_t kSamples = 8;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < kSamples; i++)
    {
      sum += analogRead(pin);
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

  void ToggleChannel(uint8_t index)
  {
    Channel &c = g_channels[index];
    c.enabled = !c.enabled;
    c.cycle_min = 0xFFFF;
    c.cycle_max = 0;
    Serial.printf("Channel %s (GP%u): %s\n", c.name, c.pin,
                  c.enabled ? "sampled" : "left alone");
  }

  // Reads each channel once and says which ones look connected.
  //
  // This matters more than it sounds. The RP2040's ADC multiplexes one converter
  // across the channels, and its sample-and-hold carries a little charge from one
  // conversion into the next. A pin with nothing on it presents a very high
  // impedance, so it neither settles nor discharges — and the neighbour sampled
  // straight after it can come back offset by that leftover charge. An unwired
  // channel is therefore not a harmless zero: it can quietly bias a good one.
  void ProbeChannels()
  {
    Serial.println();
    for (uint8_t i = 0; i < kChannelCount; i++)
    {
      Channel &c = g_channels[i];
      uint16_t raw = ReadRaw(c.pin);
      bool railed = raw <= kRailLow || raw >= kRailHigh;

      Serial.printf("Channel %s on GP%u: raw %4u  %s\n", c.name, c.pin, raw,
                    railed ? "** pinned to a rail: nothing connected"
                           : "in range, looks wired");
      if (railed)
      {
        Serial.printf("   press '%u' to stop sampling it — a floating input can\n",
                      i + 1);
        Serial.println("   drag the other channel's reading with it.");
      }
    }
  }

  void PrintWiring()
  {
    Serial.println();
    Serial.printf("Command       -> GP%-2u  (BC337 base through ~1 kOhm, drives both)\n",
                  OUT_DAMPER_PIN);
    Serial.printf("Extraction fb -> GP%-2u  (ADC0, Belimo 2-10 V through the divider)\n",
                  DAMPER_EXTRACTION_FEEDBACK_PIN);
    Serial.printf("Recycling  fb -> GP%-2u  (ADC1, same divider)\n",
                  DAMPER_RECYCLING_FEEDBACK_PIN);
    Serial.println("Ground        -> common with the 24 V supply, mandatory");
    Serial.println();
    Serial.printf("config.h says OUT_DAMPER_ACTIVE_LOW = %s\n",
                  OUT_DAMPER_ACTIVE_LOW ? "true" : "false");
    Serial.printf("  extraction    (open)  -> GP%u %s\n", OUT_DAMPER_PIN, GpioLevelFor(true));
    Serial.printf("  recirculation (closed)-> GP%u %s\n", OUT_DAMPER_PIN, GpioLevelFor(false));
    Serial.printf("Calibration, both registers until each is captured: raw %u .. %u\n",
                  DAMPER_RAW_CLOSED_DEFAULT, DAMPER_RAW_OPEN_DEFAULT);
    Serial.println("The registers are complementary: one opening must climb while the");
    Serial.println("other falls. Both climbing together means an actuator wired the");
    Serial.println("same way round as its partner rather than the opposite way.");
    Serial.println();
  }

  void PrintHelp()
  {
    Serial.println("Keys: o = extraction   c = recirculation   t = toggle now");
    Serial.println("      a = auto on/off  l = 150 s cycle     s = 30 s cycle");
    Serial.println("      1 / 2 = sample the extraction / recycling channel or not");
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
    if (!g_cycle_reported)
    {
      for (uint8_t i = 0; i < kChannelCount; i++)
      {
        Channel &c = g_channels[i];
        if (!c.enabled || c.cycle_max < c.cycle_min)
        {
          continue;
        }
        uint16_t span = c.cycle_max - c.cycle_min;
        Serial.printf("  %s cycle: raw %u..%u (span %u)%s\n", c.name, c.cycle_min,
                      c.cycle_max, span,
                      span < kFeedbackDeadband ? "  ** frozen: feedback wire?" : "");

        if (!c.settled)
        {
          Serial.printf("     ** %s never settled: the vane was still creeping when\n",
                        c.name);
          Serial.println("     ** the command switched, so neither end of this cycle is");
          Serial.println("     ** an end stop. Hold it — 'a' then 'o' or 'c' — and wait");
          Serial.println("     ** for SETTLED before writing anything down.");
        }

        if (c.cycle_max + kClipMargin >= 4095)
        {
          Serial.printf("     ** only %u counts under full scale: the open stop is\n",
                        (unsigned)(4095 - c.cycle_max));
          Serial.println("     ** near clipping. Measure the divider's ground against");
          Serial.println("     ** the Pico's GND — an offset there eats this margin.");
        }
      }
    }
    for (uint8_t i = 0; i < kChannelCount; i++)
    {
      g_channels[i].cycle_min = 0xFFFF;
      g_channels[i].cycle_max = 0;
      g_channels[i].settled = false;
      g_channels[i].settle_since = millis();
    }
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
    uint32_t held_s = (millis() - g_last_toggle) / 1000;
    Serial.printf("    +%2lus", (unsigned long)held_s);

    for (uint8_t i = 0; i < kChannelCount; i++)
    {
      Channel &c = g_channels[i];
      if (!c.enabled)
      {
        continue;
      }
      uint16_t raw = ReadRaw(c.pin);
      c.feedback->SetRawPosition(raw);

      if (raw < c.cycle_min)
        c.cycle_min = raw;
      if (raw > c.cycle_max)
        c.cycle_max = raw;

      // Arrival is decided by the reading going quiet, never by the clock. An
      // end stop is where the vane stops moving, and a fixed hold time is not
      // proof it got there: a value taken while it still creeps looks exactly
      // like a good one, and calibrating on it puts a permanent error into every
      // opening the dryer will ever show.
      uint32_t now = millis();
      int32_t drift = static_cast<int32_t>(raw) - static_cast<int32_t>(c.settle_ref);
      if (drift > static_cast<int32_t>(kSettleBand) ||
          drift < -static_cast<int32_t>(kSettleBand))
      {
        c.settle_ref = raw;
        c.settle_since = now;
        c.settled = false;
      }
      else if (!c.settled && now - c.settle_since >= kSettleMs)
      {
        c.settled = true;
        Serial.printf("\n  %s SETTLED at raw %u — that one is a real end stop\n",
                      c.name, raw);
      }

      float position = c.feedback->GetPositionPercent();
      if (isnan(position))
      {
        Serial.printf("   %s raw %4u  --.-%%        ", c.name, raw);
        continue;
      }

      // GetPositionPercent() clamps to 0..100, so a raw value past an end stop
      // shows up as a flat 0 % or 100 % with no hint of how far past it is. Say
      // so: before the first calibration it is the normal state, and the raw
      // value printed here is exactly what the menu wants.
      uint16_t lo = c.feedback->GetRawClosed();
      uint16_t hi = c.feedback->GetRawOpen();
      if (lo > hi)
      {
        uint16_t swap = lo;
        lo = hi;
        hi = swap;
      }
      const char *scale = (raw < lo || raw > hi) ? " off scale" : "";

      Serial.printf("   %s raw %4u %5.1f%% %-7s%s%s", c.name, raw, position,
                    c.feedback->IsMoving() ? "moving" : "at stop", scale,
                    c.settled ? " SETTLED" : "");
    }
    Serial.println();
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

  // The channels borrow the production AirDamper's two feedbacks, so the
  // calibration and the travel detection printed here are the firmware's.
  g_channels[0].feedback = &g_damper.Extraction();
  g_channels[1].feedback = &g_damper.Recycling();

  // A channel with nothing on it is worse than useless: it reads noise, and its
  // high impedance can bleed into the next conversion of the multiplexed ADC,
  // dragging a perfectly good neighbour off. Say so, and offer to drop it.
  ProbeChannels();

  Serial.println();
  Serial.printf("Switching every %lu s, the actuator's own travel time, so each half\n",
                (unsigned long)(g_interval_ms / 1000));
  Serial.println("ends with the vane on a stop and the raw column resting on the value");
  Serial.println("the calibration wants. Press 's' for a 30 s cycle when only the relay");
  Serial.println("and the wiring are in question.");
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
    case 'o':
      Switch(true);
      break;
    case 'c':
      Switch(false);
      break;
    case 't':
      Switch(!g_damper.IsOpen());
      break;
    case 'a':
      g_auto = !g_auto;
      Serial.printf("Auto switching %s\n", g_auto ? "on" : "off");
      g_last_toggle = millis();
      break;
    case 's':
      g_interval_ms = kQuickIntervalMs;
      Serial.println("Cycle: 30 s — relay and wiring only, no stop is reached");
      break;
    case 'l':
      g_interval_ms = kTravelIntervalMs;
      Serial.println("Cycle: 150 s — a full travel, so each half ends on a stop");
      break;
    case '1':
      ToggleChannel(0);
      break;
    case '2':
      ToggleChannel(1);
      break;
    case 'h':
      PrintWiring();
      PrintHelp();
      break;
    default:
      break;
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
