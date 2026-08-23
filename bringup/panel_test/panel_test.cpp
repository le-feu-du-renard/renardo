// Panel bring-up — built by `pio run -e panel_test -t upload -t monitor`.
//
// Checks the four wires of the operator panel: the START and STOP buttons and
// the green and red status LEDs. They sit on one contiguous block of header
// pins, 16 to 20, with the ground at 18, so a connector fitted one row out puts
// every one of them on the wrong pin at once — which is the first fault below.
//
// Answers, in the order the faults actually happen:
//   a LED lit before pinMode()   -> the LED is wired to a pin that idles high,
//                                   or across the wrong pair. Both pads must be
//                                   dark for the two seconds between power-up
//                                   and setup(), which is why this reads them
//                                   before it drives them
//   both buttons read the same   -> the connector is one row out, or the common
//                                   is not on header 18. The resting level of
//                                   each pin is printed with its name
//   the wrong button answers     -> the two signal wires are swapped. Each press
//                                   is named as it lands, so this takes one press
//                                   each to see
//   a LED never lights           -> mounted the wrong way round. Anode to the
//                                   GPIO, cathode through the resistor to ground:
//                                   an LED in backwards is not a dim LED, it is
//                                   a dark one
//   a LED lights but is washed   -> the series resistor is too large, or the
//                                   green is a high-brightness InGaN part whose
//                                   3.0-3.2 V forward drop leaves nothing across
//                                   330 ohm. Press 'b' and look at it in daylight,
//                                   which is where it has to be readable
//   a button repeats while held  -> the debounce is not doing its job; each press
//                                   must print exactly one line, however long the
//                                   button is held down
//
// It drives the *production* PushButton, StatusLed and StatusIndicator, so a
// press counted here is a press the firmware would count and a pattern shown
// here is the pattern the firmware would show. What it adds is what the firmware
// hides: the resting level of all four pins before anything drives them, and the
// ability to hold one state still while somebody looks at the panel.

#include <Arduino.h>

#include "config.h"
#include "Logger.h"
#include "PushButton.h"
#include "StatusIndicator.h"
#include "StatusLed.h"

namespace
{

  PushButton g_start_button(BTN_START_PIN, "start");
  PushButton g_stop_button(BTN_STOP_PIN, "stop");
  StatusLed  g_status_led(LED_RUN_PIN, LED_FAULT_PIN);

  // Long enough to walk round the panel and look at it, short enough that the
  // whole cycle is seen without waiting.
  constexpr uint32_t kAutoStateMs = 4000;

  // Five appearances, not four: green and red are two independent axes, so a
  // fault over a running session is its own thing to look at and the one most
  // easily got wrong in the driver.
  const PanelState kAllStates[] = {
      {DryerStatus::kStopped, DryerFault::kNone},
      {DryerStatus::kRunning, DryerFault::kNone},
      {DryerStatus::kCooling, DryerFault::kNone},
      {DryerStatus::kStopped, DryerFault::kSensorStale},
      {DryerStatus::kRunning, DryerFault::kSensorStale},
  };
  constexpr uint8_t kStateCount = sizeof(kAllStates) / sizeof(kAllStates[0]);

  PanelState g_state{DryerStatus::kStopped, DryerFault::kNone};
  bool       g_auto = true;
  bool       g_both = false; // both LEDs held on, for the brightness check
  uint8_t    g_auto_index = 0;
  uint32_t   g_last_auto = 0;

  // What each state is supposed to look like, printed next to the state so the
  // panel can be checked against words rather than against memory.
  const char *Appearance(const PanelState &state)
  {
    if (state.fault != DryerFault::kNone)
    {
      switch (state.status)
      {
        case DryerStatus::kRunning: return "green steady, red blinking";
        case DryerStatus::kCooling: return "green blinking, red blinking";
        case DryerStatus::kStopped: return "green out, red blinking";
      }
      return "?";
    }

    switch (state.status)
    {
      case DryerStatus::kRunning: return "green steady, red out";
      case DryerStatus::kCooling: return "green blinking, red out";
      case DryerStatus::kStopped: return "green out, red steady";
    }
    return "?";
  }

