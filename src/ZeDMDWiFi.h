#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#define CALLBACK __stdcall
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define CALLBACK
#endif
#include <thread>
#include <queue>
#include <string>
#include <inttypes.h>
#include <mutex>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>

typedef enum
{
   UDP_RGB24 = 0x04,
   UDP_ClearScreen = 0x0a,
} ZEDMD_WIFI_COMMAND;

struct ZeDMDWiFiFrame
{
   uint8_t command;
   uint8_t *data;
   int size;
   int width;
   uint8_t height;
};

#define ZEDMD_WIFI_FRAME_SIZE_COMMAND_LIMIT 10

typedef void(CALLBACK *ZeDMD_LogMessageCallback)(const char *format, va_list args, const void *userData);

class ZeDMDWiFi
{
public:
   ZeDMDWiFi();
   ~ZeDMDWiFi();

   void SetLogMessageCallback(ZeDMD_LogMessageCallback callback, const void *userData);

   bool Connect(const char *ip, int port);
   void Disconnect();

   void Run();
   void QueueCommand(char command, uint8_t *buffer, int size, uint16_t width, uint16_t height);
   void QueueCommand(char command);
   void QueueCommand(char command, uint8_t value);

   uint16_t GetWidth();
   uint16_t GetHeight();

private:
   void LogMessage(const char *format, ...);

   void Reset();
   void StreamBytes(ZeDMDWiFiFrame *pFrame);

   ZeDMD_LogMessageCallback m_logMessageCallback = nullptr;
   const void *m_logMessageUserData = nullptr;

   uint16_t m_width = 128;
   uint16_t m_height = 32;

   bool m_compression = true;
   uint64_t m_zoneHashes[128] = {0};

   int m_wifiSocket;
   struct sockaddr_in m_wifiServer;

   std::queue<ZeDMDWiFiFrame> m_frames;
   std::thread *m_pThread;
   std::mutex m_frameQueueMutex;
};