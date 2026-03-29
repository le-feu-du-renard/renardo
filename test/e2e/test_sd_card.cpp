#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// SD card SPI pins (from config.h)
#define SD_CARD_MISO_PIN 0   // SPI0 RX
#define SD_CARD_CS_PIN   1   // Chip Select
#define SD_CARD_SCK_PIN  2   // SCK
#define SD_CARD_MOSI_PIN 3   // SPI0 TX

#define TEST_FILENAME "sdtest.txt"
#define TEST_CONTENT  "Hello SD card!"

static bool sd_ok = false;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== SD Card Test ===");
  Serial.printf("Pins: MISO=%d  MOSI=%d  SCK=%d  CS=%d\n",
                SD_CARD_MISO_PIN, SD_CARD_MOSI_PIN, SD_CARD_SCK_PIN, SD_CARD_CS_PIN);

  // --- 1. Init SPI + SD ---
  pinMode(SD_CARD_CS_PIN, OUTPUT);
  digitalWrite(SD_CARD_CS_PIN, HIGH);

  SPI.setRX(SD_CARD_MISO_PIN);
  SPI.setTX(SD_CARD_MOSI_PIN);
  SPI.setSCK(SD_CARD_SCK_PIN);
  SPI.begin();

  if (!SD.begin(SD_CARD_CS_PIN)) {
    Serial.println("[FAIL] SD.begin() failed — vérifier le câblage ou la carte");
    return;
  }
  Serial.println("[OK]   SD initialisée");
  sd_ok = true;

  // --- 2. Lister les fichiers à la racine ---
  Serial.println("\n--- Fichiers sur la carte ---");
  File root = SD.open("/");
  if (!root) {
    Serial.println("[FAIL] Impossible d'ouvrir le répertoire racine");
    sd_ok = false;
    return;
  }
  int count = 0;
  File entry = root.openNextFile();
  while (entry) {
    Serial.printf("  %s  (%u bytes)\n", entry.name(), (unsigned)entry.size());
    entry.close();
    entry = root.openNextFile();
    count++;
  }
  root.close();
  if (count == 0) {
    Serial.println("  (carte vide)");
  }
  Serial.printf("[OK]   %d fichier(s) listé(s)\n", count);

  // --- 3. Écriture ---
  Serial.println("\n--- Test écriture ---");
  // Supprimer l'ancien fichier de test s'il existe
  if (SD.exists(TEST_FILENAME)) {
    SD.remove(TEST_FILENAME);
  }
  File wf = SD.open(TEST_FILENAME, FILE_WRITE);
  if (!wf) {
    Serial.println("[FAIL] Impossible de créer le fichier de test");
    sd_ok = false;
    return;
  }
  wf.println(TEST_CONTENT);
  wf.close();
  Serial.printf("[OK]   Fichier '%s' écrit\n", TEST_FILENAME);

  // --- 4. Lecture + vérification ---
  Serial.println("\n--- Test lecture ---");
  File rf = SD.open(TEST_FILENAME, FILE_READ);
  if (!rf) {
    Serial.println("[FAIL] Impossible de lire le fichier de test");
    sd_ok = false;
    return;
  }
  String line = rf.readStringUntil('\n');
  rf.close();
  line.trim();

  Serial.printf("  Contenu lu : \"%s\"\n", line.c_str());
  if (line == TEST_CONTENT) {
    Serial.println("[OK]   Contenu correct");
  } else {
    Serial.printf("[FAIL] Contenu attendu : \"%s\"\n", TEST_CONTENT);
    sd_ok = false;
    return;
  }

  // --- 5. Nettoyage ---
  SD.remove(TEST_FILENAME);
  Serial.printf("[OK]   Fichier '%s' supprimé\n", TEST_FILENAME);

  // --- Résultat ---
  Serial.println("\n=== RÉSULTAT : SUCCÈS ===");
}

void loop() {
  // Rien — tout est dans setup()
}
