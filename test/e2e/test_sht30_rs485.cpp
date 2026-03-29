// #include <Arduino.h>

// void setup()
// {
//   Serial.println("=== hello world ===");
// }

// void loop()
// {
//   delay(50);
//   Serial.println("---");
// }

#include <Arduino.h>
#include "config.h"
#include "Logger.h"
#include "ModbusSensors.h"

// =========================================================
// SHT30 RS485 Modbus sensor test
//
// Wiring:
//   GPIO 8  (TX) -> RS485 module DI
//   GPIO 9  (RX) -> RS485 module RO
//   GPIO 6  (DE) -> RS485 module DE + RE (tied together)
//   RS485 A/B bus shared between both sensors
//
// Sensor Modbus addresses (set via DIP switches or config tool):
//   S1 (inlet)  : address 1  (MODBUS_INLET_ADDRESS)
//   S2 (outlet) : address 2  (MODBUS_OUTLET_ADDRESS)
//
// Reads both sensors every 2 s and prints:
//   S1: 24.5 °C  - 58.3 %RH
//   S2: 22.1 °C  - 61.7 %RH
// =========================================================

static ModbusSensors sensors;

static constexpr uint32_t kReadIntervalMs = 2000;

static void readAndLog(uint8_t address, const char *label)
{
  float temp = 0.0f;
  float hum = 0.0f;

  if (sensors.ReadSensor(address, temp, hum))
  {
    Serial.print(label);
    Serial.print(": ");
    Serial.print(temp, 1);
    Serial.print(" °C  -  ");
    Serial.print(hum, 1);
    Serial.println(" %RH");
  }
  else
  {
    Serial.print(label);
    Serial.println(": lecture echouee (verifier adresse et cablage)");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(2000);  // laisse le temps au monitor USB de se connecter

  Logger::Init(LOG_LEVEL_VERBOSE);

  Serial.println();
  Serial.println("=== Test sondes SHT30 RS485 Modbus ===");
  Serial.print("S1 adresse Modbus : ");
  Serial.println(MODBUS_INLET_ADDRESS);
  Serial.print("S2 adresse Modbus : ");
  Serial.println(MODBUS_OUTLET_ADDRESS);
  Serial.print("Baudrate          : ");
  Serial.println(MODBUS_BAUDRATE);
  Serial.println("Lecture toutes les 2 s.");
  Serial.println();

  sensors.Begin(MODBUS_BAUDRATE);
}

void loop()
{
  readAndLog(MODBUS_INLET_ADDRESS, "S1");
  delay(50);
  readAndLog(MODBUS_OUTLET_ADDRESS, "S2");
  Serial.println("---");

  delay(kReadIntervalMs);
}
