#include "rfid_reader.h"

#include <SPI.h>

#include "config.h"
#include "log.h"

RfidReader::RfidReader() : mfrc522_(PIN_RC522_SS, PIN_RC522_RST) {}

bool RfidReader::begin() {
  SPI.begin(PIN_RC522_SCK, PIN_RC522_MISO, PIN_RC522_MOSI, PIN_RC522_SS);

  return init();
}

bool RfidReader::init() {
  mfrc522_.PCD_Init();

  const uint8_t version = readVersion();
  const portal::Rc522Version info = portal::describeRc522Version(version);

  healthy_ = info.chip != portal::Rc522Chip::NoResponse;

  if (healthy_) {
    logf("RFID", "RC522 siap: VersionReg 0x%02X = %s", version, info.name);
  } else {
    logf("RFID", "RC522 TIDAK menjawab (VersionReg 0x%02X). Cek 3.3V, GND, SDA=GPIO%d, SCK=%d, MOSI=%d, MISO=%d, RST=%d.",
         version, PIN_RC522_SS, PIN_RC522_SCK, PIN_RC522_MOSI, PIN_RC522_MISO, PIN_RC522_RST);
  }

  return healthy_;
}

uint8_t RfidReader::readVersion() {
  return mfrc522_.PCD_ReadRegister(MFRC522::VersionReg);
}

bool RfidReader::poll(char* uidHex, size_t size) {
  if (!mfrc522_.PICC_IsNewCardPresent() || !mfrc522_.PICC_ReadCardSerial()) {
    return false;
  }

  const size_t length = portal::formatUidHex(mfrc522_.uid.uidByte, mfrc522_.uid.size, uidHex, size);

  mfrc522_.PICC_HaltA();
  mfrc522_.PCD_StopCrypto1();

  return length > 0;
}

const __FlashStringHelper* RfidReader::lastTypeName() {
  return MFRC522::PICC_GetTypeName(MFRC522::PICC_GetType(mfrc522_.uid.sak));
}

void RfidReader::healthCheck(uint32_t nowMs, uint32_t intervalMs) {
  if (nowMs - lastHealthCheckMs_ < intervalMs) {
    return;
  }

  lastHealthCheckMs_ = nowMs;

  const uint8_t version = readVersion();

  if (portal::describeRc522Version(version).chip != portal::Rc522Chip::NoResponse) {
    if (!healthy_) {
      logf("RFID", "RC522 menjawab lagi (VersionReg 0x%02X)", version);
      healthy_ = true;
    }

    return;
  }

  logf("RFID", "RC522 tidak menjawab (VersionReg 0x%02X), inisialisasi ulang", version);
  init();
}
