#pragma once

#include <cstdint>

#include "ZeDMDComm.h"

#if defined(__linux__)
#include <gpiod.h>
#include <linux/spi/spidev.h>
#else
// Forward declarations so non-Linux builds can compile the stub implementation.
struct gpiod_chip;
struct gpiod_line;
#endif

#define GPIO_CHIP "/dev/gpiochip0"
#define SPI_DEVICE "/dev/spidev1.0"

// can be read from /sys/module/spidev/parameters/bufsiz, but hardcoded now for simplicity
constexpr int spi_kernel_bufsize = 4096;
constexpr uint32_t spi_default_speed_hz = 12000000;

class ZeDMDSpi : public ZeDMDComm
{
 public:
  ZeDMDSpi() : ZeDMDComm() {}
  ~ZeDMDSpi() { Disconnect(); }

  bool Connect() override;
  void Disconnect() override;
  bool IsConnected() override;

  void SetWidth(uint16_t width) { m_width = width; }
  void SetHeight(uint16_t height) { m_height = height; }

 protected:
  bool SendChunks(uint8_t* pData, uint16_t size) override;
  void Reset() override;

 private:
  bool IsSupportedPlatform() const;

  uint32_t m_speed = spi_default_speed_hz;
  int m_fileDescriptor = -1;
#if defined(__linux__)
  gpiod_chip* m_gpioChip = nullptr;
  gpiod_line* m_csLine = nullptr;
#endif
  bool m_connected = false;
  bool m_keepAlive = false;
};
