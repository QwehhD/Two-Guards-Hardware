#include "uid_hex.h"

namespace portal {

size_t formatUidHex(const uint8_t* uid, size_t len, char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) {
    return 0;
  }

  out[0] = '\0';

  if (uid == nullptr || len == 0 || len > kMaxUidBytes || outSize < len * 2 + 1) {
    return 0;
  }

  static const char kDigits[] = "0123456789ABCDEF";

  for (size_t i = 0; i < len; i++) {
    out[i * 2] = kDigits[uid[i] >> 4];
    out[i * 2 + 1] = kDigits[uid[i] & 0x0F];
  }

  out[len * 2] = '\0';

  return len * 2;
}

}  // namespace portal