  void PrintWiring()
  {
    Serial.println();
    Serial.println("One flat connector, header pins 16 to 20, ground in the middle:");
    Serial.println();
    Serial.printf("  16  GP%-2u  green LED anode   -- 330 ohm -- GND\n", LED_RUN_PIN);
    Serial.printf("  17  GP%-2u  red LED anode     -- 330 ohm -- GND\n", LED_FAULT_PIN);
    Serial.println("  18  GND   both cathodes and both button commons");
    Serial.printf("  19  GP%-2u  START button      -- dry contact -- GND\n", BTN_START_PIN);
    Serial.printf("  20  GP%-2u  STOP button       -- dry contact -- GND\n", BTN_STOP_PIN);
    Serial.println();
    Serial.println("LEDs are active HIGH, driven straight from the pad at ~4 mA.");
    Serial.println("Buttons are active LOW on the internal pullup: no external");
    Serial.println("resistor, and nothing but the contact to ground.");
    Serial.println();
    Serial.println("Use a *standard* green, forward drop 2.0-2.2 V. A high-brightness");
    Serial.println("InGaN green drops 3.0-3.2 V and would leave 330 ohm nothing to");
    Serial.println("work with, so its current would follow the part's tolerance.");
    Serial.println();
  }

  void PrintHelp()
  {
    Serial.println("Keys: s = stopped   r = running   c = cooling");
    Serial.println("      f = toggle a fault over whichever of those is showing");
    Serial.println("      a = cycle the five appearances, 4 s each (default)");
    Serial.println("      b = both LEDs on, for the brightness check");
    Serial.println("      0 = both LEDs off      h = wiring + help");
    Serial.println();
    Serial.println("Pressing START or STOP also sets the state, which is the panel");
    Serial.println("end to end: the button the operator presses and the LED they read.");
    Serial.println();
  }

  void ShowState(const char *how)
  {
    Serial.printf("State: %-8s %-8s (%s)  [%s]\n", StatusName(g_state.status),
                  g_state.fault == DryerFault::kNone ? "" : "+ fault",
                  Appearance(g_state), how);
  }

  void SetState(DryerStatus status, DryerFault fault, const char *how)
  {
    g_auto  = false;
    g_both  = false;
    g_state = PanelState{status, fault};
    ShowState(how);
  }

