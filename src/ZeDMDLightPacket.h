#pragma once
#include <stdint.h>
#include <string.h>

// SPI v1: RGB565 frame, then exactly 192 bytes, even with no light changes.
// Bytes 0..3: LEFX; 4: version; 5: slot mask; 6..7: reserved zero.
// Eight 21-byte payloads at offset 8: per channel 0..3 a baseline slot (a
// default, a reset, or a reset followed by a default), then per channel a live
// effect. A receiver applies all baseline slots, then all live slots.
// Bytes 176..187 reserved zero; CRC32 at 188, little endian.
//
// The same file is in libzedmd and in the PPUCDMD firmware. Keep both identical.
struct ZeDMDLightPacket {
  static constexpr unsigned Size = 192;
  static constexpr unsigned SlotSize = 21;
  static constexpr uint8_t FlagDefault = 0x01;
  static constexpr uint8_t FlagReset = 0x02;
  static constexpr uint8_t AllChannels = 0xff;

  uint8_t bytes[Size] = {};
  ZeDMDLightPacket() { memcpy(bytes, "LEFX", 4); bytes[4] = 1; }

  uint8_t* Slot(unsigned slot) { return bytes + 8 + SlotSize * slot; }
  const uint8_t* Slot(unsigned slot) const { return bytes + 8 + SlotSize * slot; }
  bool Empty() const { return bytes[5] == 0; }
  void Clear() { *this = ZeDMDLightPacket{}; }

  // Half a byte per step: fast enough for the receiver's interrupt handler,
  // without a 1 KiB table.
  static uint32_t Crc(const uint8_t* p) {
    static constexpr uint32_t table[16] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c};
    uint32_t crc = 0xffffffffu;
    for (unsigned i = 0; i < 188; ++i) {
      crc ^= p[i];
      crc = (crc >> 4) ^ table[crc & 0x0f];
      crc = (crc >> 4) ^ table[crc & 0x0f];
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
    for (unsigned i = 0; i < 8; ++i) {
      if (!(p[5] & (1u << i))) continue;
      const uint8_t* slot = p + 8 + SlotSize * i;
      if (slot[0] != (i % 4)) return false;
      // A baseline slot carries a default and/or a reset, a live slot neither.
      const uint8_t flags = slot[1];
      if (i < 4 ? (flags == 0 || (flags & ~(FlagDefault | FlagReset))) : flags != 0) return false;
    }
    return true;
  }

  // Merge a later packet into this one. Applied as described above, the result
  // leaves every channel as applying this packet and then the later one would:
  // a reset drops the live effect queued before it, and a default that follows
  // a reset keeps the reset.
  void Merge(const ZeDMDLightPacket& later) {
    for (unsigned channel = 0; channel < 4; ++channel) {
      const uint8_t baseline = uint8_t(1u << channel);
      const uint8_t live = uint8_t(1u << (channel + 4));
      if (later.bytes[5] & baseline) {
        const uint8_t* src = later.Slot(channel);
        uint8_t* dst = Slot(channel);
        const bool keepReset = (bytes[5] & baseline) && (dst[1] & FlagReset);
        if (src[1] & FlagReset) bytes[5] &= uint8_t(~live);
        memcpy(dst, src, SlotSize);
        if (keepReset) dst[1] |= FlagReset;
        bytes[5] |= baseline;
      }
      if (later.bytes[5] & live) {
        memcpy(Slot(channel + 4), later.Slot(channel + 4), SlotSize);
        bytes[5] |= live;
      }
    }
  }

  // Add one SetLightEffect payload (21 bytes, see ZeDMD::SetLightEffect) for a
  // channel or AllChannels, merged as above.
  void Add(const uint8_t* payload) {
    ZeDMDLightPacket change;
    for (unsigned channel = 0; channel < 4; ++channel) {
      if (payload[0] != AllChannels && payload[0] != channel) continue;
      const unsigned slot = channel + ((payload[1] & (FlagDefault | FlagReset)) ? 0 : 4);
      memcpy(change.Slot(slot), payload, SlotSize);
      change.Slot(slot)[0] = uint8_t(channel);
      change.bytes[5] |= uint8_t(1u << slot);
    }
    Merge(change);
  }
};
