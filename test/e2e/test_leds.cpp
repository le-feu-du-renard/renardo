#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <hardware/gpio.h>

// =========================================================
// Test voyants indicateurs via MCP23017 (I2C expander)
//
// Cablage :
//   GPIO 16 (SDA) -> MCP23017 SDA
//   GPIO 17 (SCL) -> MCP23017 SCL
//
// Adresse I2C expander : 0x20
//
// Comportement :
//   Alterne toutes les 2 s entre deux voyants :
//     - LED 1 : GPA0
//     - LED 2 : GPA1
// =========================================================

static constexpr uint8_t kSdaPin = 10;
static constexpr uint8_t kSclPin = 11;
static constexpr uint8_t kExpanderAddress = 0x20;

// #define LED_PIN_ECO_MODE 0         // GPA0 - ECO mode indicator LED
// #define LED_PIN_PHASE_INIT 1       // GPA1 - init phase indicator LED
// #define LED_PIN_PHASE_BRASSAGE 2   // GPA2 - mixing phase indicator LED
// #define LED_PIN_PHASE_EXTRACTION 3 // GPA3 - extraction phase indicator LED
// #define LED_PIN_ELECTRIC_HEATER 7  // GPA7 - electric heater indicator LED
// #define LED_PIN_HYDRO_HEATER 6     // GPA6 - hydraulic heater indicator LED
// #define LED_PIN_FAN 5              // GPA5 - fan indicator LED
// #define LED_PIN_AIR_RENEWAL 4      // GPA4 - air renewal indicator LED

// // Port B – Digital outputs (Adafruit library: pin = 8 + GPB bit index)
// #define MCP_BTN_START_LED_PIN 15  // GPB7 - START button indicator LED
// #define MCP_BTN_STOP_LED_PIN 14   // GPB6 - STOP button indicator LED

static constexpr uint8_t kLed1Pin = 15; // GPA0
// 12  // GPB4
// 13 // GPB5
// 14 // GPB6
// 15 // GPB7
// static constexpr uint8_t kLed2Pin = 7; // GPA1
static constexpr uint32_t kSwitchIntervalMs = 2000;

static Adafruit_MCP23X17 mcp;
static bool led1_active = true; // true = LED1 allumee, false = LED2 allumee

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("=== Test voyants MCP23017 ===");
  Serial.print("SDA : GPIO");
  Serial.println(kSdaPin);
  Serial.print("SCL : GPIO");
  Serial.println(kSclPin);
  Serial.print("Adresse expander : 0x");
  Serial.println(kExpanderAddress, HEX);
  Serial.println("Switch toutes les 2 s entre LED1 (GPA0) et LED2 (GPA1).");
  Serial.println();

  Wire1.setSDA(kSdaPin);
  Wire1.setSCL(kSclPin);
  Wire1.begin();
  gpio_pull_up(kSdaPin); // force pull-up après begin() pour ne pas l'écraser
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

  mcp.pinMode(kLed1Pin, OUTPUT);
  // mcp.pinMode(kLed2Pin, OUTPUT);
  mcp.digitalWrite(kLed1Pin, LOW);
  // mcp.digitalWrite(kLed2Pin, LOW);

  Serial.println("MCP23017 initialise. Debut du test...");
  Serial.println();
}

void loop()
{
  mcp.digitalWrite(kLed1Pin, led1_active ? HIGH : LOW);
  // mcp.digitalWrite(kLed2Pin, led1_active ? LOW : HIGH);

  if (led1_active)
  {
    Serial.println("LED1 (GPA0) ON  |  LED2 (GPA1) OFF");
  }
  else
  {
    Serial.println("LED1 (GPA0) OFF  |  LED2 (GPA1) ON");
  }

  led1_active = !led1_active;
  delay(kSwitchIntervalMs);
}
