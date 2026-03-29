#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <hardware/gpio.h>

// =========================================================
// Test global : voyants MCP23017 + boutons START/STOP + voltmetres
//
// Cablage :
//   GPIO 10 (SDA) -> MCP23017 SDA
//   GPIO 11 (SCL) -> MCP23017 SCL
//   Adresse I2C expander : 0x20
//
//   Bouton START (NO) : GPIO 16 — pull-up interne
//   Bouton STOP  (NO) : GPIO 17 — pull-up interne
//
//   Voltmetre temperature  : GPIO 18 (PWM 50 kHz 12-bit, 0–3 V)
//   Voltmetre humidite     : GPIO 19
//   Voltmetre temps total  : GPIO 20
//   Voltmetre % phase      : GPIO 21
//
//   Potentiometre temperature : GPIO 26 (ADC0)
//   Potentiometre humidite    : GPIO 27 (ADC1)
//
//   Selecteur rotatif (NO)    : GPIO 22 — pull-up interne
//     Position 1 (HIGH) : ECO | Position 2 (LOW) : PERFORMANCE
//
// Comportement LEDs :
//   Allume chaque LED une par une, toutes les 2 s :
//     GPA0..GPA7 -> GPB6 -> GPB7 -> recommence
//
// Comportement boutons :
//   Affiche sur le port serie chaque appui detecte (debounce 50 ms)
//
// Comportement voltmetres :
//   Applique une valeur aleatoire sur chaque voltmetre toutes les 5 s
//
// Comportement potentiometres :
//   Lecture ADC 12-bit toutes les 200 ms, affichage valeur brute + tension
//
// Comportement selecteur :
//   Lecture toutes les 50 ms avec debounce 50 ms
//   Affichage periodique toutes les 500 ms + message sur changement
// =========================================================

static constexpr uint8_t kSdaPin = 10;
static constexpr uint8_t kSclPin = 11;
static constexpr uint8_t kExpanderAddress = 0x20;
static constexpr uint32_t kSwitchIntervalMs = 2000;
static constexpr uint32_t kDebounceMs = 50;

static constexpr uint8_t kButtonStartPin = 16;
static constexpr uint8_t kButtonStopPin = 17;

static constexpr uint8_t kPwmBits = 12;
static constexpr uint32_t kPwmFreqHz = 50000;
static constexpr uint16_t kPwmMax = (1u << kPwmBits) - 1; // 4095
static constexpr float kVcc = 3.3f;
static constexpr float kDutyMax = 3.0f / kVcc; // ~0.909
static constexpr uint32_t kVoltmeterIntervalMs = 5000;

struct Voltmeter
{
  uint8_t pin;
  const char *name;
};
static const Voltmeter kVoltmeters[] = {
    {18, "Temperature"},
    {19, "Humidite   "},
    {20, "Temps total"},
    {21, "% phase    "},
};
static constexpr uint8_t kVoltmeterCount = sizeof(kVoltmeters) / sizeof(kVoltmeters[0]);
static uint32_t last_voltmeter_update_ms = 0;

static constexpr uint8_t kAdcBits = 12;
static constexpr uint16_t kAdcMax = (1u << kAdcBits) - 1; // 4095
static constexpr float kVref = 3.3f;
static constexpr uint32_t kPotReadMs = 200;

struct Potentiometer
{
  uint8_t pin;
  const char *name;
};
static const Potentiometer kPots[] = {
    {26, "Temperature"},
    {27, "Humidite   "},
};
static constexpr uint8_t kPotCount = sizeof(kPots) / sizeof(kPots[0]);
static uint32_t last_pot_read_ms = 0;

static constexpr uint8_t kSelectorPin = 22;
static constexpr uint32_t kSelectorReadMs = 50;
static constexpr uint32_t kSelectorPrintMs = 500;
static bool selector_stable_state = HIGH;
static bool selector_raw_state = HIGH;
static uint32_t selector_last_change_ms = 0;
static uint32_t selector_last_read_ms = 0;
static uint32_t selector_last_print_ms = 0;

static const char *selector_label(bool state)
{
  return state ? "Position 1 — ECO         (contact ouvert)"
               : "Position 2 — PERFORMANCE (contact ferme)";
}

