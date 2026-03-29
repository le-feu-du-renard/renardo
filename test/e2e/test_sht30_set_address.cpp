#include <Arduino.h>
#include "config.h"
#include <ModbusMaster.h>

// =========================================================
// Utilitaire : changer l'adresse Modbus d'une sonde SHT30 RS485
//
// IMPORTANT : connecter UNE SEULE sonde sur le bus RS485.
//
// Modifier NEW_ADDRESS ci-dessous avant de flasher.
//
// Procédure :
//   1. Flasher ce sketch avec NEW_ADDRESS = 2
//   2. Connecter S2 seule
//   3. Vérifier dans le moniteur série que l'adresse est changée
//   4. Flasher ensuite test_sht30_rs485 pour tester les deux sondes
// =========================================================

static constexpr uint8_t  CURRENT_ADDRESS = 1;  // adresse actuelle de la sonde
static constexpr uint8_t  NEW_ADDRESS     = 2;  // nouvelle adresse souhaitée

// Registre holding qui stocke l'adresse Modbus (FC06)
// Valeur commune pour les modules SHT30 RS485 chinois
static constexpr uint16_t REG_DEVICE_ADDRESS = 0x0101;

static ModbusMaster node;

static void preTransmit()  { digitalWrite(RS485_DE_PIN, HIGH); }
static void postTransmit() { digitalWrite(RS485_DE_PIN, LOW);  }

void setup()
{
  Serial.begin(115200);
  delay(2000);
  

  Serial.println();
  Serial.println("=== Programmation adresse SHT30 RS485 ===");
  Serial.print("Adresse actuelle : "); Serial.println(CURRENT_ADDRESS);
  Serial.print("Nouvelle adresse : "); Serial.println(NEW_ADDRESS);
  Serial.println("Assurez-vous qu'une seule sonde est connectee.");
  Serial.println();

  Serial2.setTX(RS485_TX_PIN);
  Serial2.setRX(RS485_RX_PIN);
  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1);

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);

  node.begin(CURRENT_ADDRESS, Serial2);
  node.preTransmission(preTransmit);
  node.postTransmission(postTransmit);

  delay(500);

  // Etape 1 : vérifier que la sonde répond à l'adresse actuelle
  Serial.print("Ping sonde adresse ");
  Serial.print(CURRENT_ADDRESS);
  Serial.print("... ");

  float temp = 0, hum = 0;
  uint8_t result = node.readInputRegisters(MODBUS_REG_TEMPERATURE, 2);
  if (result == ModbusMaster::ku8MBSuccess)
  {
    temp = node.getResponseBuffer(0) / MODBUS_RAW_SCALE;
    hum  = node.getResponseBuffer(1) / MODBUS_RAW_SCALE;
    Serial.println("OK");
    Serial.print("  Temp: "); Serial.print(temp, 1); Serial.println(" C");
    Serial.print("  Hum : "); Serial.print(hum,  1); Serial.println(" %RH");
  }
  else
  {
    Serial.println("ECHEC - nouvelle tentative dans 2s...");
    return;
  }

  // Etape 2 : écrire la nouvelle adresse dans le registre 0x0101
  Serial.println();
  Serial.print("Ecriture nouvelle adresse ");
  Serial.print(NEW_ADDRESS);
  Serial.print(" dans registre 0x0101... ");

  result = node.writeSingleRegister(REG_DEVICE_ADDRESS, NEW_ADDRESS);
  if (result == ModbusMaster::ku8MBSuccess)
  {
    Serial.println("OK");
  }
  else
  {
    Serial.print("ECHEC (erreur 0x");
    Serial.print(result, HEX);
    Serial.println(")");
    Serial.println("  -> Certains modules utilisent un registre different.");
    Serial.println("     Essayer REG_DEVICE_ADDRESS = 0x00FF ou 0x07D0.");
    return;
  }

  delay(200);

  // Etape 3 : vérifier que la sonde répond maintenant à la nouvelle adresse
  Serial.println();
  Serial.print("Verification lecture a la nouvelle adresse ");
  Serial.print(NEW_ADDRESS);
  Serial.print("... ");

  node.begin(NEW_ADDRESS, Serial2);
  result = node.readInputRegisters(MODBUS_REG_TEMPERATURE, 2);
  if (result == ModbusMaster::ku8MBSuccess)
  {
    temp = node.getResponseBuffer(0) / MODBUS_RAW_SCALE;
    hum  = node.getResponseBuffer(1) / MODBUS_RAW_SCALE;
    Serial.println("OK - adresse changee avec succes !");
    Serial.print("  Temp: "); Serial.print(temp, 1); Serial.println(" C");
    Serial.print("  Hum : "); Serial.print(hum,  1); Serial.println(" %RH");
  }
  else
  {
    Serial.println("ECHEC - l'adresse n'a pas ete prise en compte.");
    Serial.println("  -> Essayer de couper l'alimentation de la sonde puis relancer.");
  }

  Serial.println();
  Serial.println("=== Termine. Vous pouvez flasher test_sht30_rs485 maintenant. ===");
}

void loop()
{
  delay(2000);
  setup();
}
