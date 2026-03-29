#include <Arduino.h>

// =========================================================
// Test potentiometre lineaire 10K — LA42DWQ-22
//
// Cablage :
//   Z1  -> GND
//   Z2  -> 3.3V
//   3 (curseur/wiper) -> GPIO 26 (ADC0)
//
// Note : sur le Pico W seuls GPIO 26/27/28/29 sont ADC.
//        GPIO 15 est digital uniquement.
//
// Comportement :
//   - Lecture ADC 12 bits (0–4095) toutes les 200 ms
//   - Affichage : valeur brute et tension correspondante
// =========================================================

static constexpr uint8_t kPotPin = 26; // ADC0 — brancher le curseur ici
static constexpr uint8_t kAdcBits = 12;
static constexpr uint16_t kAdcMax = (1u << kAdcBits) - 1; // 4095
static constexpr float kVref = 3.3f;
static constexpr uint32_t kReadMs = 200;

static uint32_t last_read_ms = 0;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  analogReadResolution(kAdcBits);
  pinMode(kPotPin, INPUT);

  Serial.println();
  Serial.println("=== Test potentiometre 10K LA42DWQ-22 ===");
  Serial.println("Cablage : Z1->GND | Z2->3.3V | broche 3 (curseur)->GPIO 26");
  Serial.println();
  Serial.print("GPIO    : ");
  Serial.println(kPotPin);
  Serial.print("ADC     : ");
  Serial.print(kAdcBits);
  Serial.println(" bits (0–4095)");
  Serial.print("Vref    : ");
  Serial.print(kVref);
  Serial.println(" V");
  Serial.println();
  Serial.println("Tournez le potentiometre — lecture toutes les 200 ms.");
  Serial.println("─────────────────────────────────────────────────────");
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

  uint16_t raw = analogRead(kPotPin);
  float volts = (raw / static_cast<float>(kAdcMax)) * kVref;

  Serial.print("raw=");
  Serial.print(raw);
  Serial.print("  ");
  Serial.print(volts, 3);
  Serial.println(" V");
}
