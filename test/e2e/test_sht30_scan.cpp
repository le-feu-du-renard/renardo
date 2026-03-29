#include <Arduino.h>
#include "config.h"
#include <ModbusMaster.h>

// =========================================================
// Scanner Modbus RTU — trouve toutes les sondes sur le bus
// Teste les adresses 1 à 247, affiche celles qui répondent.
// =========================================================

static ModbusMaster node;

static void preTransmit()  { digitalWrite(RS485_DE_PIN, HIGH); }
static void postTransmit() { Serial2.flush(); digitalWrite(RS485_DE_PIN, LOW); }

void setup()
{
  Serial.begin(115200);
  delay(2000);
  

  Serial.println();
  Serial.println("=== Scanner Modbus RS485 ===");
  Serial.print("Baudrate : ");
  Serial.println(MODBUS_BAUDRATE);
  Serial.println("Scan adresses 1-247...");
  Serial.println();

  Serial2.setTX(RS485_TX_PIN);
  Serial2.setRX(RS485_RX_PIN);
  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1);

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);

  node.preTransmission(preTransmit);
  node.postTransmission(postTransmit);

  uint8_t found = 0;

  for (uint8_t addr = 1; addr <= 2; addr++)
  {
    node.begin(addr, Serial2);
    uint8_t result = node.readHoldingRegisters(MODBUS_REG_HUMIDITY, 2);

    if (result == ModbusMaster::ku8MBSuccess)
    {
      float temp = node.getResponseBuffer(0) / MODBUS_RAW_SCALE;
      float hum = node.getResponseBuffer(1) / MODBUS_RAW_SCALE;
      Serial.print("  [TROUVE] adresse ");
      Serial.print(addr);
      Serial.print("  ->  ");
      Serial.print(temp, 1);
      Serial.print(" C  /  ");
      Serial.print(hum, 1);
      Serial.println(" %RH");
      found++;
    }
    else
    {
      Serial.print(".");
      if (addr % 40 == 0)
        Serial.println();
    }

    delay(50);
  }

  Serial.println();
  Serial.println();
  if (found == 0)
  {
    Serial.println("Aucune sonde trouvee.");
    Serial.println("-> Verifier cablage A/B, TX/RX, et alimentation sonde.");
  }
  else
  {
    Serial.print(found);
    Serial.println(" sonde(s) trouvee(s).");
  }
  Serial.println("=== Scan termine ===");
}

void loop()
{
  delay(3000);

  Serial.println();
  Serial.println("--- Nouveau scan ---");

  uint8_t found = 0;

  for (uint8_t addr = 1; addr <= 247; addr++)
  {
    node.begin(addr, Serial2);
    uint8_t result = node.readHoldingRegisters(MODBUS_REG_HUMIDITY, 2);

    if (result == ModbusMaster::ku8MBSuccess)
    {
      float temp = node.getResponseBuffer(0) / MODBUS_RAW_SCALE;
      float hum = node.getResponseBuffer(1) / MODBUS_RAW_SCALE;
      Serial.print("  [TROUVE] adresse ");
      Serial.print(addr);
      Serial.print("  ->  ");
      Serial.print(temp, 1);
      Serial.print(" C  /  ");
      Serial.print(hum, 1);
      Serial.println(" %RH");
      found++;
    }
    else
    {
      Serial.print(".");
      if (addr % 40 == 0)
        Serial.println();
    }

    delay(50);
  }

  Serial.println();
  if (found == 0)
    Serial.println("Aucune sonde trouvee.");
  else
  {
    Serial.print(found);
    Serial.println(" sonde(s) trouvee(s).");
  }
}
