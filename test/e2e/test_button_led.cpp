#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <hardware/gpio.h>

// =========================================================
// Test bouton lumineux 24 V — Swideer LA155-P4-102
//
// Cablage :
//   GPIO 16 (SDA)  -> MCP23017 SDA
//   GPIO 17 (SCL)  -> MCP23017 SCL
//   Adresse I2C expander : 0x20
//
//   LED interne (via relais 24 V) : MCP23017 GPA2 (broche 2)
//   Contact NO du bouton          : GPIO 15 (pull-up interne)
//
// Comportement :
//   - Chaque appui sur le bouton bascule l'etat de la LED
//   - L'etat est affiche sur le port serie
//   - Un debounce logiciel de 50 ms est applique
// =========================================================

static constexpr uint8_t kSdaPin = 10;
static constexpr uint8_t kSclPin = 11;
static constexpr uint8_t kExpanderAddress = 0x20;
static constexpr uint8_t kLedPin = 1; // GPA1

static constexpr uint8_t kButtonPin = 17; // Contact NO — pull-up interne
static constexpr uint32_t kDebounceMs = 50;

static Adafruit_MCP23X17 mcp;
static bool led_state = false;
static bool last_btn_state = HIGH; // pull-up : repos = HIGH
static uint32_t last_change_ms = 0;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("=== Test bouton lumineux Swideer LA155-P4-102 ===");
  Serial.print("SDA : GPIO");
  Serial.println(kSdaPin);
  Serial.print("SCL : GPIO");
  Serial.println(kSclPin);
  Serial.print("Adresse expander : 0x");
  Serial.println(kExpanderAddress, HEX);
  Serial.print("LED  : MCP23017 GPA");
  Serial.println(kLedPin);
  Serial.print("Bouton (NO) : GPIO");
  Serial.println(kButtonPin);
  Serial.println("Appuyez sur le bouton pour basculer la LED.");
  Serial.println();

  // Bouton : entree avec pull-up interne, contact NO -> bas quand appuye
  pinMode(kButtonPin, INPUT_PULLUP);

  Wire1.setSDA(kSdaPin);
  Wire1.setSCL(kSclPin);
  Wire1.begin();
  gpio_pull_up(kSdaPin);
  gpio_pull_up(kSclPin);

  if (!mcp.begin_I2C(kExpanderAddress, &Wire1))
  {
    Serial.println("ERREUR : MCP23017 introuvable - verifier cablage et adresse.");
    while (true)
    {
      delay(1000);
    }
  }

  mcp.pinMode(kLedPin, OUTPUT);
  mcp.digitalWrite(kLedPin, LOW);

  Serial.println("MCP23017 initialise. LED eteinte. En attente d'un appui...");
  Serial.println();
}

void loop()
{
  bool current_btn = digitalRead(kButtonPin);
  uint32_t now = millis();

  // Front descendant + debounce -> bouton appuye
  if (current_btn == LOW && last_btn_state == HIGH && (now - last_change_ms) > kDebounceMs)
  {
    last_change_ms = now;
    led_state = !led_state;

    mcp.digitalWrite(kLedPin, led_state ? HIGH : LOW);

    Serial.print("Bouton appuye -> LED ");
    Serial.println(led_state ? "ON" : "OFF");
  }

  last_btn_state = current_btn;
}
