#pragma once

#include <cstdint>

namespace portal {

// Kapan portal yang terbuka harus ditutup lagi. Murni waktu, tanpa servo.
//
// open() saat portal masih terbuka memperpanjang batasnya menjadi
// nowMs + durationMs. Batas tidak pernah dimajukan: command open yang datang
// belakangan tidak bisa memperpendek waktu buka.
class PortalTimer {
 public:
  // Mengembalikan true kalau portal sebelumnya tertutup (baru dibuka).
  bool open(uint32_t nowMs, uint32_t durationMs) {
    const uint32_t closeAt = nowMs + durationMs;
    const bool wasClosed = !open_;

    if (wasClosed || static_cast<int32_t>(closeAt - closeAtMs_) > 0) {
      closeAtMs_ = closeAt;
    }

    open_ = true;

    return wasClosed;
  }

  // true sekali, pada panggilan pertama setelah batasnya lewat; sejak itu
  // portal dianggap tertutup.
  bool closeIfDue(uint32_t nowMs) {
    if (open_ && static_cast<int32_t>(nowMs - closeAtMs_) >= 0) {
      open_ = false;

      return true;
    }

    return false;
  }

  bool isOpen() const { return open_; }

  // Sisa waktu buka dalam ms (0 kalau tertutup atau sudah lewat).
  uint32_t remainingMs(uint32_t nowMs) const {
    if (!open_) {
      return 0;
    }

    const int32_t remaining = static_cast<int32_t>(closeAtMs_ - nowMs);

    return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
  }

 private:
  bool open_ = false;
  uint32_t closeAtMs_ = 0;
};

}  // namespace portal
