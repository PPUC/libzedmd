#pragma once
#include <stdint.h>
#include <string.h>

// SPI v1: RGB565 frame, then exactly 192 bytes, even with no light changes.
// Bytes 0..3: LEFX; 4: version; 5: channel mask; 6..7: reserved zero.
// Eight 21-byte payloads at offset 8: defaults/reset first, then live effects.
// Bytes 176..187 reserved zero; CRC32 at 188, little endian.
struct ZeDMDLightPacket {
  static constexpr unsigned Size = 192;
  uint8_t bytes[Size] = {};
  ZeDMDLightPacket() { memcpy(bytes, "LEFX", 4); bytes[4] = 1; }
  static uint32_t Crc(const uint8_t* p) {
    uint32_t crc = 0xffffffffu;
    for (unsigned i = 0; i < 188; ++i) {
      crc ^= p[i];
      for (unsigned b = 0; b < 8; ++b)
        crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
  }
  void Seal() {
    const uint32_t crc = Crc(bytes);
    for (unsigned i = 0; i < 4; ++i) bytes[188+i] = uint8_t(crc >> (8*i));
  }
  static bool Valid(const uint8_t* p) {
    if (memcmp(p, "LEFX", 4) || p[4] != 1 || p[6] || p[7]) return false;
    const uint32_t crc = Crc(p);
    for (unsigned i = 0; i < 4; ++i) if (p[188+i] != uint8_t(crc >> (8*i))) return false;
    for (unsigned i = 0; i < 8; ++i)
      if ((p[5] & (1u << i)) && p[8+21*i] != (i % 4)) return false;
    return true;
  }
};
