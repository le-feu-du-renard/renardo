#include <Arduino.h>

// =========================================================
// Test sélecteur rotatif 2 positions — Schneider ZBE BE101C NO
//
// Cablage :
//   A1 (contact) -> GPIO 22 (pull-up interne)
//   A2 (contact) -> GND
//
// Principe :
//   Contact NO (Normally Open)
//   Position 1 : contact ouvert  -> GPIO HIGH -> mode ECO
//   Position 2 : contact ferme   -> GPIO LOW  -> mode PERFORMANCE
//
// Comportement :
//   - Lecture toutes les 50 ms avec debounce 50 ms
//   - Affichage de la position courante et des changements d'etat
// =========================================================

static constexpr uint8_t kSelectorPin = 22;
static constexpr uint32_t kDebounceMs = 50;
static constexpr uint32_t kReadMs = 50;
static constexpr uint32_t kPrintMs = 500;

static bool last_stable_state = HIGH; // pull-up : repos = HIGH
static bool last_raw_state = HIGH;
static uint32_t last_change_ms = 0;
static uint32_t last_read_ms = 0;
static uint32_t last_print_ms = 0;

static const char *position_label(bool gpio_state)
{
  return gpio_state ? "Position 1 — ECO         (contact ouvert)"
                    : "Position 2 — PERFORMANCE (contact ferme)";
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  pinMode(kSelectorPin, INPUT_PULLUP);

  last_stable_state = digitalRead(kSelectorPin);
  last_raw_state = last_stable_state;

  Serial.println();
  Serial.println("=== Test selecteur rotatif ZBE BE101C NO (2 positions) ===");
  Serial.println("Cablage : A1 -> GPIO 22 (pull-up) | A2 -> GND");
  Serial.println();
  Serial.print("GPIO        : ");
  Serial.println(kSelectorPin);
  Serial.print("Debounce    : ");
  Serial.print(kDebounceMs);
  Serial.println(" ms");
  Serial.println();
  Serial.print("Etat initial : ");
  Serial.println(position_label(last_stable_state));
  Serial.println("─────────────────────────────────────────────────────────");
  Serial.println();
}

void loop()
{
  uint32_t now = millis();
  if (now - last_read_ms < kReadMs)
  {
    return;
  }
  last_read_ms = now;

  bool current = digitalRead(kSelectorPin);

  // Debut d'un changement potentiel
  if (current != last_raw_state)
  {
    last_raw_state = current;
    last_change_ms = now;
  }

  // Changement stable apres debounce
  if (current != last_stable_state && (now - last_change_ms) >= kDebounceMs)
  {
    last_stable_state = current;
    Serial.print(">>> Changement : ");
    Serial.println(position_label(last_stable_state));
  }

  // Affichage periodique
  if (now - last_print_ms >= kPrintMs)
  {
    last_print_ms = now;
    Serial.print("GPIO=");
    Serial.print(last_stable_state ? "HIGH" : "LOW ");
    Serial.print("  ");
    Serial.println(position_label(last_stable_state));
  }
}
