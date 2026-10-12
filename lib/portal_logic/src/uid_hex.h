#pragma once

#include <cstddef>
#include <cstdint>

namespace portal {

// UID kartu ISO 14443A paling panjang 10 byte (triple size).
constexpr size_t kMaxUidBytes = 10;

// Cukup untuk UID terpanjang: 2 karakter per byte + NUL.
constexpr size_t kUidHexSize = kMaxUidBytes * 2 + 1;

// Format UID yang dikirim ke server: hex huruf besar tanpa pemisah, semua
// byte sesuai panjang UID, nol di depan tiap byte dipertahankan.
// {0x04, 0xA1, 0x0B, 0x3C} -> "04A10B3C".
//
// Mengembalikan panjang string. Untuk len 0, len di atas kMaxUidBytes, atau
// buffer yang terlalu kecil, out diisi string kosong dan hasilnya 0.
size_t formatUidHex(const uint8_t* uid, size_t len, char* out, size_t outSize);

}  // namespace portal