  // What the four pins do before pinMode() ever runs. Between power-up and
  // setup() the pads are plain inputs sitting low through their default
  // pull-down, so both LEDs must be dark and both buttons must read high once
  // their pullup is on. The measurement is here because that is an assumption
  // until something reads the pins back.
  void CheckIdleLevels()
  {
    pinMode(LED_RUN_PIN, INPUT);
    pinMode(LED_FAULT_PIN, INPUT);
    delay(5);

    bool green_high = digitalRead(LED_RUN_PIN) == HIGH;
    bool red_high   = digitalRead(LED_FAULT_PIN) == HIGH;

    Serial.printf("Boot idle level on GP%-2u (green LED): %-4s -> %s\n", LED_RUN_PIN,
                  green_high ? "HIGH" : "LOW", green_high ? "** LIT **" : "dark");
    Serial.printf("Boot idle level on GP%-2u (red LED)  : %-4s -> %s\n", LED_FAULT_PIN,
                  red_high ? "HIGH" : "LOW", red_high ? "** LIT **" : "dark");

    pinMode(BTN_START_PIN, INPUT_PULLUP);
    pinMode(BTN_STOP_PIN, INPUT_PULLUP);
    delay(5);

    bool start_low = digitalRead(BTN_START_PIN) == LOW;
    bool stop_low  = digitalRead(BTN_STOP_PIN) == LOW;

    Serial.printf("Resting level on GP%-2u (START)      : %-4s -> %s\n", BTN_START_PIN,
                  start_low ? "LOW" : "HIGH",
                  start_low ? "** PRESSED or shorted to GND **" : "released");
    Serial.printf("Resting level on GP%-2u (STOP)       : %-4s -> %s\n", BTN_STOP_PIN,
                  stop_low ? "LOW" : "HIGH",
                  stop_low ? "** PRESSED or shorted to GND **" : "released");

    if (green_high || red_high)
    {
      Serial.println();
      Serial.println("  ** A LED is lit before the firmware drives its pin. It runs on");
      Serial.println("  ** every reset, for the whole boot window. Check that the anode");
      Serial.println("  ** is on the GPIO and the cathode goes to ground through the");
      Serial.println("  ** resistor — a LED wired to 3V3 instead is on for ever.");
    }
    if (start_low || stop_low)
    {
      Serial.println();
      Serial.println("  ** A button reads pressed with nobody touching it. Either the");
      Serial.println("  ** contact is stuck closed, or that signal wire is shorted to");
      Serial.println("  ** the ground at header 18 — a connector one row out does");
      Serial.println("  ** exactly that.");
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
  Serial.println("  Panel buttons and status LEDs — bring-up");
  Serial.println("========================================");

  PrintWiring();

  // Before anything is driven: what the four pins do on their own.
  CheckIdleLevels();

  Logger::Init();

  g_status_led.Begin();
  g_start_button.Begin();
  g_stop_button.Begin();

  Serial.println();
  PrintHelp();
  ShowState("cycling");

  g_last_auto = millis();
}

void loop()
{
  uint32_t now = millis();

  if (Serial.available() > 0)
  {
    int key = Serial.read();
    while (Serial.available() > 0)
    {
      Serial.read();
    }

    switch (key)
    {
    case 's':
      SetState(DryerStatus::kStopped, DryerFault::kNone, "held");
      break;
    case 'r':
      SetState(DryerStatus::kRunning, DryerFault::kNone, "held");
      break;
    case 'c':
      SetState(DryerStatus::kCooling, DryerFault::kNone, "held");
      break;
    // The fault is a modifier on whatever the session is doing, so the key
    // toggles it in place rather than replacing the state — which is the only
    // way to watch green hold steady while red starts blinking over it.
    case 'f':
      SetState(g_state.status,
               g_state.fault == DryerFault::kNone ? DryerFault::kSensorStale
                                                  : DryerFault::kNone,
               "held");
      break;
    case 'a':
      g_auto      = true;
      g_both      = false;
      g_last_auto = now;
      Serial.println("Cycling the five appearances, 4 s each.");
      break;
    case 'b':
      g_auto = false;
      g_both = true;
      Serial.println("Both LEDs on. Look at them in daylight: if the green is the");
      Serial.println("one that disappears, it is the wrong green or the wrong resistor.");
      break;
    case '0':
      g_auto = false;
      g_both = false;
      g_status_led.Apply({false, false});
      Serial.println("Both LEDs off.");
      return;
    case 'h':
      PrintWiring();
      PrintHelp();
      break;
    default:
      break;
    }
  }

  g_start_button.Update(now);
  g_stop_button.Update(now);

  // Named as it lands: one press each is all it takes to tell two swapped
  // signal wires apart.
  if (g_stop_button.IsPressed())
  {
    Serial.println("STOP pressed");
    SetState(DryerStatus::kStopped, g_state.fault, "from the button");
  }
  if (g_start_button.IsPressed())
  {
    Serial.println("START pressed");
    SetState(DryerStatus::kRunning, g_state.fault, "from the button");
  }

  if (g_auto && (now - g_last_auto) >= kAutoStateMs)
  {
    g_last_auto  = now;
    g_auto_index = (g_auto_index + 1) % kStateCount;
    g_state      = kAllStates[g_auto_index];
    ShowState("cycling");
  }

  if (g_both)
  {
    g_status_led.Apply({true, true});
  }
  else
  {
    g_status_led.Apply(PatternFor(g_state, now));
  }

  delay(10);
}
