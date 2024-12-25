#pragma once

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
#include "libserialport.h"
#endif

#include <inttypes.h>
#include <stdarg.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#ifdef _MSC_VER
#define ZEDMDCALLBACK __stdcall
#else
#define ZEDMDCALLBACK
#endif

#define ZEDMD_COMM_BAUD_RATE 921600
#define ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE 1888

#define ZEDMD_S3_COMM_BAUD_RATE 2000000
#define ZEDMD_S3_COMM_MAX_SERIAL_WRITE_AT_ONCE 256

#define ZEDMD_COMM_SERIAL_READ_TIMEOUT 16
#define ZEDMD_COMM_SERIAL_WRITE_TIMEOUT 8
#define ZEDMD_COMM_NUM_TIMEOUTS_TO_WAIT_FOR_ACKNOWLEDGE 3
#define ZEDMD_COMM_FRAME_QUEUE_SIZE_MAX 8

// Typically, the MTU is 1480 (1500 - 20 byte header).
// 1460 is safe. For UART or USB CDC we use the same limit since the ZeDMD firmware is unified.
// For USB UART 128x32 send one row (16 zones).
#define ZEDMD_ZONES_BYTE_LIMIT (128 * 4 * 2 + 16)

typedef enum
{
  FrameSize = 0x02,
  Handshake = 0x0c,
  LEDTest = 0x10,
  EnableUpscaling = 0x15,
  DisableUpscaling = 0x14,
  Brightness = 0x16,
  RGBOrder = 0x17,
  GetBrightness = 0x18,
  GetRGBOrder = 0x19,
  SetWiFiSSID = 0x1b,
  SetWiFiPassword = 0x1c,
  SetWiFiPort = 0x1d,
  SaveSettings = 0x1e,
  Reset = 0x1f,
  GetVersionBytes = 0x20,
  GetResolution = 0x21,

  AnnounceRGB565ZonesStream = 0x04,
  RGB565ZonesStream = 0x05,
  RenderRGB565Frame = 0x06,

  ClearScreen = 0x0a,

  DisableDebug = 0x62,
  EnableDebug = 0x63,
} ZEDMD_COMM_COMMAND;

struct ZeDMDFrameData
{
  uint8_t* data;
  int size;

  // Default constructor
  ZeDMDFrameData(int sz = 0) : size(sz), data((sz > 0) ? new uint8_t[sz] : nullptr) {}

  // Constructor to copy data
  ZeDMDFrameData(uint8_t* d, int sz = 0) : size(sz), data((sz > 0) ? new uint8_t[sz] : nullptr)
  {
    if (sz > 0) memcpy(data, d, sz);
  }

  // Destructor
  ~ZeDMDFrameData() { delete[] data; }

  // Copy constructor (deep copy)
  ZeDMDFrameData(const ZeDMDFrameData& other)
      : size(other.size), data((other.size > 0) ? new uint8_t[other.size] : nullptr)
  {
    if (other.size > 0) memcpy(data, other.data, other.size);
  }

  // Copy assignment operator (deep copy)
  ZeDMDFrameData& operator=(const ZeDMDFrameData& other)
  {
    if (this != &other)
    {
      delete[] data;  // Clean up existing resource
      size = other.size;
      data = (other.size > 0) ? new uint8_t[other.size] : nullptr;
      if (other.size > 0) memcpy(data, other.data, other.size);
    }
    return *this;
  }

  // Move constructor
  ZeDMDFrameData(ZeDMDFrameData&& other) noexcept : size(other.size), data(other.data)
  {
    other.size = 0;
    other.data = nullptr;
  }

  // Move assignment operator
  ZeDMDFrameData& operator=(ZeDMDFrameData&& other) noexcept
  {
    if (this != &other)
    {
      delete[] data;  // Clean up existing resource

      size = other.size;
      data = other.data;

      other.size = 0;
      other.data = nullptr;
    }

    return *this;
  }
};

struct ZeDMDFrame
{
  uint8_t command;
  std::vector<ZeDMDFrameData> data;

  // Constructor with just the command
  ZeDMDFrame(uint8_t cmd) : command(cmd) {}

  // Constructor to add initial data
  ZeDMDFrame(uint8_t cmd, uint8_t* d, int s) : command(cmd)
  {
    data.emplace_back(d, s);  // Create and move a new ZeDMDFrameData object
  }

  // Destructor (no need to manually clear the vector)
  ~ZeDMDFrame() = default;

  // Copy constructor and assignment deleted
  ZeDMDFrame(const ZeDMDFrame&) = delete;
  ZeDMDFrame& operator=(const ZeDMDFrame&) = delete;

  // Move constructor
  ZeDMDFrame(ZeDMDFrame&& other) noexcept : command(other.command), data(std::move(other.data)) {}

  // Move assignment operator
  ZeDMDFrame& operator=(ZeDMDFrame&& other) noexcept
  {
    if (this != &other)
    {
      command = other.command;
      data = std::move(other.data);
    }
    return *this;
  }
};

typedef void(ZEDMDCALLBACK* ZeDMD_LogCallback)(const char* format, va_list args, const void* userData);

class ZeDMDComm
{
 public:
  static const int CTRL_CHARS_HEADER_SIZE = 5;
  static constexpr uint8_t CTRL_CHARS_HEADER[] = {'Z', 'e', 'D', 'M', 'D'};

 public:
  ZeDMDComm();
  ~ZeDMDComm();

  void SetLogCallback(ZeDMD_LogCallback callback, const void* userData);

  void IgnoreDevice(const char* ignore_device);
  void SetDevice(const char* device);

  virtual bool Connect();
  virtual void Disconnect();
  virtual bool IsConnected();

  void Run();
  void QueueFrame(uint8_t* buffer, int size);
  void QueueCommand(char command, uint8_t* buffer, int size);
  void QueueCommand(char command);
  void QueueCommand(char command, uint8_t value);
  bool FillDelayed();
  void SoftReset();

  uint16_t const GetWidth();
  uint16_t const GetHeight();
  bool const IsS3();

 protected:
  virtual bool StreamBytes(ZeDMDFrame* pFrame);
  virtual void Reset();
  void Log(const char* format, ...);

  uint16_t m_width = 128;
  uint16_t m_height = 32;
  bool m_s3 = false;
  bool m_cdc = false;
  uint8_t m_zoneWidth = 8;
  uint8_t m_zoneHeight = 4;
  std::atomic<bool> m_stopFlag;
  std::atomic<bool> m_fullFrameFlag;

 private:
  bool Connect(char* pName);
  bool Handshake(char* pDevice);
  bool SendChunks(uint8_t* pData, uint16_t size);

  ZeDMD_LogCallback m_logCallback = nullptr;
  const void* m_logUserData = nullptr;
  uint64_t m_zoneHashes[128] = {0};
  const uint8_t m_allBlack[32768] = {0};

  char m_ignoredDevices[10][32] = {0};
  uint8_t m_ignoredDevicesCounter = 0;
  uint8_t m_noAcknowledgeCounter = 0;
  char m_device[32] = {0};
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  struct sp_port* m_pSerialPort;
  struct sp_port_config* m_pSerialPortConfig;
#endif
  std::queue<ZeDMDFrame> m_frames;
  std::thread* m_pThread;
  std::mutex m_frameQueueMutex;
  ZeDMDFrame m_delayedFrame = {0};
  std::mutex m_delayedFrameMutex;
  bool m_delayedFrameReady = false;
};
