// Command output bring-up — built by `pio run -e output_test -t upload -t monitor`.
//
// Drives the three command outputs — fan, damper, electric heating — one at a
// time, so each BC337 stage can be measured at the connector before any load is
// wired to it. This is step 4 of the bring-up order in HARDWARE.md, the step
// where a polarity mistake is caught while it still costs nothing.
//
// Answers, in the order the faults actually happen:
//   idle level wrong at boot     -> the stage is wired the other way round and
//                                   the load is energised for the two seconds
//                                   between power-up and pinMode(). On a fan or
//                                   a heater that is not a cosmetic fault, and
//                                   it is the reason this test reads the pins
//                                   before it drives them
//   collector never moves        -> no base current: check the 1 kOhm base
//                                   resistor, the emitter to the common ground,
//                                   and the BC337's pinout — it is E-B-C, the
//                                   opposite of a BC547, in the same package
//   collector moves the wrong way-> OUT_*_ACTIVE_LOW in config.h does not match
//                                   the stage; the printed GPIO level says which
//                                   way the pin is being driven
//   Vce above ~0.4 V when on     -> the transistor is out of saturation: the
//                                   coil draws more than the base resistor was
//                                   sized for. Drop 1 kOhm to 680 Ohm and
//                                   measure again
//   the wrong contactor clicks   -> two command wires are swapped; only one
//                                   output is ever driven at a time here, and
//                                   the test names it before driving it
//
// It drives the *production* OutputDriver, so the polarity applied here is the
// one the firmware applies. What it adds is what the firmware hides: the resting
// level of each command pin before it is ever driven, the GPIO level behind each
// logical state, and the rule that only one output moves at a time.
//
// One at a time is deliberate. The method in HARDWARE.md is to measure at the
// connector with nothing else moving, and on a dryer whose loads are already
// wired it also keeps the electric heating from being held on with the fan
// stopped. Press 'm' to allow several at once, when that is genuinely what is
// being tested.

#include <Arduino.h>

#include "config.h"
#include "Logger.h"
#include "OutputDriver.h"

namespace
{

  OutputDriver g_fan_output(OUT_FAN_PIN, OUT_FAN_ACTIVE_LOW, "fan");
  OutputDriver g_damper_output(OUT_DAMPER_PIN, OUT_DAMPER_ACTIVE_LOW, "damper");
  OutputDriver g_electric_output(OUT_ELECTRIC_PIN, OUT_ELECTRIC_ACTIVE_LOW, "electric");

  // Long enough to walk to the cabinet and put a probe on a collector, short
  // enough that nothing is left energised while the round-robin is unattended.
  constexpr uint32_t kRoundRobinMs = 5000;

  // A contactor held closed by a bring-up tool is easy to forget about. Say so
  // periodically for as long as anything is on.
  constexpr uint32_t kHoldReminderMs = 15000;

  struct Output
  {
    const char    *name;
    const char    *load;
    uint8_t        pin;
    uint8_t        header; // physical pin on the Pico header
    bool           active_low;
    OutputDriver  *driver;
  };

  // In GPIO order, which is also the order they sit on the header.
  Output g_outputs[] = {
      {"fan     ", "ventilation contactor coil", OUT_FAN_PIN, 1,
       OUT_FAN_ACTIVE_LOW, &g_fan_output},
      {"damper  ", "air register relay", OUT_DAMPER_PIN, 2,
       OUT_DAMPER_ACTIVE_LOW, &g_damper_output},
      {"electric", "electric heating contactor coil", OUT_ELECTRIC_PIN, 4,
       OUT_ELECTRIC_ACTIVE_LOW, &g_electric_output},
  };

  constexpr uint8_t kOutputCount = sizeof(g_outputs) / sizeof(g_outputs[0]);

  bool     g_allow_multiple = false;
  bool     g_round_robin = false;
  uint8_t  g_round_robin_index = 0;
  uint32_t g_last_round_robin = 0;
  uint32_t g_last_reminder = 0;

