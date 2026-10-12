#pragma once

#include <cstdint>

namespace portal {

enum class Rc522Chip : uint8_t {
  // 0x00 atau 0xFF: chip tidak menjawab lewat SPI (wiring, daya, atau pin SS/RST).
  NoResponse,
  Genuine,
  // Klon yang umum di modul murah; tetap bisa membaca UID dengan normal.
  Clone,
  Unknown,
};

struct Rc522Version {
  Rc522Chip chip;
  const char* name;
};

// Arti nilai VersionReg (0x37) MFRC522.
inline Rc522Version describeRc522Version(uint8_t version) {
  switch (version) {
    case 0x00:
    case 0xFF:
      return {Rc522Chip::NoResponse, "tidak ada jawaban"};
    case 0x91:
      return {Rc522Chip::Genuine, "MFRC522 v1.0"};
    case 0x92:
      return {Rc522Chip::Genuine, "MFRC522 v2.0"};
    case 0x88:
      return {Rc522Chip::Clone, "klon FM17522"};
    case 0x12:
      return {Rc522Chip::Clone, "klon (0x12)"};
    case 0xB2:
      return {Rc522Chip::Clone, "klon FM17522 (0xB2)"};
    case 0x82:
      // Modul di project ini: nilainya sama di setiap boot dan UID terbaca normal.
      return {Rc522Chip::Clone, "klon (0x82)"};
    default:
      return {Rc522Chip::Unknown, "versi tidak dikenal"};
  }
}

}  // namespace portal
