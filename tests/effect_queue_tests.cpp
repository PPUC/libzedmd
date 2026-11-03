#include "ZeDMDComm.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

static ZeDMDLightPacket Payload(uint8_t channel, uint8_t flags, uint8_t mode) {
  uint8_t payload[ZeDMDLightPacket::SlotSize] = {channel, flags};
  payload[4] = mode;
  payload[20] = 255;
  ZeDMDLightPacket p;
  p.Add(payload);
  return p;
}

class Link final : public ZeDMDComm {
 public:
  Link() { m_zoneStream = false; m_compression = false; m_keepAliveNotSupported = true; }
  bool IsConnected() override { return connected; }
  std::atomic<bool> connected{true};
  std::mutex mutex;
  std::condition_variable cv;
  int last = -1;
  unsigned received = 0;
  bool sawDefault = false;
  bool SendChunks(const uint8_t*, uint16_t) override { assert(false); return false; }
  bool SendFrameWithEffects(const uint8_t* data, uint16_t size, const uint8_t* effects) override {
    // A slow link, so that producers queue up behind it.
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    std::lock_guard<std::mutex> lock(mutex);
    assert(size == 4);
    const int id = int(data[0]) | (int(data[1]) << 8);
    // Only the newest waiting picture goes out, in order.
    assert(id > last);
    last = id;
    assert(ZeDMDLightPacket::Valid(effects));
    // The newest live effect of the frames merged into this one belongs to this frame.
    assert(effects[5] & 0x10);
    assert(effects[8+21*4+4] == uint8_t(id));
    if (effects[5] & 0x01) { assert(effects[8+1] == ZeDMDLightPacket::FlagDefault); sawDefault = true; }
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

  // Slot checks: channel, a baseline slot needs a flag, a live slot must not have one.
  p.bytes[5] = 0xff;
  for (unsigned i = 0; i < 8; ++i) { p.Slot(i)[0] = i % 4; p.Slot(i)[1] = i < 4 ? ZeDMDLightPacket::FlagDefault : 0; }
  p.Seal(); assert(ZeDMDLightPacket::Valid(p.bytes));
  p.Slot(0)[0] = 9; p.Seal(); assert(!ZeDMDLightPacket::Valid(p.bytes)); p.Slot(0)[0] = 0;
  p.Slot(1)[1] = 0; p.Seal(); assert(!ZeDMDLightPacket::Valid(p.bytes)); p.Slot(1)[1] = 3;
  p.Seal(); assert(ZeDMDLightPacket::Valid(p.bytes));
  p.Slot(5)[1] = 1; p.Seal(); assert(!ZeDMDLightPacket::Valid(p.bytes));

  const uint8_t D = ZeDMDLightPacket::FlagDefault, R = ZeDMDLightPacket::FlagReset;
  {
    // A live effect, then a reset: the reset wins.
    ZeDMDLightPacket m = Payload(1, 0, 7);
    m.Merge(Payload(1, R, 0));
    assert(m.bytes[5] == 0x02 && m.Slot(1)[1] == R);
  }
  {
    // A reset, then a default: both, the reset first.
    ZeDMDLightPacket m = Payload(2, R, 0);
    m.Merge(Payload(2, D, 9));
    assert(m.bytes[5] == 0x04 && m.Slot(2)[1] == (R | D) && m.Slot(2)[4] == 9);
  }
  {
    // A reset, then a live effect: both.
    ZeDMDLightPacket m = Payload(0, R, 0);
    m.Merge(Payload(0, 0, 5));
    assert(m.bytes[5] == 0x11 && m.Slot(0)[1] == R && m.Slot(4)[4] == 5);
  }
  {
    // A default, then a reset: only the reset.
    ZeDMDLightPacket m = Payload(3, D, 4);
    m.Merge(Payload(3, R, 0));
    assert(m.bytes[5] == 0x08 && m.Slot(3)[1] == R);
  }
  {
    // A live effect, then a default: both, the live effect stays.
    ZeDMDLightPacket m = Payload(0, 0, 6);
    m.Merge(Payload(0, D, 2));
    assert(m.bytes[5] == 0x11 && m.Slot(0)[1] == D && m.Slot(4)[4] == 6);
  }
  {
    // All channels at once, and other channels are left alone.
    ZeDMDLightPacket m = Payload(ZeDMDLightPacket::AllChannels, 0, 8);
    assert(m.bytes[5] == 0xf0);
    for (unsigned c = 0; c < 4; ++c) assert(m.Slot(4 + c)[0] == c);
    m.Merge(Payload(2, R, 0));
    assert(m.bytes[5] == (0xf0 & ~0x40) + 0x04);
    m.Seal(); assert(ZeDMDLightPacket::Valid(m.bytes));
  }

  Link link;
  link.Run();
  for (unsigned id = 0; id < 1000; ++id) {
    uint8_t frame[4] = {uint8_t(id), uint8_t(id >> 8), 0, 0};
    ZeDMDLightPacket effects = Payload(0, 0, uint8_t(id));
    if (id == 500) effects.Merge(Payload(0, D, 1));
    link.QueueFrameWithEffects(frame, sizeof(frame), effects);
    // Queue must own both buffers; producer changes cannot affect the wire.
    memset(frame, 0xff, sizeof(frame)); memset(effects.bytes, 0xff, sizeof(effects.bytes));
    if (id % 50 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  {
    std::unique_lock<std::mutex> lock(link.mutex);
    assert(link.cv.wait_for(lock, std::chrono::seconds(5), [&] { return link.last == 999; }));
    // Frames were merged while the link was busy, and the default sent with one of them was not lost.
    assert(link.received < 1000);
    assert(link.sawDefault);
  }
  link.connected = false;
}
