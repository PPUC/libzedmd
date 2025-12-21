#include "ZeDMDSpi.h"

#if defined(SPI_SUPPORT)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

namespace
{
constexpr uint8_t kSpiBitsPerWord = 8;
constexpr uint8_t kSpiMode = SPI_MODE_0;
constexpr unsigned int kCsGpio = 25;  // GPIO25 on Raspberry Pi
constexpr const char kGpioConsumer[] = "ZeDMDSpi";
}  // namespace

bool ZeDMDSpi::IsSupportedPlatform() const
{
  std::ifstream model("/proc/device-tree/model");
  if (!model.is_open())
  {
    return false;
  }
  std::string line;
  std::getline(model, line);
  return line.find("Raspberry Pi") != std::string::npos;
}

bool ZeDMDSpi::Connect()
{
  if (!IsSupportedPlatform())
  {
    Log("ZeDMDSpi: unsupported platform. This transport only runs on Raspberry Pi with Linux.");
    return false;
  }

  if (m_connected)
  {
    return true;
  }

  m_fileDescriptor = open(SPI_DEVICE, O_RDWR | O_CLOEXEC);
  if (m_fileDescriptor < 0)
  {
    Log("ZeDMDSpi: couldn't open SPI device %s: %s", SPI_DEVICE, strerror(errno));
    return false;
  }
  Log("ZeDMDSpi: opened SPI device %s", SPI_DEVICE);

  if (ioctl(m_fileDescriptor, SPI_IOC_WR_MODE, &kSpiMode) < 0)
  {
    Log("ZeDMDSpi: couldn't set SPI mode: %s", strerror(errno));
    Disconnect();
    return false;
  }
  // Disable kernel chip-select handling; we drive CS manually on GPIO25.
#if defined(SPI_NO_CS) && defined(SPI_IOC_WR_MODE32)
  uint32_t mode32 = kSpiMode | SPI_NO_CS;
  if (ioctl(m_fileDescriptor, SPI_IOC_WR_MODE32, &mode32) < 0)
  {
    Log("ZeDMDSpi: warning - couldn't set SPI_NO_CS: %s", strerror(errno));
  }
#endif

  uint8_t bitsPerWord = kSpiBitsPerWord;
  if (ioctl(m_fileDescriptor, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord) < 0)
  {
    Log("ZeDMDSpi: couldn't set SPI bits/word: %s", strerror(errno));
    Disconnect();
    return false;
  }

  if (ioctl(m_fileDescriptor, SPI_IOC_WR_MAX_SPEED_HZ, &m_speed) < 0)
  {
    Log("ZeDMDSpi: couldn't set SPI speed: %s", strerror(errno));
    Disconnect();
    return false;
  }
  Log("ZeDMDSpi: set SPI speed %d", m_speed);

  m_gpioChip = gpiod_chip_open(GPIO_CHIP);
  if (!m_gpioChip)
  {
    Log("ZeDMDSpi: couldn't open gpio chip %s: %s", GPIO_CHIP, strerror(errno));
    Disconnect();
    return false;
  }

  m_csLine = gpiod_chip_get_line(m_gpioChip, kCsGpio);
  if (!m_csLine)
  {
    Log("ZeDMDSpi: couldn't get CS gpio %d: %s", kCsGpio, strerror(errno));
    Disconnect();
    return false;
  }

  if (gpiod_line_request_output(m_csLine, kGpioConsumer, 1) < 0)
  {
    Log("ZeDMDSpi: couldn't request CS gpio %d as output: %s", kCsGpio, strerror(errno));
    Disconnect();
    return false;
  }

  // Create a rising edge to switch ZeDMD from loopback to SPI mode.
  // Keep CS high when idle.
  gpiod_line_set_value(m_csLine, 0);
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  gpiod_line_set_value(m_csLine, 1);

  Log("ZeDMDSpi: signaling via GPIO %d established", kCsGpio);

  m_connected = true;
  return true;
}

void ZeDMDSpi::Disconnect()
{
  if (m_csLine)
  {
    gpiod_line_set_value(m_csLine, 1);
    gpiod_line_release(m_csLine);
    m_csLine = nullptr;
  }
  if (m_gpioChip)
  {
    gpiod_chip_close(m_gpioChip);
    m_gpioChip = nullptr;
  }
  if (m_fileDescriptor >= 0)
  {
    close(m_fileDescriptor);
    m_fileDescriptor = -1;
  }
  m_connected = false;
}

bool ZeDMDSpi::IsConnected() { return m_connected; }

void ZeDMDSpi::Reset() {}

bool ZeDMDSpi::SendChunks(uint8_t* pData, uint16_t size)
{
  if (!m_connected || m_fileDescriptor < 0)
  {
    Log("ZeDMDSpi: device not connected");
    return false;
  }

  uint32_t remaining = size;
  uint8_t* cursor = pData;

  if (m_csLine && gpiod_line_set_value(m_csLine, 0) < 0)
  {
    Log("ZeDMDSpi: failed to pull CS low: %s", strerror(errno));
    return false;
  }

  std::this_thread::sleep_for(std::chrono::microseconds(10));

  while (remaining > 0)
  {
    uint32_t chunkSize = std::min<uint32_t>(remaining, spi_kernel_bufsize);
    struct spi_ioc_transfer transfer;
    memset(&transfer, 0, sizeof(transfer));
    transfer.tx_buf = reinterpret_cast<__u64>(cursor);
    transfer.len = chunkSize;
    transfer.speed_hz = m_speed;
    transfer.bits_per_word = kSpiBitsPerWord;

    int res = ioctl(m_fileDescriptor, SPI_IOC_MESSAGE(1), &transfer);
    if (res < 0)
    {
      Log("ZeDMDSpi: SPI write failed: %s", strerror(errno));
      if (m_csLine) gpiod_line_set_value(m_csLine, 1);
      return false;
    }
    // Log("ZeDMDSpi: send chunk of %d bytes", chunkSize);
    cursor += chunkSize;
    remaining -= chunkSize;
  }

  if (m_csLine && gpiod_line_set_value(m_csLine, 1) < 0)
  {
    Log("ZeDMDSpi: failed to release CS: %s", strerror(errno));
    return false;
  }

  std::this_thread::sleep_for(std::chrono::microseconds(100));

  return true;
}

#else  // non-Linux or non-aarch64 stub

bool ZeDMDSpi::IsSupportedPlatform() const { return false; }

bool ZeDMDSpi::Connect()
{
  Log("ZeDMDSpi: unsupported platform. SPI only runs on Raspberry Pi with Linux.");
  return false;
}

void ZeDMDSpi::Disconnect() {}

bool ZeDMDSpi::IsConnected() { return false; }

void ZeDMDSpi::Reset() {}

bool ZeDMDSpi::SendChunks(uint8_t*, uint16_t) { return false; }

#endif  // __linux__
