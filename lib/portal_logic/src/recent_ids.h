#pragma once

#include <cstddef>
#include <cstdint>

namespace portal {

// access_log_id yang terakhir diproses. QoS 1 dan retry server berarti command
// yang sama bisa datang lebih dari sekali; yang kedua dan seterusnya diabaikan.
// Kapasitas tetap: ID paling lama tertimpa begitu buffer penuh.
template <size_t Capacity>
class RecentIds {
  static_assert(Capacity > 0, "RecentIds butuh kapasitas minimal 1");

 public:
  bool contains(int64_t id) const {
    for (size_t i = 0; i < count_; i++) {
      if (ids_[i] == id) {
        return true;
      }
    }

    return false;
  }

  void add(int64_t id) {
    ids_[next_] = id;
    next_ = (next_ + 1) % Capacity;

    if (count_ < Capacity) {
      count_++;
    }
  }

 private:
  int64_t ids_[Capacity] = {};
  size_t next_ = 0;
  size_t count_ = 0;
};

}  // namespace portal
