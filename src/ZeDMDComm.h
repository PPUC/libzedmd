#pragma once

#include <thread>
#include <queue>
#include <string>
#include <inttypes.h>
#include <mutex>
#include <stdio.h>
#include <stdarg.h>
#include "SerialPort.h"

#if defined(_WIN32) || defined(_WIN64)
#define CALLBACK __stdcall
#else
#define CALLBACK
#endif

typedef enum
{
   FrameSize = 0x02,
   Handshake = 0x0c,
   Chunk = 0x0d,
   Compression = 0x0e,
   EnableCompression = 0x0e,
   DisableCompression = 0x0f,
   LEDTest = 0x10,
   EnableUpscaling = 0x14,
   DisableUpscaling = 0x15,
   Brightness = 0x16,
   RGBOrder = 0x17,
   GetBrightness = 0x18,
   GetRGBOrder = 0x19,
   EnableFlowControlV2 = 0x1a,
   SaveSettings = 0x1e,
   Reset = 0x1f,
   GetVersionBytes = 0x20,
   GetResolution = 0x21,

   RGB24 = 0x03,
   Gray2 = 0x08,
   ColGray4 = 0x09,
   ColGray6 = 0x0b,
   ClearScreen = 0x0a,

   DisableDebug = 0x62,
   EnableDebug = 0x63,
} ZEDMD_COMM_COMMAND;

struct ZeDMDFrame
{
   uint8_t command;
   uint8_t *data;
   int size;
};

#define ZEDMD_COMM_BAUD_RATE 921600
#define ZEDMD_COMM_SERIAL_READ_TIMEOUT 8
#define ZEDMD_COMM_SERIAL_WRITE_TIMEOUT 8

#if defined(_WIN32) || defined(_WIN64)
#define ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE 8192
#elif defined(__APPLE__)
#define ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE 256
#elif defined(__ANDROID__)
#define ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE 2048
#else
// defined (__linux__)
#define ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE 256
#endif

#define ZEDMD_COMM_FRAME_SIZE_SLOW_THRESHOLD 4096
#define ZEDMD_COMM_FRAME_SIZE_COMMAND_LIMIT 10

#define ZEDMD_COMM_FRAME_QUEUE_SIZE_MAX 128
#define ZEDMD_COMM_FRAME_QUEUE_SIZE_SLOW 4

#ifndef __ANDROID__
#define ZEDMD_COMM_FRAME_QUEUE_SIZE_DEFAULT 32
#else
#define ZEDMD_COMM_FRAME_QUEUE_SIZE_DEFAULT 8
#endif

#ifdef __ANDROID__
typedef void* (*ZeDMD_AndroidGetJNIEnvFunc)();
#endif

typedef void (CALLBACK *ZeDMD_LogMessageCallback)(const char* format, va_list args, const void* userData);

class ZeDMDComm
{
public:
   static const int CTRL_CHARS_HEADER_SIZE = 6;
   static constexpr uint8_t CTRL_CHARS_HEADER[] = {0x5a, 0x65, 0x64, 0x72, 0x75, 0x6d};

public:
   ZeDMDComm();
   ~ZeDMDComm();

#ifdef __ANDROID__
   void SetAndroidGetJNIEnvFunc(ZeDMD_AndroidGetJNIEnvFunc func);
#endif

   void SetLogMessageCallback(ZeDMD_LogMessageCallback callback, const void* userData);

   void IgnoreDevice(const char *ignore_device);

   bool Connect();
   void Disconnect();

   void Run();
   void QueueCommand(char command, uint8_t *buffer, int size);
   void QueueCommand(char command);
   void QueueCommand(char command, uint8_t value);

   int GetWidth();
   int GetHeight();

private:
   void LogMessage(const char* format, ...);

   bool Connect(char *pName);
   void Reset();
   bool StreamBytes(ZeDMDFrame *pFrame);

   ZeDMD_LogMessageCallback m_logMessageCallback = nullptr;
   const void* m_logMessageUserData = nullptr;

   int m_width = 128;
   int m_height = 32;
   uint8_t m_flowControlCounter = 0;
   char m_ignoredDevices[10][32] = {0};
   uint8_t m_ignoredDevicesCounter = 0;
   SerialPort m_serialPort;
   std::queue<ZeDMDFrame> m_frames;
   std::thread *m_pThread;
   std::mutex m_frameQueueMutex;
};