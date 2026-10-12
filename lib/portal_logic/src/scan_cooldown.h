#pragma once

#include <cstddef>
#include <cstdint>

#include "uid_hex.h"

namespace portal {

// Cooldown per UID, supaya kartu yang ditahan di depan reader (atau terbaca
// berulang) tidak terkirim berkali-kali.
//
// Jendelanya bergeser: setiap deteksi memperbarui waktu terakhir UID itu,
// juga deteksi yang ditolak. Kartu yang terus ditahan baru terkirim lagi
// setelah ia tidak terbaca selama cooldownMs. UID lain tidak terpengaruh.
//
// Waktu dalam millis(); selisihnya dihitung dengan aritmetika unsigned, jadi
// tetap benar ketika millis() kembali ke nol setelah ~49 hari.
class ScanCooldown {
 public:
  static constexpr size_t kSlots = 8;

  explicit ScanCooldown(uint32_t cooldownMs) : cooldownMs_(cooldownMs) {}

  // true kalau scan UID ini boleh dikirim sekarang.
  bool allow(const char* uidHex, uint32_t nowMs);

 private:
  struct Slot {
    char uid[kUidHexSize];
    uint32_t lastSeenMs;
    bool used;
  };

  uint32_t cooldownMs_;
  Slot slots_[kSlots] = {};
};

}  // namespace portal
