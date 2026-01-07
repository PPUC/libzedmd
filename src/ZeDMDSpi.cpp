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
#include <vector>

namespace
{
constexpr uint8_t kSpiBitsPerWord = 8;
constexpr uint8_t kSpiMode = SPI_MODE_0;
constexpr const char kSpiBufSizePath[] = "/sys/module/spidev/parameters/bufsiz";

uint32_t GetSpiKernelBufSize()
{
  static uint32_t cachedBufSize = 0;
  if (cachedBufSize != 0)
  {
    return cachedBufSize;
  }

  std::ifstream bufFile(kSpiBufSizePath);
  uint32_t parsed = 0;
  if (bufFile.is_open() && (bufFile >> parsed) && parsed > 0)
  {
    cachedBufSize = parsed;
    return cachedBufSize;
  }

  cachedBufSize = 4096;  // fallback if sysfs entry is unavailable
  return cachedBufSize;
}
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

  m_connected = true;
  // Create a short CS signal to switch ZeDMD from loopback to SPI mode.
  const uint8_t dummy[4] = {0};
  if (!SendChunks(dummy, 4)) {
    m_connected = false;
  }

  return m_connected;
}

void ZeDMDSpi::Disconnect()
{
  if (m_fileDescriptor >= 0)
  {
    close(m_fileDescriptor);
    m_fileDescriptor = -1;
  }
  m_connected = false;
}

bool ZeDMDSpi::IsConnected() { return m_connected; }

void ZeDMDSpi::Reset() {}

void ZeDMDSpi::QueueCommand(char command, uint8_t* buffer, int size)
{
  switch (command)
  {
    case ZEDMD_COMM_COMMAND::ClearScreen:
      SendChunks(m_allBlack, GetWidth() * GetHeight() * 2);  // RGB565
      break;

    default:
      // ZeDMDComm::QueueCommand(command, buffer, size);
      // Drop commands other than ClearScreen for SPI transport for now.
      break;
  }
}

bool ZeDMDSpi::SendChunks(const uint8_t* pData, uint16_t size)
{
  if (!m_connected || m_fileDescriptor < 0)
  {
    Log("ZeDMDSpi: device not connected");
    return false;
  }

  uint32_t remaining = size;
  uint8_t* cursor = (uint8_t*)pData;

  std::this_thread::sleep_for(std::chrono::microseconds(10));
  const uint32_t spi_kernel_bufsize = GetSpiKernelBufSize();

  if (remaining <= spi_kernel_bufsize)
  {
    struct spi_ioc_transfer transfer;
    memset(&transfer, 0, sizeof(transfer));
    transfer.tx_buf = reinterpret_cast<__u64>(cursor);
    transfer.len = remaining;
    transfer.speed_hz = m_speed;
    transfer.bits_per_word = kSpiBitsPerWord;

    int res = ioctl(m_fileDescriptor, SPI_IOC_MESSAGE(1), &transfer);
    if (res < 0)
    {
      Log("ZeDMDSpi: SPI write failed: %s", strerror(errno));
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      return false;
    }

    const uint32_t bytesTransferred = static_cast<uint32_t>(res);
    if (bytesTransferred != remaining)
    {
      Log("ZeDMDSpi: partial SPI write (%u/%u bytes)", bytesTransferred, remaining);
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      return false;
    }

    if (m_verbose) Log("SendChunks, transferred %d, remaining %d", bytesTransferred, 0);
  }
  else
  {
    std::vector<spi_ioc_transfer> transfers;
    transfers.reserve((remaining + spi_kernel_bufsize - 1) / spi_kernel_bufsize);

    while (remaining > 0)
    {
      uint32_t chunkSize = std::min<uint32_t>(remaining, spi_kernel_bufsize);
      struct spi_ioc_transfer transfer;
      memset(&transfer, 0, sizeof(transfer));
      transfer.tx_buf = reinterpret_cast<__u64>(cursor);
      transfer.len = chunkSize;
      transfer.speed_hz = m_speed;
      transfer.bits_per_word = kSpiBitsPerWord;
      transfers.push_back(transfer);

      cursor += chunkSize;
      remaining -= chunkSize;
    }

    int res = ioctl(m_fileDescriptor, SPI_IOC_MESSAGE(transfers.size()), transfers.data());
    if (res < 0)
    {
      Log("ZeDMDSpi: SPI write failed: %s", strerror(errno));
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      return false;
    }

    const uint32_t bytesTransferred = static_cast<uint32_t>(res);
    const uint32_t expected = size;
    if (bytesTransferred != expected)
    {
      Log("ZeDMDSpi: partial SPI write (%u/%u bytes)", bytesTransferred, expected);
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      return false;
    }

    if (m_verbose) Log("SendChunks, transferred %d, remaining %d", bytesTransferred, 0);
  }

  if (m_framePause > 0)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(m_framePause));
  }
  else
  {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }

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

void ZeDMDSpi::QueueCommand(char command, uint8_t* buffer, int size) { ZeDMDComm::QueueCommand(command, buffer, size); }

bool ZeDMDSpi::SendChunks(const uint8_t*, uint16_t) { return false; }

#endif  // __linux__
