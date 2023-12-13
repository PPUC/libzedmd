#include "ZeDMDWiFi.h"
#include "miniz/miniz.h"
#include "komihash/komihash.h"

ZeDMDWiFi::ZeDMDWiFi()
{
   m_pThread = NULL;
}

ZeDMDWiFi::~ZeDMDWiFi()
{
   if (m_pThread)
   {
      m_pThread->join();

      delete m_pThread;
   }
}

void ZeDMDWiFi::SetLogMessageCallback(ZeDMD_LogMessageCallback callback, const void *userData)
{
   m_logMessageCallback = callback;
   m_logMessageUserData = userData;
}

void ZeDMDWiFi::LogMessage(const char *format, ...)
{
   if (!m_logMessageCallback)
      return;

   va_list args;
   va_start(args, format);
   (*(m_logMessageCallback))(format, args, m_logMessageUserData);
   va_end(args);
}

void ZeDMDWiFi::Run()
{
   m_pThread = new std::thread([this]()
                               {
                                  LogMessage("ZeDMDWiFi run thread starting");

                                  while (true)
                                  {
                                     m_frameQueueMutex.lock();

                                     if (m_frames.empty())
                                     {
                                        m_frameQueueMutex.unlock();
                                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                        continue;
                                     }

                                     ZeDMDWiFiFrame frame = m_frames.front();
                                     m_frames.pop();
                                     m_frameQueueMutex.unlock();

                                     bool success = StreamBytes(&frame);

                                     if (!success && frame.size < ZEDMD_WIFI_FRAME_SIZE_COMMAND_LIMIT)
                                     {
                                        // Try to send the command again, in case the the wait for the (R)eady signal ran into a timeout.
                                        success = StreamBytes(&frame);
                                     }

                                     if (frame.data)
                                     {
                                        free(frame.data);
                                     }

                                     if (!success)
                                     {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(8));
                                     }

                                     m_frameQueueMutex.lock();
                                     while (m_frames.size() >= ZEDMD_WIFI_FRAME_QUEUE_SIZE_MAX)
                                     {
                                        frame = m_frames.front();
                                        m_frames.pop();
                                        if (frame.size < ZEDMD_WIFI_FRAME_SIZE_COMMAND_LIMIT)
                                        {
                                           while (m_frames.size() > 0)
                                           {
                                              ZeDMDWiFiFrame tmpFrame = m_frames.front();
                                              m_frames.pop();

                                              if (tmpFrame.data)
                                                 free(tmpFrame.data);
                                           }
                                           m_frames.push(frame);
                                        }
                                        else if (frame.data)
                                        {
                                           free(frame.data);
                                        }
                                     }
                                     m_frameQueueMutex.unlock();
                                  }

                                  LogMessage("ZeDMDWiFi run thread finished"); });
}

void ZeDMDWiFi::QueueCommand(char command, uint8_t *data, int size, int width, uint8_t height)
{
   ZeDMDWiFiFrame frame = {0};
   frame.command = command;
   frame.size = size;
   frame.width = width;
   frame.height = height;

   if (data && size > 0)
   {
      frame.data = (uint8_t *)malloc(size);
      memcpy(frame.data, data, size);
   }

   m_frameQueueMutex.lock();
   m_frames.push(frame);
   m_frameQueueMutex.unlock();
}

void ZeDMDWiFi::QueueCommand(char command, uint8_t value)
{
   QueueCommand(command, &value, 1, 0, 0);
}

void ZeDMDWiFi::QueueCommand(char command)
{
   QueueCommand(command, NULL, 0, 0, 0);
}

bool ZeDMDWiFi::Connect(const char *ip, int port)
{
#if defined(_WIN32) || defined(_WIN64)
   WSADATA wsaData;
   if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR)
   {
      return false;
   }
