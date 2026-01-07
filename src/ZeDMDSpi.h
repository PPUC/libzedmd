#pragma once

#if defined(__linux__) && defined(__aarch64__) && !defined(__ANDROID__)
#define SPI_SUPPORT 1
#endif

#include <cstdint>

#include "ZeDMDComm.h"

#if defined(SPI_SUPPORT)
#include <linux/spi/spidev.h>

#define SPI_DEVICE "/dev/spidev1.0"
#endif

class ZeDMDSpi : public ZeDMDComm
{
 public:
  ZeDMDSpi() : ZeDMDComm()
  {
    m_compression = false;
    m_zoneStream = false;
    m_keepAliveNotSupported = true;
  }
  ~ZeDMDSpi() { Disconnect(); }

  bool Connect() override;
  void Disconnect() override;
  bool IsConnected() override;
  using ZeDMDComm::QueueCommand;
  void QueueCommand(char command, uint8_t* buffer, int size) override;

  void SetSpeed(uint32_t speed) { m_speed = speed; }
  void SetFramePause(uint8_t pause) { m_framePause = pause; }
  void SetWidth(uint16_t width) { m_width = width; }
  void SetHeight(uint16_t height) { m_height = height; }

 protected:
  bool SendChunks(const uint8_t* pData, uint16_t size) override;
  void Reset() override;

 private:
  bool IsSupportedPlatform() const;

  uint32_t m_speed = 72000000;  // 72MHz
  uint8_t m_framePause = 2;     // 2ms
  int m_fileDescriptor = -1;
  bool m_connected = false;
};
