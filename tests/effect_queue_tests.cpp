#include "ZeDMDComm.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

class Link final : public ZeDMDComm {
 public:
  Link() { m_zoneStream = false; m_compression = false; m_keepAliveNotSupported = true; }
  bool IsConnected() override { return connected; }
  std::atomic<bool> connected{true};
  std::mutex mutex;
  std::condition_variable cv;
  unsigned received = 0;
  bool SendChunks(const uint8_t*, uint16_t) override { assert(false); return false; }
  bool SendFrameWithEffects(const uint8_t* data, uint16_t size, const uint8_t* effects) override {
    std::lock_guard<std::mutex> lock(mutex);
    assert(size == 4);
    const unsigned id = unsigned(data[0]) | (unsigned(data[1]) << 8);
    assert(id == received);
    assert(ZeDMDLightPacket::Valid(effects));
    assert(effects[5] == (id % 2 ? 0 : 0x10));
    if (effects[5]) assert(effects[8+21*4+4] == uint8_t(id));
    ++received;
    cv.notify_one();
    return true;
  }
};
int main() {
  ZeDMDLightPacket p;
  p.Seal();
  assert(ZeDMDLightPacket::Valid(p.bytes));
  // Golden empty-packet CRC from Python zlib.crc32, not from this implementation.
  assert(p.bytes[188] == 0xd7 && p.bytes[189] == 0xd2 && p.bytes[190] == 0xce && p.bytes[191] == 0xa9);
  for (unsigned i = 0; i < sizeof(p.bytes); ++i) {
    p.bytes[i] ^= 1;
    assert(!ZeDMDLightPacket::Valid(p.bytes));
    p.bytes[i] ^= 1;
  }
  p.bytes[5] = 0xff;
  for (unsigned i = 0; i < 8; ++i) p.bytes[8+21*i] = i % 4;
  p.Seal(); assert(ZeDMDLightPacket::Valid(p.bytes));
  p.bytes[8] = 9; p.Seal(); assert(!ZeDMDLightPacket::Valid(p.bytes));
  Link link;
  link.Run();
  for (unsigned id = 0; id < 1000; ++id) {
    uint8_t frame[4] = {uint8_t(id), uint8_t(id >> 8), 0, 0};
    ZeDMDLightPacket effects;
    if (!(id % 2)) { effects.bytes[5] = 0x10; effects.bytes[8+21*4] = 0; effects.bytes[8+21*4+4] = uint8_t(id); }
    link.QueueFrameWithEffects(frame, sizeof(frame), effects);
    // Queue must own both buffers; producer changes cannot affect the wire.
    memset(frame, 0xff, sizeof(frame)); memset(effects.bytes, 0xff, sizeof(effects.bytes));
  }
  {
    std::unique_lock<std::mutex> lock(link.mutex);
    assert(link.cv.wait_for(lock, std::chrono::seconds(5), [&] { return link.received == 1000; }));
  }
  link.connected = false;
}
