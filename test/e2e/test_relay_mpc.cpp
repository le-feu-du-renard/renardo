#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <hardware/gpio.h>

// =========================================================
// Test relais MCP23017 — ON/OFF GPB0 toutes les 3s
//
// Cablage :
//   GPIO 10 (SDA) -> MCP23017 SDA
//   GPIO 11 (SCL) -> MCP23017 SCL
//   Adresse I2C   : 0x20
//   GPB0 (pin 8)  -> relais
// =========================================================

static constexpr uint8_t kSdaPin = 10;
static constexpr uint8_t kSclPin = 11;
static constexpr uint8_t kExpanderAddress = 0x20;
static constexpr uint8_t kRelayPin = 10; // GPB0
static constexpr uint32_t kDelayMs = 3000;

static Adafruit_MCP23X17 mcp;
static bool state = false;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Wire1.setSDA(kSdaPin);
  Wire1.setSCL(kSclPin);
  Wire1.begin();
  gpio_pull_up(kSdaPin);
  gpio_pull_up(kSclPin);

  if (!mcp.begin_I2C(kExpanderAddress, &Wire1))
  {
    Serial.println("ERREUR : MCP23017 introuvable.");
    while (true)
    {
      delay(1000);
    }
  }

  mcp.pinMode(kRelayPin, OUTPUT);
  Serial.println("MCP23017 pret. Debut du test...");
}

void loop()
{
  state = !state;
  mcp.digitalWrite(kRelayPin, state ? HIGH : LOW);
  Serial.print("GPB0 = ");
  Serial.println(state ? "ON" : "OFF");
  delay(kDelayMs);
}
