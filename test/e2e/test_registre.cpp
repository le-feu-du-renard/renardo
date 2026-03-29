#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <hardware/gpio.h>

// =========================================================
// Test registre — ON/OFF via MCP23017 GPB0, GPB1, GPB2
//
// Cablage :
//   GPIO 10 (SDA) -> MCP23017 SDA
//   GPIO 11 (SCL) -> MCP23017 SCL
//   Adresse I2C expander : 0x20
//
//   GPB0 (pin MCP 8)  -> registre 1
//   GPB1 (pin MCP 9)  -> registre 2
//   GPB2 (pin MCP 10) -> registre 3
//
// Comportement :
//   - Alterne l'activation de GPB0, GPB1, GPB2 toutes les 2 s
//   - Affiche l'etat courant sur le port serie
// =========================================================

static constexpr uint8_t kSdaPin = 10;
static constexpr uint8_t kSclPin = 11;
static constexpr uint8_t kExpanderAddress = 0x20;

// Port B : pin index dans la lib Adafruit = 8 + bit GPB
static constexpr uint8_t kGpb0 = 9;
// static constexpr uint8_t kGpb0 = 8;
// static constexpr uint8_t kGpb1 = 9;
// static constexpr uint8_t kGpb2 = 10;

static constexpr uint32_t kDelayMs = 3000;

static Adafruit_MCP23X17 mcp;
static bool state = false; // 0=GPB0, 1=GPB1, 2=GPB2

static void setAll()
{
  mcp.digitalWrite(kGpb0, state ? HIGH : LOW);
  // mcp.digitalWrite(kGpb1, gpb1 ? HIGH : LOW);
  // mcp.digitalWrite(kGpb2, gpb2 ? HIGH : LOW);
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("=== Test registre via MCP23017 GPB0/GPB1/GPB2 ===");
  Serial.print("SDA : GPIO");
  Serial.println(kSdaPin);
  Serial.print("SCL : GPIO");
  Serial.println(kSclPin);
  Serial.print("Adresse : 0x");
  Serial.println(kExpanderAddress, HEX);
  Serial.println("Alternance GPB0 -> GPB1 -> GPB2 toutes les 2 s.");
  Serial.println();

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

  mcp.pinMode(kGpb0, OUTPUT);
  // mcp.pinMode(kGpb1, OUTPUT);
  // mcp.pinMode(kGpb2, OUTPUT);
  setAll();

  Serial.println("MCP23017 initialise. Debut du test...");
  Serial.println();
}

void loop()
{
  setAll();
  state = !state;

  Serial.print("GPB0=");
  Serial.print(state ? "ON " : "OFF");
  delay(kDelayMs);
}
