#pragma once

#include <MFRC522.h>

#include "rc522_version.h"
#include "uid_hex.h"

// RC522 lewat SPI. Dipakai firmware utama dan rfid-test, jadi format UID
// yang terlihat di rfid-test sama persis dengan yang dikirim ke server.
class RfidReader {
 public:
  RfidReader();

  // SPI + PCD_Init, lalu mencatat versi chip ke serial. false kalau chip
  // tidak menjawab (cek wiring dan 3.3V).
  bool begin();

  uint8_t readVersion();

  // true kalau ada kartu baru; uidHex diisi format final (portal::formatUidHex).
  // Kartu langsung di-halt, jadi kartu yang tetap ditahan biasanya tidak
  // terbaca lagi sampai diangkat.
  bool poll(char* uidHex, size_t size);

  // Hasil poll() terakhir yang berhasil, untuk info tambahan di rfid-test.
  const MFRC522::Uid& lastUid() const { return mfrc522_.uid; }
  const __FlashStringHelper* lastTypeName();

  // Dipanggil berkala. RC522 kadang berhenti menjawab (gangguan daya/noise);
  // kalau VersionReg tidak masuk akal, chip diinisialisasi ulang.
  void healthCheck(uint32_t nowMs, uint32_t intervalMs);

 private:
  bool init();

  MFRC522 mfrc522_;
  uint32_t lastHealthCheckMs_ = 0;
  bool healthy_ = false;
};
