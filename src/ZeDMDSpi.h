#pragma once

#if defined(__linux__) && defined(__aarch64__) && !defined(__ANDROID__)
#define SPI_SUPPORT 1
#endif

#include <cstdint>

#include "ZeDMDComm.h"

#if defined(SPI_SUPPORT)
#include <gpiod.h>
#include <linux/spi/spidev.h>

#define GPIO_CHIP "/dev/gpiochip0"
#define SPI_DEVICE "/dev/spidev1.0"
#else
// Forward declarations so non-Linux builds can compile the stub implementation.
struct gpiod_chip;
struct gpiod_line;
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
  void SetWidth(uint16_t width) { m_width = width; }
  void SetHeight(uint16_t height) { m_height = height; }

 protected:
  bool SendChunks(uint8_t* pData, uint16_t size) override;
  void Reset() override;

 private:
  bool IsSupportedPlatform() const;

  uint32_t m_speed = 24000000;  // 24 MHz
  int m_fileDescriptor = -1;
#if defined(SPI_SUPPORT)
  gpiod_chip* m_gpioChip = nullptr;
  gpiod_line* m_csLine = nullptr;
#endif
  bool m_connected = false;
};
