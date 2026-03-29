#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// I2C Bus 1 pins (from config.h)
#define I2C_BUS_1_SDA_PIN 10
#define I2C_BUS_1_SCL_PIN 11

RTC_DS1307 rtc;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== RTC DS1307 Test ===");
  Serial.printf("Pins: SDA=%d  SCL=%d  Adresse I2C=0x68\n",
                I2C_BUS_1_SDA_PIN, I2C_BUS_1_SCL_PIN);

  // --- 1. Init I2C + RTC ---
  Wire1.setSDA(I2C_BUS_1_SDA_PIN);
  Wire1.setSCL(I2C_BUS_1_SCL_PIN);
  Wire1.begin();

  if (!rtc.begin(&Wire1)) {
    Serial.println("[FAIL] RTC non détectée — scan du bus I2C en cours...\n");

    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire1.beginTransmission(addr);
      uint8_t err = Wire1.endTransmission();
      if (err == 0) {
        Serial.printf("  [SCAN] Périphérique trouvé à 0x%02X", addr);
        if (addr == 0x68) Serial.print("  <-- DS1307 attendu ici");
        Serial.println();
        found++;
      }
    }
    if (found == 0) {
      Serial.println("  [SCAN] Aucun périphérique trouvé — vérifier le câblage SDA/SCL et l'alimentation");
    } else {
      Serial.printf("\n  [SCAN] %d périphérique(s) trouvé(s), mais pas de RTC à 0x68\n", found);
    }
    return;
  }
  Serial.println("[OK]   RTC détectée sur le bus I2C");

  // --- 2. Oscillateur en marche ---
  if (!rtc.isrunning()) {
    Serial.println("[WARN] L'oscillateur RTC n'est pas en marche (perte de batterie ?)");
    Serial.println("       Mise à l'heure de compilation...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    delay(100);
  } else {
    Serial.println("[OK]   Oscillateur RTC en marche");
  }

  // --- 3. Lecture de l'heure courante ---
  DateTime now = rtc.now();
  Serial.printf("[OK]   Heure actuelle : %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());

  if (now.year() < 2020) {
    Serial.println("[FAIL] Année < 2020 — heure invalide");
    return;
  }
  Serial.println("[OK]   Année cohérente (>= 2020)");

  // --- 4. Écriture d'une heure de référence puis vérification ---
  DateTime ref(2025, 6, 15, 12, 0, 0);
  rtc.adjust(ref);
  delay(200); // laisser le temps à l'oscillateur de prendre la nouvelle valeur

  DateTime check = rtc.now();
  Serial.printf("[OK]   Heure de référence écrite : %04d-%02d-%02d %02d:%02d:%02d\n",
                ref.year(), ref.month(), ref.day(),
                ref.hour(), ref.minute(), ref.second());
  Serial.printf("       Heure relue                : %04d-%02d-%02d %02d:%02d:%02d\n",
                check.year(), check.month(), check.day(),
                check.hour(), check.minute(), check.second());

  // Tolérance de 2 secondes pour tenir compte du délai d'exécution
  long diff = (long)check.unixtime() - (long)ref.unixtime();
  if (diff < 0 || diff > 2) {
    Serial.printf("[FAIL] Écart trop grand : %ld seconde(s)\n", diff);
    return;
  }
  Serial.println("[OK]   Heure relue correcte (tolérance 2s)");

  // --- 5. Restaurer l'heure compile-time ---
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println("[OK]   Heure restaurée à la compilation");

  // --- Résultat ---
  Serial.println("\n=== RÉSULTAT : SUCCÈS ===");
}

void loop() {
  // Rien — tout est dans setup()
}
