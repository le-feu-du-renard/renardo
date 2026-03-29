#include <Arduino.h>
#include "config.h"
#include <ModbusMaster.h>

// =========================================================
// Test direct SHT30 — reproduit exactement le code Python
// FC03, registre 0x0000 (hum) + 0x0001 (temp), adresse 1
// =========================================================

static constexpr uint8_t SENSOR_ADDRESS = 2;

static ModbusMaster node;

static void preTransmit()
{
  Serial.println("  [DE] HIGH (transmit)");
  digitalWrite(RS485_DE_PIN, HIGH);
  delayMicroseconds(200);
}

static void postTransmit()
{
  Serial2.flush();  // attendre que tous les octets soient sortis du UART
  digitalWrite(RS485_DE_PIN, LOW);
  Serial.println("  [DE] LOW (receive)");
}

void setup()
{
  Serial.begin(115200);
  delay(2000);
  

  Serial.println();
  Serial.println("=== Test direct SHT30 RS485 ===");
  Serial.print("TX pin : GPIO ");
  Serial.println(RS485_TX_PIN);
  Serial.print("RX pin : GPIO ");
  Serial.println(RS485_RX_PIN);
  Serial.print("DE pin : GPIO ");
  Serial.println(RS485_DE_PIN);
  Serial.print("Baud   : ");
  Serial.println(MODBUS_BAUDRATE);
  Serial.print("Adresse: ");
  Serial.println(SENSOR_ADDRESS);
  Serial.println();

  Serial2.setTX(RS485_TX_PIN);
  Serial2.setRX(RS485_RX_PIN);
  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1);

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);

  node.begin(SENSOR_ADDRESS, Serial2);
  node.preTransmission(preTransmit);
  node.postTransmission(postTransmit);
}

void loop()
{
  Serial.print("Lecture FC03 reg 0x0000 x2... ");

  uint8_t result = node.readHoldingRegisters(0x0000, 2);

  Serial.print("result=0x");
  Serial.println(result, HEX);

  if (result == ModbusMaster::ku8MBSuccess)
  {
    float hum = node.getResponseBuffer(0) / 10.0f;
    float temp = node.getResponseBuffer(1) / 10.0f;
    Serial.print("  Hum : ");
    Serial.print(hum, 1);
    Serial.println(" %RH");
    Serial.print("  Temp: ");
    Serial.print(temp, 1);
    Serial.println(" C");
  }
  else
  {
    if (result == 0xE2)
      Serial.println("  -> Timeout (pas de reponse)");
    else if (result == 0x02)
      Serial.println("  -> Adresse registre rejetee par la sonde");
    else if (result == 0x01)
      Serial.println("  -> Fonction rejetee par la sonde");
    else if (result == 0xE3)
      Serial.println("  -> CRC invalide");
  }

  Serial.println("---");
  delay(2000);
}
