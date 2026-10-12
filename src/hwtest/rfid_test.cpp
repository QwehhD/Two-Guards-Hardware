// rfid-test: cek RC522 menjawab, lalu cetak UID setiap kartu yang ditempel,
// dalam format final yang dikirim firmware utama ke server.
//
//   pio run -e rfid-test -t upload && pio device monitor
//
// Sengaja tanpa cooldown: setiap deteksi dicetak, supaya terlihat apakah
// kartu yang ditahan terbaca berulang.

#include <Arduino.h>

#include "app/log.h"
#include "app/rfid_reader.h"
#include "config.h"

namespace {

RfidReader reader;
bool ready = false;
uint32_t detections = 0;
uint32_t lastRetryMs = 0;

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  logBootBanner("Two-Guards rfid-test");

  ready = reader.begin();

  if (ready) {
    logf("RFID", "tempelkan kartu...");
  }
}

void loop() {
  const uint32_t now = millis();

  if (!ready) {
    if (now - lastRetryMs >= 2000) {
      lastRetryMs = now;
      ready = reader.begin();

      if (ready) {
        logf("RFID", "tempelkan kartu...");
      }
    }

    return;
  }

  char uid[portal::kUidHexSize];

  if (reader.poll(uid, sizeof uid)) {
    detections++;
    const MFRC522::Uid& raw = reader.lastUid();
    logf("SCAN", "#%lu UID %s (%u byte, SAK 0x%02X, %s)", static_cast<unsigned long>(detections), uid, raw.size, raw.sak,
         String(reader.lastTypeName()).c_str());
  }

  reader.healthCheck(now, RFID_HEALTH_INTERVAL_MS);
  delay(RFID_POLL_INTERVAL_MS);
}
