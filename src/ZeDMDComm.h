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

// The maximum baud rate supported by CP210x. CH340 and others might be able to run higher baudrates, but on the ESP32
// we don't know which USB-to-serial converter is in use.
#define ZEDMD_COMM_BAUD_RATE 921600
#define ZEDMD_COMM_MIN_SERIAL_WRITE_AT_ONCE 32
#define ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE 1920
#define ZEDMD_COMM_DEFAULT_SERIAL_WRITE_AT_ONCE 64

#define ZEDMD_COMM_SERIAL_READ_TIMEOUT 16
#define ZEDMD_COMM_SERIAL_WRITE_TIMEOUT 8

#define ZEDMD_COMM_KEEP_ALIVE_INTERVAL 3000

#define ZEDMD_COMM_FRAME_QUEUE_SIZE_MAX 8

#define ZEDMD_ZONES_BYTE_LIMIT (128 * 4 * 2 + 16)
#define ZEDMD_S3_ZONES_BYTE_LIMIT (ZEDMD_ZONES_BYTE_LIMIT)

typedef enum
{
  FrameSize = 0x02,
  Handshake = 0x0c,
  LEDTest = 0x10,
  EnableUpscaling = 0x15,
  DisableUpscaling = 0x14,
  Brightness = 0x16,
  RGBOrder = 0x17,
  SetWiFiSSID = 0x1b,
  SetWiFiPassword = 0x1c,
  SetWiFiPort = 0x1d,
  SaveSettings = 0x1e,
  Reset = 0x1f,

  SetClkphase = 0x28,
  SetI2sspeed = 0x29,
  SetLatchBlanking = 0x2a,
  SetMinRefreshRate = 0x2b,
  SetDriver = 0x2c,
  SetTransport = 0x2d,
  SetUdpDelay = 0x2e,
  SetUsbPackageSizeMultiplier = 0x2f,
  SetYOffset = 0x30,

  RGB565ZonesStream = 0x05,
  RenderRGB565Frame = 0x06,

  ClearScreen = 0x0a,

  KeepAlive = 0x0b,  // 16

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
    if (s > 0)
    {
      data.emplace_back(d, s);  // Create and move a new ZeDMDFrameData object
    }
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
  static const int FRAME_HEADER_SIZE = 5;
  static constexpr uint8_t FRAME_HEADER[] = {'F', 'R', 'A', 'M', 'E'};

 public:
  ZeDMDComm();
  ~ZeDMDComm();

  void SetLogCallback(ZeDMD_LogCallback callback, const void* userData);

  void IgnoreDevice(const char* ignore_device);
  void SetDevice(const char* device);

  virtual bool Connect();
  virtual void Disconnect();
  virtual bool IsConnected();
  virtual uint8_t GetTransport();
  virtual const char* GetWiFiSSID();
  virtual void StoreWiFiPassword();
  virtual int GetWiFiPort();

  void Run();
  void QueueFrame(uint8_t* buffer, int size);
  void QueueCommand(char command, uint8_t* buffer, int size);
  void QueueCommand(char command);
  void QueueCommand(char command, uint8_t value);
  bool FillDelayed();
  void SoftReset();
  void EnableKeepAlive() { m_keepAlive = true; }
  void DisableKeepAlive() { m_keepAlive = false; }

  uint16_t const GetWidth();
  uint16_t const GetHeight();
  bool const IsS3();
  bool const IsHalf();
  const char* GetFirmwareVersion() { return (const char*)m_firmwareVersion; }
  const char* GetDevice() { return (const char*)m_device; }
  uint16_t GetId() { return m_id; }
  uint8_t GetBrightness() { return m_brightness; }
  uint8_t GetRGBOrder() { return m_rgbMode; }
  uint8_t GetYOffset() { return m_yOffset; }
  uint8_t GetPanelClockPhase() { return m_panelClkphase; }
  uint8_t GetPanelDriver() { return m_panelDriver; }
  uint8_t GetPanelI2sSpeed() { return m_panelI2sspeed; }
  uint8_t GetPanelLatchBlanking() { return m_panelLatchBlanking; }
  uint8_t GetPanelMinRefreshRate() { return m_panelMinRefreshRate; }
  uint8_t GetUdpDelay() { return m_udpDelay; }
  uint16_t GetUsbPackageSize() { return m_writeAtOnce; }

 protected:
  virtual bool SendChunks(uint8_t* pData, uint16_t size);
  virtual void Reset();
  void Log(const char* format, ...);
  void ClearFrames();
  bool IsQueueEmpty();

  char m_firmwareVersion[12] = "0.0.0";
  uint16_t m_id = 0;
  uint16_t m_width = 128;
  uint16_t m_height = 32;
  bool m_s3 = false;
  bool m_cdc = false;
  bool m_half = false;
  uint8_t m_zoneWidth = 8;
  uint8_t m_zoneHeight = 4;
  std::atomic<bool> m_stopFlag;
  std::atomic<bool> m_fullFrameFlag;
  std::chrono::milliseconds m_keepAliveInterval;

  uint8_t m_brightness = 2;
  uint8_t m_rgbMode = 0;
  uint8_t m_yOffset = 0;
  uint8_t m_panelClkphase = 0;
  uint8_t m_panelDriver = 0;
  uint8_t m_panelI2sspeed = 8;
  uint8_t m_panelLatchBlanking = 2;
  uint8_t m_panelMinRefreshRate = 30;
  uint8_t m_udpDelay = 5;
  uint16_t m_writeAtOnce = ZEDMD_COMM_DEFAULT_SERIAL_WRITE_AT_ONCE;

 private:
  bool Connect(char* pName);
  bool Handshake(char* pDevice);
  bool StreamBytes(ZeDMDFrame* pFrame);
  void KeepAlive();

  ZeDMD_LogCallback m_logCallback = nullptr;
  const void* m_logUserData = nullptr;
  uint64_t m_zoneHashes[128] = {0};
  const uint8_t m_allBlack[32768] = {0};

  char m_ignoredDevices[10][32] = {0};
  uint8_t m_ignoredDevicesCounter = 0;
  char m_device[32] = {0};
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  struct sp_port* m_pSerialPort;
#endif
  std::queue<ZeDMDFrame> m_frames;
  std::thread* m_pThread;
  std::mutex m_frameQueueMutex;
  ZeDMDFrame m_delayedFrame = {0};
  std::mutex m_delayedFrameMutex;
  bool m_delayedFrameReady = false;
  bool m_keepAlive = true;
  std::chrono::steady_clock::time_point m_lastKeepAlive;
  bool m_autoDetect = true;
};