  // The level the pin is driven to for a given command, straight out of the
  // polarity rule in OutputDriver. Printed rather than assumed: the point of the
  // test is to catch a config.h that disagrees with the board.
  const char *GpioLevelFor(const Output &output, bool active)
  {
    return (active != output.active_low) ? "HIGH" : "LOW";
  }

  bool AnyActive()
  {
    for (uint8_t i = 0; i < kOutputCount; i++)
    {
      if (g_outputs[i].driver->IsActive())
      {
        return true;
      }
    }
    return false;
  }

  void PrintWiring()
  {
    Serial.println();
    Serial.println("Each output is one BC337 in common emitter, low side:");
    Serial.println("  GPIO --[1 kOhm]-- base, 10 kOhm base to emitter");
    Serial.println("  emitter -> GND, common with the 24 V supply, mandatory");
    Serial.println("  collector -> coil, coil -> +24 V");
    Serial.println("  1N4007 across collector and +24 V *on the board*, cathode");
    Serial.println("    to +24 V — a diode at the far coil leaves the cable's own");
    Serial.println("    inductance unclamped, and the BC337 is a 45 V part");
    Serial.println();
    Serial.println("The stage does not invert the command: the coil is the load,");
    Serial.println("so GPIO HIGH saturates the transistor and energises it. That is");
    Serial.println("what leaves every output off while the pads are still inputs.");
    Serial.println();

    for (uint8_t i = 0; i < kOutputCount; i++)
    {
      const Output &output = g_outputs[i];
      Serial.printf("%s -> GP%-2u (header %2u)  %s\n", output.name, output.pin,
                    output.header, output.load);
      Serial.printf("           config.h says active %s: on -> GP%u %s, off -> GP%u %s\n",
                    output.active_low ? "LOW" : "HIGH", output.pin,
                    GpioLevelFor(output, true), output.pin,
                    GpioLevelFor(output, false));
    }

    Serial.println();
    Serial.println("Measure at the connector, loads not yet wired: the collector sits");
    Serial.println("at the supply rail when off and below 0.4 V when on. Anything");
    Serial.println("between the two is a transistor out of saturation.");
    Serial.println();
  }

  void PrintHelp()
  {
    Serial.println("Keys: f = fan          d = damper        e = electric heating");
    Serial.println("      0 = all off      a = round-robin, 5 s each");
    Serial.println("      m = allow several at once (off by default)");
    Serial.println("      h = wiring + help");
    Serial.println();
  }

  void PrintState()
  {
    Serial.print("State:");
    for (uint8_t i = 0; i < kOutputCount; i++)
    {
      Serial.printf("  %s %s", g_outputs[i].name,
                    g_outputs[i].driver->IsActive() ? "ON " : "off");
    }
    Serial.println();
  }

  void Set(uint8_t index, bool active)
  {
    Output &output = g_outputs[index];

    // One at a time unless told otherwise: the whole method is to measure one
    // collector with nothing else moving, and on a wired dryer it also keeps the
    // heating from being held on with the fan stopped.
    if (active && !g_allow_multiple)
    {
      for (uint8_t i = 0; i < kOutputCount; i++)
      {
        if (i != index && g_outputs[i].driver->IsActive())
        {
          g_outputs[i].driver->Set(false);
          Serial.printf("%s -> off  (only one output at a time)\n", g_outputs[i].name);
        }
      }
    }

    output.driver->Set(active);
    Serial.printf("%s -> %-3s  GP%u driven %-4s  collector %s\n", output.name,
                  active ? "ON" : "off", output.pin, GpioLevelFor(output, active),
                  active ? "should fall below 0.4 V" : "should return to the rail");

    g_last_reminder = millis();
  }

  void AllOff()
  {
    for (uint8_t i = 0; i < kOutputCount; i++)
    {
      if (g_outputs[i].driver->Set(false))
      {
        Serial.printf("%s -> off\n", g_outputs[i].name);
      }
    }
    Serial.println("All outputs off.");
  }

  void Toggle(uint8_t index)
  {
    Set(index, !g_outputs[index].driver->IsActive());
  }