static const uint8_t kLedPins[] = {0, 1, 2, 3, 4, 5, 6, 7, 14, 15};
static const char *kLedNames[] = {
    "GPA0", "GPA1", "GPA2", "GPA3", "GPA4", "GPA5", "GPA6", "GPA7",
    "GPB6", "GPB7"};
static constexpr uint8_t kLedCount = sizeof(kLedPins) / sizeof(kLedPins[0]);

static void applyRandomVoltmeters()
{
  Serial.println("--- Voltmetres ---");
  for (uint8_t i = 0; i < kVoltmeterCount; i++)
  {
    uint8_t percent = random(0, 101);
    float duty = (percent / 100.0f) * kDutyMax;
    uint16_t raw = static_cast<uint16_t>(duty * kPwmMax);
    float expected = duty * kVcc;
    analogWrite(kVoltmeters[i].pin, raw);
    Serial.print("  ");
    Serial.print(kVoltmeters[i].name);
    Serial.print(" (GPIO ");
    Serial.print(kVoltmeters[i].pin);
    Serial.print(") : ");
    Serial.print(percent);
    Serial.print(" %  ->  ");
    Serial.print(expected, 3);
    Serial.println(" V");
  }
}

static Adafruit_MCP23X17 mcp;
static uint8_t current_led = 0;
static uint32_t last_led_switch_ms = 0;