#endif

   if ((m_wifiSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   {
      return false;
   }

   m_wifiServer.sin_family = AF_INET;
   m_wifiServer.sin_port = htons(port);
   m_wifiServer.sin_addr.s_addr = inet_addr(ip);

   return true;
}

void ZeDMDWiFi::Disconnect()
{
   Reset();

#if defined(_WIN32) || defined(_WIN64)
   WSACleanup();
#endif
}

void ZeDMDWiFi::Reset()
{
}

bool ZeDMDWiFi::StreamBytes(ZeDMDWiFiFrame *pFrame)
{
   // An UDP package should not exceed the MTU (WiFi rx_buffer in ESP32 is 1460 bytes).
   // We send obe zones of 16 * 8 pixels:
   // 128 pixels, 3 bytes RGB per pixel, 5 byte command header: 389 bytes
   // And we additionally use compression.

   if (pFrame->size < ZEDMD_WIFI_FRAME_SIZE_COMMAND_LIMIT)
   {
      uint8_t data[ZEDMD_WIFI_FRAME_SIZE_COMMAND_LIMIT + 4] = {0};
      data[0] = pFrame->command; // command
      data[1] = 0;               // not compressed
      data[2] = (uint8_t)(pFrame->size >> 8 & 0xFF);
      data[3] = (uint8_t)(pFrame->size & 0xFF);
      if (pFrame->size > 0)
      {
         memcpy(&data[4], pFrame->data, pFrame->size);
      }

      // Send command three times in case an UDP packet gets lost.
      for (int i = 0; i < 3; i++)
      {
#if defined(_WIN32) || defined(_WIN64)
         sendto(m_wifiSocket, (const char *)data, pFrame->size + 4, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#else
         sendto(m_wifiSocket, data, pFrame->size + 4, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#endif
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      return true;
   }

   uint8_t idx = 0;
   for (uint8_t y = 0; y < pFrame->height; y += 8)
   {
      for (uint8_t x = 0; x < pFrame->width; x += 16)
      {
         uint8_t data[16 * 8 * 3 + 4] = {0};
         data[0] = pFrame->command; // command

         uint8_t zone[16 * 8 * 3] = {0};
         for (uint8_t z = 0; z < 8; z++)
         {
            memcpy(&zone[z * 16 * 3], &pFrame->data[((y + z) * pFrame->width + x) * 3], 16 * 3);
         }

         uint64_t hash = komihash(zone, 16 * 8 * 3, 0);
         if (hash != m_zoneHashes[idx])
         {
            m_zoneHashes[idx] = hash;

            if (m_compression)
            {
               data[1] = (uint8_t)(128 | idx); // compressed + zone index

               mz_ulong compressedSize = 16 * 8 * 3;
               int status = mz_compress(&data[4], &compressedSize, zone, 16 * 8 * 3);
               data[2] = (uint8_t)(compressedSize >> 8 & 0xFF);
               data[3] = (uint8_t)(compressedSize & 0xFF);

               if (status == MZ_OK)
               {
#if defined(_WIN32) || defined(_WIN64)
                  sendto(m_wifiSocket, (const char *)data, compressedSize + 4, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#else
                  sendto(m_wifiSocket, data, compressedSize + 4, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#endif
               }
            }
            else
            {
               data[1] = (uint8_t)idx; // not compressed + zone index

               int size = 16 * 8 * 3;
               data[2] = (uint8_t)(size >> 8 & 0xFF);
               data[3] = (uint8_t)(size & 0xFF);
               memcpy(&data[4], zone, size);

#if defined(_WIN32) || defined(_WIN64)
               sendto(m_wifiSocket, (const char *)data, size + 4, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#else
               sendto(m_wifiSocket, data, size + 4, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#endif
            }
         }
         idx++;
      }
   }

   // std::this_thread::sleep_for(std::chrono::milliseconds(1));

   return true;
}

int ZeDMDWiFi::GetWidth()
{
   return m_width;
}

int ZeDMDWiFi::GetHeight()
{
   return m_height;
}