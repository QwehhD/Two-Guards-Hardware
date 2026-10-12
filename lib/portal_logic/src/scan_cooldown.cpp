#include "scan_cooldown.h"

#include <cstring>

namespace portal {

bool ScanCooldown::allow(const char* uidHex, uint32_t nowMs) {
  for (Slot& slot : slots_) {
    if (slot.used && strcmp(slot.uid, uidHex) == 0) {
      const bool allowed = nowMs - slot.lastSeenMs >= cooldownMs_;
      slot.lastSeenMs = nowMs;

      return allowed;
    }
  }

  // UID baru: pakai slot kosong, atau timpa UID yang paling lama tidak terlihat.
  Slot* victim = &slots_[0];

  for (Slot& slot : slots_) {
    if (!slot.used) {
      victim = &slot;
      break;
    }

    if (nowMs - slot.lastSeenMs > nowMs - victim->lastSeenMs) {
      victim = &slot;
    }
  }

  strncpy(victim->uid, uidHex, kUidHexSize - 1);
  victim->uid[kUidHexSize - 1] = '\0';
  victim->lastSeenMs = nowMs;
  victim->used = true;

  return true;
}

}  // namespace portal