static bool last_start_state = HIGH;
static bool last_stop_state = HIGH;
static uint32_t last_start_change_ms = 0;
static uint32_t last_stop_change_ms = 0;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("=== Test global : voyants + boutons ===");
  Serial.print("SDA : GPIO");
  Serial.println(kSdaPin);
  Serial.print("SCL : GPIO");
  Serial.println(kSclPin);
  Serial.print("Adresse expander : 0x");
  Serial.println(kExpanderAddress, HEX);
  Serial.println("LEDs : GPA0..GPA7, GPB6, GPB7 — switch toutes les 2 s");
  Serial.print("Bouton START (NO) : GPIO");
  Serial.println(kButtonStartPin);
  Serial.print("Bouton STOP  (NO) : GPIO");
  Serial.println(kButtonStopPin);
  Serial.println("Voltmetres PWM 50 kHz 12-bit, valeur aleatoire toutes les 5 s :");
  for (uint8_t i = 0; i < kVoltmeterCount; i++)
  {
    Serial.print("  ");
    Serial.print(kVoltmeters[i].name);
    Serial.print(" -> GPIO ");
    Serial.println(kVoltmeters[i].pin);
  }
  Serial.println("Potentiometres ADC 12-bit, lecture toutes les 200 ms :");
  for (uint8_t i = 0; i < kPotCount; i++)
  {
    Serial.print("  ");
    Serial.print(kPots[i].name);
    Serial.print(" -> GPIO ");
    Serial.println(kPots[i].pin);
  }
  Serial.print("Selecteur rotatif (NO) : GPIO ");
  Serial.println(kSelectorPin);
  Serial.println();

  pinMode(kButtonStartPin, INPUT_PULLUP);
  pinMode(kButtonStopPin, INPUT_PULLUP);

  analogWriteFreq(kPwmFreqHz);
  analogWriteResolution(kPwmBits);
  for (uint8_t i = 0; i < kVoltmeterCount; i++)
  {
    pinMode(kVoltmeters[i].pin, OUTPUT);
    gpio_pull_up(kVoltmeters[i].pin);
  }
  randomSeed(micros());

  analogReadResolution(kAdcBits);
  for (uint8_t i = 0; i < kPotCount; i++)
  {
    pinMode(kPots[i].pin, INPUT);
  }

  pinMode(kSelectorPin, INPUT_PULLUP);
  selector_stable_state = digitalRead(kSelectorPin);
  selector_raw_state = selector_stable_state;
  Serial.print("Selecteur etat initial : ");
  Serial.println(selector_label(selector_stable_state));

  Wire1.setSDA(kSdaPin);
  Wire1.setSCL(kSclPin);
  Wire1.begin();
  gpio_pull_up(kSdaPin);
  gpio_pull_up(kSclPin);

  Serial.println("Scan I2C...");
  for (uint8_t addr = 1; addr < 127; addr++)
  {
    Wire1.beginTransmission(addr);
    if (Wire1.endTransmission() == 0)
    {
      Serial.print("  Device trouve : 0x");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("Scan termine.");

  if (!mcp.begin_I2C(kExpanderAddress, &Wire1))
  {
    Serial.println("ERREUR : MCP23017 introuvable - verifier cablage et adresse.");
    while (true)
    {
      delay(1000);
    }
  }

  for (uint8_t i = 0; i < kLedCount; i++)
  {
    mcp.pinMode(kLedPins[i], OUTPUT);
    mcp.digitalWrite(kLedPins[i], LOW);
  }

  Serial.println("MCP23017 initialise. Debut du test...");
  Serial.println();

  last_led_switch_ms = millis();
  last_voltmeter_update_ms = millis();
  applyRandomVoltmeters();
  last_pot_read_ms = millis();
  selector_last_read_ms = millis();
  selector_last_print_ms = millis();
}

void loop()
{
  uint32_t now = millis();

  // --- LEDs : switch non-bloquant ---
  if (now - last_led_switch_ms >= kSwitchIntervalMs)
  {
    last_led_switch_ms = now;

    for (uint8_t i = 0; i < kLedCount; i++)
    {
      mcp.digitalWrite(kLedPins[i], LOW);
    }
    mcp.digitalWrite(kLedPins[current_led], HIGH);

    Serial.print("LED ON : ");
    Serial.print(kLedNames[current_led]);
    Serial.print(" (pin ");
    Serial.print(kLedPins[current_led]);
    Serial.println(")");

    current_led = (current_led + 1) % kLedCount;
  }

  // --- Bouton START (GPIO 16) ---
  bool start_state = digitalRead(kButtonStartPin);
  if (start_state == LOW && last_start_state == HIGH && (now - last_start_change_ms) > kDebounceMs)
  {
    last_start_change_ms = now;
    Serial.println("Bouton START appuye (GPIO 16)");
  }
  last_start_state = start_state;

  // --- Voltmetres : update non-bloquant ---
  if (now - last_voltmeter_update_ms >= kVoltmeterIntervalMs)
  {
    last_voltmeter_update_ms = now;
    applyRandomVoltmeters();
  }

  // --- Bouton STOP (GPIO 17) ---
  bool stop_state = digitalRead(kButtonStopPin);
  if (stop_state == LOW && last_stop_state == HIGH && (now - last_stop_change_ms) > kDebounceMs)
  {
    last_stop_change_ms = now;
    Serial.println("Bouton STOP appuye (GPIO 17)");
  }
  last_stop_state = stop_state;

  // --- Potentiometres (ADC) ---
  if (now - last_pot_read_ms >= kPotReadMs)
  {
    last_pot_read_ms = now;
    for (uint8_t i = 0; i < kPotCount; i++)
    {
      uint16_t raw = analogRead(kPots[i].pin);
      float volts = (raw / static_cast<float>(kAdcMax)) * kVref;
      Serial.print("Pot ");
      Serial.print(kPots[i].name);
      Serial.print(" (GPIO ");
      Serial.print(kPots[i].pin);
      Serial.print(") : raw=");
      Serial.print(raw);
      Serial.print("  ");
      Serial.print(volts, 3);
      Serial.println(" V");
    }
  }

  // --- Selecteur rotatif (GPIO 22) ---
  if (now - selector_last_read_ms >= kSelectorReadMs)
  {
    selector_last_read_ms = now;
    bool current = digitalRead(kSelectorPin);

    if (current != selector_raw_state)
    {
      selector_raw_state = current;
      selector_last_change_ms = now;
    }

    if (current != selector_stable_state && (now - selector_last_change_ms) >= kDebounceMs)
    {
      selector_stable_state = current;
      Serial.print(">>> Selecteur : ");
      Serial.println(selector_label(selector_stable_state));
    }

    if (now - selector_last_print_ms >= kSelectorPrintMs)
    {
      selector_last_print_ms = now;
      Serial.print("Selecteur GPIO=");
      Serial.print(selector_stable_state ? "HIGH" : "LOW ");
      Serial.print("  ");
      Serial.println(selector_label(selector_stable_state));
    }
  }
}