  // What every command pin does before pinMode() ever runs. Between power-up and
  // setup() the pads are plain inputs, and whatever each BC337 stage makes of
  // that level is what the dryer does for the first two seconds of every boot.
  //
  // On this board that has to be "off" on all three, and the reason it is has
  // nothing to do with the firmware: an undriven RP2040 pad sits low through its
  // default pull-down, a low base means a blocked transistor, and a blocked
  // transistor means an unenergised coil. The measurement is here because that
  // chain is an assumption until something reads the pin back.
  void CheckIdleLevels()
  {
    bool any_energised = false;

    for (uint8_t i = 0; i < kOutputCount; i++)
    {
      const Output &output = g_outputs[i];
      pinMode(output.pin, INPUT);
      delay(5);

      bool high = digitalRead(output.pin) == HIGH;
      bool idle_active = high != output.active_low;
      any_energised = any_energised || idle_active;

      Serial.printf("Boot idle level on GP%-2u (%s): %-4s -> %s\n", output.pin,
                    output.name, high ? "HIGH" : "LOW",
                    idle_active ? "** ENERGISED **" : "off");
    }

    if (any_energised)
    {
      Serial.println();
      Serial.println("  ** A load is energised before the firmware drives its pin.");
      Serial.println("  ** It runs on every reset, for the whole boot window, and no");
      Serial.println("  ** amount of firmware can shorten that. Re-check the stage:");
      Serial.println("  ** with an NPN in common emitter and the coil on the collector,");
      Serial.println("  ** the pad's pull-down must leave the base at 0 V, hence");
      Serial.println("  ** OUT_*_ACTIVE_LOW = false. An added 10 kOhm from base to");
      Serial.println("  ** emitter makes that off state a property of the board.");
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
  Serial.println("  Command outputs — bring-up");
  Serial.println("========================================");

  PrintWiring();

  // Before anything is driven: what each stage does with an undriven pin.
  CheckIdleLevels();

  Logger::Init();

  g_fan_output.Begin();
  g_damper_output.Begin();
  g_electric_output.Begin();

  Serial.println();
  Serial.println("All three outputs are off. Nothing moves until a key is pressed,");
  Serial.println("and only one output at a time unless 'm' says otherwise.");
  Serial.println();
  PrintHelp();
  PrintState();
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
    case 'f':
      Toggle(0);
      break;
    case 'd':
      Toggle(1);
      break;
    case 'e':
      Toggle(2);
      break;
    case '0':
      g_round_robin = false;
      AllOff();
      break;
    case 'a':
      g_round_robin = !g_round_robin;
      Serial.printf("Round-robin %s\n", g_round_robin ? "on, 5 s each" : "off");
      if (g_round_robin)
      {
        g_round_robin_index = 0;
        g_last_round_robin = millis();
        Set(g_round_robin_index, true);
      }
      else
      {
        AllOff();
      }
      break;
    case 'm':
      g_allow_multiple = !g_allow_multiple;
      if (g_allow_multiple)
      {
        Serial.println("Several outputs may now be on at once.");
        Serial.println("  ** On a dryer whose loads are wired, that can hold the");
        Serial.println("  ** electric heating on with the fan stopped. Watch it.");
      }
      else
      {
        Serial.println("Back to one output at a time.");
        AllOff();
      }
      break;
    case 'h':
      PrintWiring();
      PrintHelp();
      PrintState();
      break;
    default:
      break;
    }
    return;
  }

  uint32_t now = millis();

  if (g_round_robin && now - g_last_round_robin >= kRoundRobinMs)
  {
    g_last_round_robin = now;
    g_outputs[g_round_robin_index].driver->Set(false);
    g_round_robin_index = (g_round_robin_index + 1) % kOutputCount;
    Set(g_round_robin_index, true);
  }

  // A contactor held closed by a bring-up tool is easy to walk away from.
  if (!g_round_robin && AnyActive() && now - g_last_reminder >= kHoldReminderMs)
  {
    g_last_reminder = now;
    Serial.print("Still held — ");
    PrintState();
  }
}
