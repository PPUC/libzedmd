#include "ZeDMDWiFi.h"
#include "miniz/miniz.h"

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

                                  bool sleep = false;
                                  int maxQueuedFrames = ZEDMD_WIFI_FRAME_QUEUE_SIZE_MAX;

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
                                     if (frame.size < ZEDMD_WIFI_FRAME_SIZE_COMMAND_LIMIT)
                                     {
                                        sleep = false;
                                        maxQueuedFrames = ZEDMD_WIFI_FRAME_QUEUE_SIZE_MAX;
                                     }
                                     else if (frame.size > ZEDMD_WIFI_FRAME_SIZE_SLOW_THRESHOLD)
                                     {
                                        // For 128x32 RGB24 we just limit the amount of queued frames.
                                        // For 256x64 Content we must also slow down the frequency.
                                        if (m_width == 256)
                                        {
                                           sleep = false;
                                           maxQueuedFrames = ZEDMD_WIFI_FRAME_QUEUE_SIZE_MAX;
                                        }
                                        else
                                        {
                                           sleep = false;
                                           maxQueuedFrames = ZEDMD_WIFI_FRAME_QUEUE_SIZE_SLOW;
                                        }
                                     }
                                     else
                                     {
                                        maxQueuedFrames = ZEDMD_WIFI_FRAME_QUEUE_SIZE_DEFAULT;
                                     }

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

                                     sleep = !success || sleep;
                                     if (sleep)
                                     {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(8));
                                     }

                                     m_frameQueueMutex.lock();
                                     while (m_frames.size() >= maxQueuedFrames)
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
}

void ZeDMDWiFi::Reset()
{
}

bool ZeDMDWiFi::StreamBytes(ZeDMDWiFiFrame *pFrame)
{
   // An UDP package should not exceed the MTU (WiFi rx_buffer in ESP32 is 1460 bytes), a single HD in the row should be less:
   // 256 pixels, 3 bytes RGB per pixel, 3 byte command header: 773 bytes
   // And we additionally use compression.
   uint8_t *data = (uint8_t *)malloc(pFrame->size + 3);

   data[0] = pFrame->command; // command
   // data[1] = 128 | m_frameCounter; // compressed + frameCounter
   data[1] = m_frameCounter; // compressed + frameCounter

   for (uint8_t y = 0; y < pFrame->height; y++)
   {
      if (pFrame->command != m_previousFrame.command || pFrame->width != m_previousFrame.width || pFrame->height != m_previousFrame.height || 0 != memcmp(pFrame->data + (y * pFrame->width * 3), &(m_previousFrame.data[y * pFrame->width]), pFrame->width * 3))
      {
         data[2] = y;

         memcpy(&data[3], pFrame->data + (y * pFrame->width * 3), pFrame->width * 3);
#if defined(_WIN32) || defined(_WIN64)
         sendto(m_wifiSocket, (const char *)data, pFrame->width * 3 + 3, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#else
         sendto(m_wifiSocket, data, pFrame->width * 3 + 3, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#endif
         // mz_ulong compressedSize = width * 3;
         // int status = mz_compress(&data[3], &compressedSize, pFrame->data + (y * width * 3), width * 3);

         // if (status == MZ_OK) {
         // sendto(m_wifiSocket, data, compressedSize + 3, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
         //}
      }
   }

   data[1] = 64 + (m_frameCounter++);
   if (m_frameCounter >= 64)
   {
      m_frameCounter = 0;
   }

   for (int i = 0; i < 3; i++)
   {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
#if defined(_WIN32) || defined(_WIN64)
      sendto(m_wifiSocket, (const char *)data, 2, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#else
      sendto(m_wifiSocket, data, 2, 0, (struct sockaddr *)&m_wifiServer, sizeof(m_wifiServer));
#endif
   }

   m_previousFrame.command = pFrame->command;
   m_previousFrame.size = pFrame->size;
   m_previousFrame.width = pFrame->width;
   m_previousFrame.height = pFrame->height;
   free(m_previousFrame.data);
   m_previousFrame.data = (uint8_t *)malloc(pFrame->size);
   memcpy(m_previousFrame.data, pFrame->data, pFrame->size);

   // std::this_thread::sleep_for(std::chrono::milliseconds(4));

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