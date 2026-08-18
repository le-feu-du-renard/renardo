// Encoder bring-up — built by `pio run -e encoder_test -t upload -t monitor`.
//
// Answers, one knob movement at a time:
//   nothing reported          -> a line is broken, or the common is not on GND
//   "CCW" when turning right  -> A and B are swapped
//   more than 4 edges/detent  -> contact bounce, or the wrong detent ratio
//   phantom detents at rest   -> a line is floating (no pull-up path to 3V3)
//   click never fires         -> the switch pin or its return to GND is wrong
//
// It compiles the *production* QuadratureDecoder, so a detent reported here is
// a detent the firmware would see. What it adds over the firmware is the raw
// edge accounting that RotaryEncoder deliberately hides: the number of valid
// transitions behind each detent, and the two kinds of rejected ones. That is
// the difference between "the encoder is not wired" and "the encoder is wired
// but bouncing", which look identical from inside the menu.
//
// The switch is debounced exactly like RotaryEncoder does, but polled every
// loop instead of every 50 ms, so a click missed here is a wiring fault rather
// than a sampling one.

#include <Arduino.h>

#include "config.h"
#include "QuadratureDecoder.h"

namespace
{

QuadratureDecoder g_decoder;

// Written by the interrupt, read by the loop under noInterrupts().
volatile uint8_t  g_state = 0;
volatile uint32_t g_edges_valid = 0;      // legal single-line transitions
volatile uint32_t g_edges_both = 0;       // both lines changed at once: bounce
volatile uint32_t g_edges_repeat = 0;     // interrupt fired, nothing had moved
volatile uint32_t g_valid_since_detent = 0;
volatile uint32_t g_bounce_since_detent = 0;

volatile int32_t  g_position = 0;
volatile uint32_t g_detent_count = 0;
volatile uint32_t g_cw_count = 0;
volatile uint32_t g_ccw_count = 0;
volatile int8_t   g_last_direction = 0;
volatile uint32_t g_last_valid = 0;
volatile uint32_t g_last_bounce = 0;

// Switch state, main loop only.
bool     g_sw_raw_prev = false;
bool     g_sw_consumed = false;
uint32_t g_sw_down_ms = 0;
uint32_t g_click_count = 0;

constexpr uint32_t kDebounceMs = 30;

uint32_t g_reported_detents = 0;
uint32_t g_last_summary_ms = 0;
constexpr uint32_t kSummaryIntervalMs = 15000;

void OnEdge()
{
  uint8_t current = static_cast<uint8_t>((digitalRead(ENCODER_A_PIN) == HIGH ? 2 : 0) |
                                         (digitalRead(ENCODER_B_PIN) == HIGH ? 1 : 0));
  uint8_t changed = static_cast<uint8_t>(current ^ g_state);
  g_state = current;

  if (changed == 0)
  {
    // The interrupt fired but the pins read the same as last time: the line
    // bounced back before we got here. Harmless, but a rising count means the
    // contacts are noisy enough to be worth a capacitor.
    g_edges_repeat++;
    g_bounce_since_detent++;
  }
  else if (changed == 0x3)
  {
    // Both lines cannot legally change together on a quadrature encoder. The
    // decoder drops these; counting them is how we know bounce is happening.
    g_edges_both++;
    g_bounce_since_detent++;
  }
  else
  {
    g_edges_valid++;
    g_valid_since_detent++;
  }

  int8_t movement = g_decoder.Step((current & 0x2) != 0, (current & 0x1) != 0);
  if (movement == 0)
  {
    return;
  }

  // Applied here so the test agrees with the firmware: RotaryEncoder reverses
  // at the same place. Without it, a board configured as reversed would still
  // report CCW here and send you looking for a wiring fault that is not there.
  if (ENCODER_REVERSED)
  {
    movement = static_cast<int8_t>(-movement);
  }

  g_position += movement;
  g_detent_count++;
  if (movement > 0)
  {
    g_cw_count++;
  }
  else
  {
    g_ccw_count++;
  }
  g_last_direction = movement;
  g_last_valid  = g_valid_since_detent;
  g_last_bounce = g_bounce_since_detent;
  g_valid_since_detent = 0;
  g_bounce_since_detent = 0;
}

void PrintWiring()
{
  // EC11 boards are silkscreened in at least three ways for the same three
  // signals, so all of them are printed: wiring by position instead of by
  // function is the mistake this is meant to prevent.
  Serial.println("Expected wiring — the same three signals, whatever the silkscreen:");
  Serial.printf("  A  / S1 / CLK   ->  GP%-2u  (header pin 9)\n", ENCODER_A_PIN);
  Serial.printf("  B  / S2 / DT    ->  GP%-2u  (header pin 10)\n", ENCODER_B_PIN);
  Serial.printf("  SW / Key        ->  GP%-2u  (header pin 11)\n", ENCODER_SW_PIN);
  Serial.println("  GND             ->  GND   (header pin 8)");
  Serial.println("  -- header pins 8 to 11, in that order, one flat connector --");
  Serial.println("  + / VCC, if the board has one  ->  3V3 (header pin 36)");
  Serial.println();
  Serial.println("On the bare component, GND is the *middle* pin of the three-pin");
  Serial.println("side, plus the second pin of the switch. On a breakout both are");
  Serial.println("already routed to the GND pad, so one wire does it.");
  Serial.println();
  Serial.println("3.3V only — GP6, GP7 and GP8 are not 5V tolerant. All three lines");
  Serial.println("use the internal pull-up, so a bare EC11 needs no resistor and no");
  Serial.println("supply at all; a breakout's own pull-ups simply sit in parallel.");
  Serial.println();
  Serial.println("S1 and S2 are interchangeable: swapping them only reverses the");
  Serial.println("direction, which check 2 below reports.");
  Serial.println();
  Serial.printf("ENCODER_REVERSED is %s in config.h.\n",
                ENCODER_REVERSED ? "true (directions flipped below)" : "false");
  Serial.println();
}

void PrintRestState()
{
  bool a = digitalRead(ENCODER_A_PIN) == HIGH;
  bool b = digitalRead(ENCODER_B_PIN) == HIGH;
  bool sw = digitalRead(ENCODER_SW_PIN) == HIGH;

  Serial.printf("Rest state:  A=%s  B=%s  SW=%s\n",
                a ? "HIGH" : "LOW ", b ? "HIGH" : "LOW ", sw ? "HIGH" : "LOW ");

  if (a && b && sw)
  {
    Serial.println("  -> as expected at a detent, knob released.");
  }
  if (!a || !b)
  {
    Serial.println("  !! A or B low at rest. Either the knob is parked between");
    Serial.println("     detents (turn it one click and reset), or that line is");
    Serial.println("     shorted to GND / wired to the common instead of GND.");
  }
  if (!sw)
  {
    Serial.println("  !! SW low with the button released: the switch is wired");
    Serial.println("     the wrong way round, or shorted to GND.");
  }
  Serial.println();
}

void PrintSummary()
{
  noInterrupts();
  uint32_t detents = g_detent_count;
  uint32_t cw      = g_cw_count;
  uint32_t ccw     = g_ccw_count;
  int32_t  position = g_position;
  uint32_t valid   = g_edges_valid;
  uint32_t both    = g_edges_both;
  uint32_t repeat  = g_edges_repeat;
  interrupts();

  Serial.println();
  Serial.println("--- summary ---------------------------------------");
  Serial.printf("  detents      %lu  (CW %lu / CCW %lu)   position %+ld\n",
                (unsigned long)detents, (unsigned long)cw, (unsigned long)ccw,
                (long)position);
  if (detents > 0)
  {
    Serial.printf("  valid edges  %lu  ->  %.2f per detent (expect 4.00)\n",
                  (unsigned long)valid, (float)valid / (float)detents);
  }
  Serial.printf("  rejected     %lu both-lines-at-once, %lu repeats\n",
                (unsigned long)both, (unsigned long)repeat);
  Serial.printf("  clicks       %lu\n", (unsigned long)g_click_count);
  Serial.println("---------------------------------------------------");
  Serial.println();
}

void UpdateSwitch()
{
  uint32_t now = millis();
  bool raw = (digitalRead(ENCODER_SW_PIN) == LOW);

  if (!raw)
  {
    if (g_sw_raw_prev && g_sw_consumed)
    {
      Serial.printf("release       held %lu ms\n", (unsigned long)(now - g_sw_down_ms));
    }
    g_sw_consumed = false;
  }
  else if (!g_sw_raw_prev)
  {
    g_sw_down_ms = now;
  }
  else if (!g_sw_consumed && (now - g_sw_down_ms) >= kDebounceMs)
  {
    g_sw_consumed = true;
    g_click_count++;
    Serial.printf("CLICK    #%-4lu\n", (unsigned long)g_click_count);
  }
  g_sw_raw_prev = raw;
}

void ReportDetents()
{
  noInterrupts();
  uint32_t detents  = g_detent_count;
  int32_t  position = g_position;
  int8_t   direction = g_last_direction;
  uint32_t valid    = g_last_valid;
  uint32_t bounce   = g_last_bounce;
  interrupts();

  if (detents == g_reported_detents)
  {
    return;
  }

  // Several detents can land between two loops when the knob is spun; only the
  // last one's edge counts survive, which is fine — the summary averages them.
  uint32_t missed = detents - g_reported_detents - 1;
  g_reported_detents = detents;

  Serial.printf("detent  #%-4lu %s   position %+ld   edges %lu",
                (unsigned long)detents, direction > 0 ? "CW " : "CCW",
                (long)position, (unsigned long)valid);
  if (bounce > 0)
  {
    Serial.printf("  (+%lu bounced)", (unsigned long)bounce);
  }
  if (missed > 0)
  {
    Serial.printf("  [%lu more while printing]", (unsigned long)missed);
  }
  Serial.println();

  if (valid != QuadratureDecoder::kCountsPerDetent)
  {
    Serial.printf("        ^ %lu valid edges for one detent, expected %d — see the note above\n",
                  (unsigned long)valid, (int)QuadratureDecoder::kCountsPerDetent);
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(2000);

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  delay(10);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  Rotary encoder — bring-up");
  Serial.println("========================================");
  Serial.println();

  PrintWiring();
  PrintRestState();

  // Seed the decoder with the resting position, exactly as RotaryEncoder does,
  // so the first movement is not read as a transition from an imaginary state.
  g_state = static_cast<uint8_t>((digitalRead(ENCODER_A_PIN) == HIGH ? 2 : 0) |
                                 (digitalRead(ENCODER_B_PIN) == HIGH ? 1 : 0));
  g_decoder.Reset();
  g_decoder.Step((g_state & 0x2) != 0, (g_state & 0x1) != 0);

  g_sw_raw_prev = (digitalRead(ENCODER_SW_PIN) == LOW);
  g_sw_consumed = g_sw_raw_prev;

  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), OnEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), OnEdge, CHANGE);

  Serial.println("What to do:");
  Serial.println("  1. Leave it alone for ten seconds. Nothing must be reported;");
  Serial.println("     detents at rest mean a line is floating.");
  Serial.println("  2. Turn one detent clockwise. Expect exactly one line,");
  Serial.println("     'CW', position +1, 4 edges. 'CCW' means this part numbers");
  Serial.println("     its pads the other way round: flip ENCODER_REVERSED.");
  Serial.println("  3. Turn ten detents each way. Position must come back to 0.");
  Serial.println("  4. Press the button: one CLICK per press, never two.");
  Serial.println("Send any character to print a summary.");
  Serial.println();
  g_last_summary_ms = millis();
}

void loop()
{
  ReportDetents();
  UpdateSwitch();

  if (Serial.available() > 0)
  {
    while (Serial.available() > 0)
    {
      Serial.read();
    }
    PrintSummary();
  }

  uint32_t now = millis();
  if (now - g_last_summary_ms >= kSummaryIntervalMs)
  {
    g_last_summary_ms = now;
    if (g_detent_count > 0 || g_click_count > 0)
    {
      PrintSummary();
    }
  }
}
