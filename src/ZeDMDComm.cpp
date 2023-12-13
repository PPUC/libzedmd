#include "ZeDMDComm.h"
#include "miniz/miniz.h"
#include "komihash/komihash.h"

ZeDMDComm::ZeDMDComm()
{
   m_pThread = NULL;
}

ZeDMDComm::~ZeDMDComm()
{
   if (m_pThread)
   {
      m_pThread->join();

      delete m_pThread;
   }
}

#ifdef __ANDROID__
void ZeDMDComm::SetAndroidGetJNIEnvFunc(ZeDMD_AndroidGetJNIEnvFunc func)
{
   m_serialPort.SetAndroidGetJNIEnvFunc(func);
}
#endif

void ZeDMDComm::SetLogMessageCallback(ZeDMD_LogMessageCallback callback, const void *userData)
{
   m_logMessageCallback = callback;
   m_logMessageUserData = userData;
}

void ZeDMDComm::LogMessage(const char *format, ...)
{
   if (!m_logMessageCallback)
      return;

   va_list args;
   va_start(args, format);
   (*(m_logMessageCallback))(format, args, m_logMessageUserData);
   va_end(args);
}

void ZeDMDComm::Run()
{
   m_pThread = new std::thread([this]()
                               {
                                  LogMessage("ZeDMDComm run thread starting");

                                  bool sleep = false;
                                  int maxQueuedFrames;
                                  if (m_width == 256)
                                  {
                                      maxQueuedFrames = ZEDMD_COMM_RGB24_QUEUE_SIZE_MAX;
                                  }
                                  else
                                  {
                                      maxQueuedFrames = ZEDMD_COMM_FRAME_QUEUE_SIZE_MAX;
                                  }

                                  while (m_serialPort.IsOpen())
                                  {
                                     m_frameQueueMutex.lock();

                                     if (m_frames.empty())
                                     {
                                        m_frameQueueMutex.unlock();
                                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                        continue;
                                     }

                                     ZeDMDFrame frame = m_frames.front();
                                     m_frames.pop();
                                     m_frameQueueMutex.unlock();
                                     if (frame.size > ZEDMD_COMM_FRAME_SIZE_SLOW_THRESHOLD)
                                     {
                                        maxQueuedFrames = ZEDMD_COMM_FRAME_QUEUE_SIZE_SLOW;
                                     }
                                     else
                                     {
                                         if (m_width == 256)
                                         {
                                             maxQueuedFrames = ZEDMD_COMM_RGB24_QUEUE_SIZE_MAX;
                                         }
                                         else
                                         {
                                             maxQueuedFrames = ZEDMD_COMM_FRAME_QUEUE_SIZE_MAX;
                                         }
                                     }

                                     bool success = StreamBytes(&frame);
                                     if (!success && frame.size < ZEDMD_COMM_FRAME_SIZE_COMMAND_LIMIT)
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
                                     while (m_frames.size() >= maxQueuedFrames)
                                     {
                                        frame = m_frames.front();
                                        m_frames.pop();
                                        if (frame.size < ZEDMD_COMM_FRAME_SIZE_COMMAND_LIMIT)
                                        {
                                           while (m_frames.size() > 0)
                                           {
                                              ZeDMDFrame tmpFrame = m_frames.front();
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

                                  LogMessage("ZeDMDComm run thread finished"); });
}

void ZeDMDComm::QueueCommand(char command, uint8_t *data, int size)
{
   if (!m_serialPort.IsOpen())
   {
      return;
   }

   ZeDMDFrame frame = {0};
   frame.command = command;
   frame.size = size;

   if (data && size > 0)
   {
      frame.data = (uint8_t *)malloc(size);
      memcpy(frame.data, data, size);
   }

   m_frameQueueMutex.lock();
   m_frames.push(frame);
   m_frameQueueMutex.unlock();
}

void ZeDMDComm::QueueCommand(char command, uint8_t value)
{
   QueueCommand(command, &value, 1);
}

void ZeDMDComm::QueueCommand(char command)
{
   QueueCommand(command, NULL, 0);
}

void ZeDMDComm::QueueCommand(char command, uint8_t *data, int size, int width, uint8_t height)
{
   uint8_t idx = 0;
   for (uint8_t y = 0; y < height; y += 8)
   {
      for (uint8_t x = 0; x < width; x += 16)
      {
         uint8_t zone[16 * 8 * 3 + 1] = {0};
         zone[0] = idx;

         for (uint8_t z = 0; z < 8; z++)
         {
            memcpy(&zone[z * 16 * 3 + 1], &data[((y + z) * width + x) * 3], 16 * 3);
         }

         uint64_t hash = komihash(&zone[1], 16 * 8 * 3, 0);
         if (hash != m_zoneHashes[idx])
         {
            m_zoneHashes[idx] = hash;

            QueueCommand(command, zone, 16 * 8 * 3 + 1);
         }
         idx++;
      }
   }
}

void ZeDMDComm::IgnoreDevice(const char *ignore_device)
{
   if (sizeof(ignore_device) < 32 && m_ignoredDevicesCounter < 10)
   {
      strcpy(&m_ignoredDevices[m_ignoredDevicesCounter++][0], ignore_device);
   }
}

bool ZeDMDComm::Connect()
{
#ifndef __ANDROID__
   char szDevice[32];

   for (int i = 0; i < 7; i++)
   {
#ifdef __APPLE__
      sprintf(szDevice, "/dev/cu.usbserial-%04d", i);
#elif defined(_WIN32) || defined(_WIN64)
      sprintf(szDevice, "\\\\.\\COM%d", i + 1);
#else
      sprintf(szDevice, "/dev/ttyUSB%d", i);
#endif

      for (int j = 0; j < m_ignoredDevicesCounter; j++)
      {
         if (strcmp(szDevice, m_ignoredDevices[j]) == 0)
         {
            continue;
         }
      }

      if (Connect(szDevice))
         return true;
   }

   return false;
#else
   return Connect(NULL);
#endif
}

void ZeDMDComm::Disconnect()
{
   if (!m_serialPort.IsOpen())
      return;

   Reset();

   m_serialPort.Close();
}

bool ZeDMDComm::Connect(char *pDevice)
{
   m_serialPort.SetReadTimeout(ZEDMD_COMM_SERIAL_READ_TIMEOUT);
   m_serialPort.SetWriteTimeout(ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);

   if (m_serialPort.Open(pDevice, ZEDMD_COMM_BAUD_RATE, 8, 1, 0) != 1)
      return false;

   Reset();

   uint8_t data[8] = {0};

   // Android in general but also ZeDMD HD require some time after opening the device.
   std::this_thread::sleep_for(std::chrono::milliseconds(1000));

   // ESP32 sends some information about itself before we could use the line.
   while (m_serialPort.Available() > 0)
      m_serialPort.ReadBytes(data, 8);

#ifdef __ANDROID__
   // Android returns a large buffer of 80 and 00, so wait and read again to clear
   std::this_thread::sleep_for(std::chrono::milliseconds(1000));

   while (m_serialPort.Available() > 0)
      m_serialPort.ReadBytes(data, 8);
#endif

   m_serialPort.WriteBytes((uint8_t *)CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
   m_serialPort.WriteChar(ZEDMD_COMM_COMMAND::Handshake);
   std::this_thread::sleep_for(std::chrono::milliseconds(200));

   if (m_serialPort.ReadBytes(data, 8))
   {
      if (!memcmp(data, CTRL_CHARS_HEADER, 4))
      {
         m_width = data[4] + data[5] * 256;
         m_height = data[6] + data[7] * 256;

         uint8_t response;

         if (m_serialPort.ReadByte() == 'R')
         {
            m_serialPort.WriteBytes((uint8_t *)CTRL_CHARS_HEADER, 6);
            m_serialPort.WriteChar(ZEDMD_COMM_COMMAND::Compression);
            std::this_thread::sleep_for(std::chrono::milliseconds(4));

            if (m_serialPort.ReadByte() == 'A' && m_serialPort.ReadByte() == 'R')
            {
               m_serialPort.WriteBytes((uint8_t *)CTRL_CHARS_HEADER, 6);
               m_serialPort.WriteChar(ZEDMD_COMM_COMMAND::Chunk);
               m_serialPort.WriteChar(ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE / 256);
               std::this_thread::sleep_for(std::chrono::milliseconds(4));

               if (m_serialPort.ReadByte() == 'A' && m_serialPort.ReadByte() == 'R')
               {
                  m_serialPort.WriteBytes((uint8_t *)CTRL_CHARS_HEADER, 6);
                  m_serialPort.WriteChar(ZEDMD_COMM_COMMAND::EnableFlowControlV2);
                  std::this_thread::sleep_for(std::chrono::milliseconds(4));

                  if (m_serialPort.ReadByte() == 'A')
                  {
                     m_flowControlCounter = 1;

                     if (pDevice)
                     {
                        LogMessage("ZeDMD found: device=%s, width=%d, height=%d", pDevice, m_width, m_height);
                     }
                     else
                     {
                        LogMessage("ZeDMD found: width=%d, height=%d", m_width, m_height);
                     }

                     return true;
                  }
               }
            }
         }
      }
   }

   Disconnect();

   return false;
}

void ZeDMDComm::Reset()
{
   m_serialPort.ClearDTR();
   m_serialPort.SetRTS();
   std::this_thread::sleep_for(std::chrono::milliseconds(200));

   m_serialPort.ClearRTS();
   m_serialPort.ClearDTR();
   std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

bool ZeDMDComm::StreamBytes(ZeDMDFrame *pFrame)
{
   uint8_t *data;
   int size;

   if (pFrame->size == 0)
   {
      size = CTRL_CHARS_HEADER_SIZE + 1;
      data = (uint8_t *)malloc(size);
      memcpy(data, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
      data[CTRL_CHARS_HEADER_SIZE] = pFrame->command;
   }
   else
   {
      mz_ulong compressedSize = mz_compressBound(pFrame->size);
      data = (uint8_t *)malloc(CTRL_CHARS_HEADER_SIZE + 3 + compressedSize);
      memcpy(data, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
      data[CTRL_CHARS_HEADER_SIZE] = pFrame->command;
      mz_compress(data + CTRL_CHARS_HEADER_SIZE + 3, &compressedSize, pFrame->data, pFrame->size);
      size = CTRL_CHARS_HEADER_SIZE + 3 + compressedSize;
      data[CTRL_CHARS_HEADER_SIZE + 1] = (uint8_t)(compressedSize >> 8 & 0xFF);
      data[CTRL_CHARS_HEADER_SIZE + 2] = (uint8_t)(compressedSize & 0xFF);
   }

   bool success = false;

   uint8_t flowControlCounter;
   do
   {
      // In case of a timeout, ReadByte() returns 0.
      flowControlCounter = m_serialPort.ReadByte();
   } while (flowControlCounter != 0 && flowControlCounter != m_flowControlCounter);

   if (flowControlCounter == m_flowControlCounter)
   {
      int position = 0;
      success = true;

      while (position < size && success)
      {
         m_serialPort.WriteBytes(data + position, ((size - position) < ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE) ? (size - position) : ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE);

         uint8_t response;
         do
         {
            response = m_serialPort.ReadByte();
         } while (response == flowControlCounter);

         if (response == 'A')
            position += ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE;
         else
         {
            success = false;
            LogMessage("Write bytes failure: response=%c", response);
         }
      }

      if (m_flowControlCounter < 32)
      {
         m_flowControlCounter++;
      }
      else
      {
         m_flowControlCounter = 1;
      }
   }
   else
   {
      LogMessage("No Ready Signal");
   }

   free(data);

   return success;
}

int ZeDMDComm::GetWidth()
{
   return m_width;
}

int ZeDMDComm::GetHeight()
{
   return m_height;
}