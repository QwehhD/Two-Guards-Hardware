#pragma once

#include <cstdint>

namespace portal {

// Jeda antar-percobaan sambung ulang: mulai dari minMs, berlipat dua tiap
// kegagalan, berhenti naik di maxMs. reset() setelah berhasil.
class Backoff {
 public:
  Backoff(uint32_t minMs, uint32_t maxMs) : minMs_(minMs), maxMs_(maxMs), nextMs_(minMs) {}

  // Jeda untuk percobaan berikutnya, lalu naikkan jeda sesudahnya.
  uint32_t next() {
    const uint32_t delay = nextMs_;
    nextMs_ = nextMs_ > maxMs_ / 2 ? maxMs_ : nextMs_ * 2;

    return delay;
  }

  void reset() { nextMs_ = minMs_; }

 private:
  uint32_t minMs_;
  uint32_t maxMs_;
  uint32_t nextMs_;
};

}  // namespace portal
