#include "ZeDMDComm.h"

#include "komihash/komihash.h"
#include "miniz/miniz.h"

ZeDMDComm::ZeDMDComm()
{
  m_pThread = nullptr;
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  m_pSerialPort = nullptr;
  m_pSerialPortConfig = nullptr;
#endif
}

ZeDMDComm::~ZeDMDComm()
{
  Disconnect();

  if (m_pThread)
  {
    m_pThread->join();

    delete m_pThread;
  }
}

void ZeDMDComm::SetLogCallback(ZeDMD_LogCallback callback, const void* userData)
{
  m_logCallback = callback;
  m_logUserData = userData;
}

void ZeDMDComm::Log(const char* format, ...)
{
  if (!m_logCallback)
  {
    return;
  }

  va_list args;
  va_start(args, format);
  (*(m_logCallback))(format, args, m_logUserData);
  va_end(args);
}

void ZeDMDComm::Run()
{
  m_pThread = new std::thread(
      [this]()
      {
        Log("ZeDMDComm run thread starting");
        int8_t lastStreamId = -1;

        while (IsConnected())
        {
          bool frame_completed = false;

          m_frameQueueMutex.lock();

          if (m_frames.empty())
          {
            m_delayedFrameMutex.lock();
            if (m_delayedFrameReady)
            {
              while (!m_delayedFrames.empty())
              {
                lastStreamId = m_delayedFrames.front().streamId;
                m_frames.push(std::move(m_delayedFrames.front()));
                m_delayedFrames.pop();
              }
              m_delayedFrameReady = false;
              m_frameCounter = 1;
              m_delayedFrameMutex.unlock();
              m_frameQueueMutex.unlock();
              continue;
            }
            m_delayedFrameMutex.unlock();
            m_frameQueueMutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
          }
          else
          {
            bool delay = false;
            m_delayedFrameMutex.lock();
            delay = m_delayedFrameReady;
            m_delayedFrameMutex.unlock();

            if (delay && m_frameCounter > ZEDMD_COMM_FRAME_QUEUE_SIZE_MAX_DELAYED)
            {
              while (!m_frames.empty())
              {
                m_frames.pop();
              }
              m_frameCounter = 0;
              m_frameQueueMutex.unlock();
              continue;
            }
          }

          if (m_frames.front().streamId != -1)
          {
            if (m_frames.front().streamId != lastStreamId)
            {
              if (lastStreamId != -1)
              {
                m_frameCounter--;
                frame_completed = true;
              }

              lastStreamId = m_frames.front().streamId;
            }
          }
          else
          {
            m_frameCounter--;
            frame_completed = true;
          }

          m_frameQueueMutex.unlock();

          bool success = StreamBytes(&(m_frames.front()));
          if (!success && m_frames.front().size < ZEDMD_COMM_FRAME_SIZE_COMMAND_LIMIT)
          {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            // Try to send the command again, in case the wait for the (R)eady
            // signal ran into a timeout.
            success = StreamBytes(&(m_frames.front()));
          }

          m_frames.pop();

          if (!success)
          {
            // Allow ZeDMD to empty its buffers.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
          }
        }

        Log("ZeDMDComm run thread finished");
      });
}

void ZeDMDComm::QueueCommand(char command, uint8_t* data, int size, int8_t streamId, bool delayed)
{
  if (!IsConnected())
  {
    return;
  }

  ZeDMDFrame frame = {0};
  frame.command = command;
  frame.size = size;
  frame.streamId = streamId;

  if (data && size > 0)
  {
    frame.data = (uint8_t*)malloc(size * sizeof(uint8_t));
    memcpy(frame.data, data, size);
  }

  // delayed standard frame
  if (streamId == -1 && FillDelayed())
  {
    m_delayedFrameMutex.lock();
    while (!m_delayedFrames.empty())
    {
      m_delayedFrames.pop();
    }

    m_delayedFrames.push(std::move(frame));
    m_delayedFrameReady = true;
    m_delayedFrameMutex.unlock();
    m_lastStreamId = -1;
    // Next streaming needs to be complete.
    memset(m_zoneHashes, 0, sizeof(m_zoneHashes));
  }
  // delayed streamed zones
  else if (streamId != -1 && delayed)
  {
    m_delayedFrameMutex.lock();
    m_delayedFrames.push(std::move(frame));
    m_delayedFrameMutex.unlock();
    m_lastStreamId = streamId;
  }
  else
  {
    m_frameQueueMutex.lock();
    if (streamId == -1 || m_lastStreamId != streamId)
    {
      m_frameCounter++;
      m_lastStreamId = streamId;
    }
    m_frames.push(std::move(frame));
    m_frameQueueMutex.unlock();
    if (streamId == -1)
    {
      // Next streaming needs to be complete.
      memset(m_zoneHashes, 0, sizeof(m_zoneHashes));
    }
  }
}

void ZeDMDComm::QueueCommand(char command, uint8_t value) { QueueCommand(command, &value, 1); }

void ZeDMDComm::QueueCommand(char command) { QueueCommand(command, nullptr, 0); }

void ZeDMDComm::QueueCommand(char command, uint8_t* data, int size, uint16_t width, uint16_t height, uint8_t bytes)
{
  uint8_t* buffer = (uint8_t*)malloc(256 * 16 * bytes + 16);
  uint16_t bufferSize = 0;
  uint8_t idx = 0;
  uint8_t* zone = (uint8_t*)malloc(16 * 8 * bytes);
  uint16_t zonesBytesLimit = 0;
  if (m_zonesBytesLimit)
  {
    while (zonesBytesLimit < m_zonesBytesLimit)
    {
      zonesBytesLimit += m_zoneWidth * m_zoneHeight * bytes + 1;
    }
  }
  else
  {
    zonesBytesLimit = width * m_zoneHeight * bytes + 16;
  }

  if (++m_streamId > 64)
  {
    m_streamId = 0;
  }

  bool delayed = false;
  if (FillDelayed())
  {
    // printf("DELAYED\n");
    delayed = true;
    m_delayedFrameMutex.lock();
    m_delayedFrameReady = false;
    while (!m_delayedFrames.empty())
    {
      m_delayedFrames.pop();
    }

    m_delayedFrameMutex.unlock();
    // A delayed frame needs to be complete.
    memset(m_zoneHashes, 0, sizeof(m_zoneHashes));
  }

  for (uint16_t y = 0; y < height; y += m_zoneHeight)
  {
    for (uint16_t x = 0; x < width; x += m_zoneWidth)
    {
      for (uint8_t z = 0; z < m_zoneHeight; z++)
      {
        memcpy(&zone[z * m_zoneWidth * bytes], &data[((y + z) * width + x) * bytes], m_zoneWidth * bytes);
      }

      uint64_t hash = komihash(zone, m_zoneWidth * m_zoneHeight * bytes, 0);
      if (hash != m_zoneHashes[idx])
      {
        m_zoneHashes[idx] = hash;

        buffer[bufferSize++] = idx;
        memcpy(&buffer[bufferSize], zone, m_zoneWidth * m_zoneHeight * bytes);
        bufferSize += m_zoneWidth * m_zoneHeight * bytes;

        if (bufferSize >= zonesBytesLimit)
        {
          QueueCommand(command, buffer, bufferSize, m_streamId, delayed);
          bufferSize = 0;
        }
      }
      idx++;
    }
  }

  if (bufferSize > 0)
  {
    QueueCommand(command, buffer, bufferSize, m_streamId, delayed);
  }

  if (delayed)
  {
    m_delayedFrameMutex.lock();
    m_delayedFrameReady = true;
    m_delayedFrameMutex.unlock();
  }

  free(buffer);
  free(zone);
}

bool ZeDMDComm::FillDelayed()
{
  uint8_t count = 0;
  bool delayed = false;
  m_frameQueueMutex.lock();
  count = m_frameCounter;
  delayed = m_delayedFrameReady;
  m_frameQueueMutex.unlock();
  return (count > ZEDMD_COMM_FRAME_QUEUE_SIZE_MAX) || delayed;
}

void ZeDMDComm::IgnoreDevice(const char* ignore_device)
{
  if (sizeof(ignore_device) < 32 && m_ignoredDevicesCounter < 10)
  {
    strcpy(&m_ignoredDevices[m_ignoredDevicesCounter++][0], ignore_device);
  }
}

void ZeDMDComm::SetDevice(const char* device)
{
  if (sizeof(device) < 32)
  {
    strcpy(m_device, device);
  }
}

bool ZeDMDComm::Connect()
{
  bool success = false;

#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  if (*m_device != 0)
  {
    Log("Connecting to ZeDMD on %s...", m_device);

    success = Connect(m_device);

    if (!success)
    {
      Log("Unable to connect to ZeDMD on %s", m_device);
    }
  }
  else
  {
    Log("Searching for ZeDMD...");

    struct sp_port** ppPorts;
    enum sp_return result = sp_list_ports(&ppPorts);
    if (result == SP_OK)
    {
      for (int i = 0; ppPorts[i]; i++)
      {
        char* pDevice = sp_get_port_name(ppPorts[i]);
        bool ignored = false;
        for (int j = 0; j < m_ignoredDevicesCounter; j++)
        {
          ignored = (strcmp(pDevice, m_ignoredDevices[j]) == 0);
          if (ignored)
          {
            break;
          }
        }
        if (!ignored)
        {
          success = Connect(pDevice);
        }
        if (success)
        {
          break;
        }
      }
      sp_free_port_list(ppPorts);
    }

    if (!success)
    {
      Log("Unable to find ZeDMD");
    }
  }
#endif

  return success;
}

void ZeDMDComm::Disconnect()
{
  if (!IsConnected())
  {
    return;
  }

  Reset();
  // Wait a bit to let the reset command be transmitted.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  sp_set_config(m_pSerialPort, m_pSerialPortConfig);
  sp_free_config(m_pSerialPortConfig);
  m_pSerialPortConfig = nullptr;

  sp_close(m_pSerialPort);
  sp_free_port(m_pSerialPort);
  m_pSerialPort = nullptr;
#endif
}

bool ZeDMDComm::Connect(char* pDevice)
{
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  enum sp_return result = sp_get_port_by_name(pDevice, &m_pSerialPort);
  if (result != SP_OK)
  {
    return false;
  }

  result = sp_open(m_pSerialPort, SP_MODE_READ_WRITE);
  if (result != SP_OK)
  {
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }

  sp_new_config(&m_pSerialPortConfig);
  sp_get_config(m_pSerialPort, m_pSerialPortConfig);

  const char* manufacturer = sp_get_port_usb_manufacturer(m_pSerialPort);
  if (manufacturer && strcmp(manufacturer, "Espressif") == 0)
  {
    // Native USB connection, hopefully an ESP32 S3.
    m_s3 = true;
  }

  sp_set_baudrate(m_pSerialPort, m_s3 ? ZEDMD_S3_COMM_BAUD_RATE : ZEDMD_COMM_BAUD_RATE);
  sp_set_bits(m_pSerialPort, 8);
  sp_set_parity(m_pSerialPort, SP_PARITY_NONE);
  sp_set_stopbits(m_pSerialPort, 1);
  sp_set_xon_xoff(m_pSerialPort, SP_XONXOFF_DISABLED);

  Reset();

  uint8_t data[8] = {0};

  while (sp_input_waiting(m_pSerialPort) > 0)
  {
    sp_blocking_read(m_pSerialPort, data, 8, ZEDMD_COMM_SERIAL_READ_TIMEOUT);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  data[0] = ZEDMD_COMM_COMMAND::Handshake;
  sp_blocking_write(m_pSerialPort, (void*)CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
  sp_blocking_write(m_pSerialPort, (void*)data, 1, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  if (sp_blocking_read(m_pSerialPort, data, 8, ZEDMD_COMM_SERIAL_READ_TIMEOUT))
  {
    if (!memcmp(data, CTRL_CHARS_HEADER, 4))
    {
      m_width = data[4] + data[5] * 256;
      m_height = data[6] + data[7] * 256;
      m_zoneWidth = m_width / 16;
      m_zoneHeight = m_height / 8;

      if (sp_blocking_read(m_pSerialPort, data, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT) && data[0] == 'R')
      {
        data[0] = ZEDMD_COMM_COMMAND::Compression;
        sp_blocking_write(m_pSerialPort, (void*)CTRL_CHARS_HEADER, 6, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
        sp_blocking_write(m_pSerialPort, (void*)data, 1, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
        std::this_thread::sleep_for(std::chrono::milliseconds(4));

        if (sp_blocking_read(m_pSerialPort, data, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT) && data[0] == 'A' &&
            sp_blocking_read(m_pSerialPort, data, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT) && data[0] == 'R')
        {
          data[0] = ZEDMD_COMM_COMMAND::Chunk;
          data[1] = ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE / 32;
          sp_blocking_write(m_pSerialPort, (void*)CTRL_CHARS_HEADER, 6, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
          sp_blocking_write(m_pSerialPort, (void*)data, 2, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
          std::this_thread::sleep_for(std::chrono::milliseconds(4));

          if (sp_blocking_read(m_pSerialPort, data, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT) && data[0] == 'A' &&
              sp_blocking_read(m_pSerialPort, data, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT) && data[0] == 'R')
          {
            data[0] = ZEDMD_COMM_COMMAND::EnableFlowControlV2;
            sp_blocking_write(m_pSerialPort, (void*)CTRL_CHARS_HEADER, 6, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
            sp_blocking_write(m_pSerialPort, (void*)data, 1, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
            std::this_thread::sleep_for(std::chrono::milliseconds(4));

            if (sp_blocking_read(m_pSerialPort, data, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT) && data[0] == 'A')
            {
              // Store the device name for reconnects.
              SetDevice(pDevice);
              m_noReadySignalCounter = 0;
              m_flowControlCounter = 1;

              Log("ZeDMD found: device=%s, width=%d, height=%d", pDevice, m_width, m_height);

              return true;
            }
          }
        }
      }
    }
  }

  Disconnect();
#endif

  return false;
}

bool ZeDMDComm::IsConnected()
{
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  return (m_pSerialPort != nullptr);
#else
  return false;
#endif
}

void ZeDMDComm::Reset()
{
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  if (!m_pSerialPort)
  {
    return;
  }

  if (m_s3)
  {
    // Hard reset, see
    // https://github.com/espressif/esptool/blob/master/esptool/reset.py
    sp_set_rts(m_pSerialPort, SP_RTS_ON);
    sp_set_dtr(m_pSerialPort, SP_DTR_OFF);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    sp_set_rts(m_pSerialPort, SP_RTS_OFF);
    sp_set_dtr(m_pSerialPort, SP_DTR_OFF);
    // At least under Linux we need to wait very long until the USB JTAG port
    // re-appears.
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  }
  else
  {
    // Could be an ESP32.
    sp_set_dtr(m_pSerialPort, SP_DTR_OFF);
    sp_set_rts(m_pSerialPort, SP_RTS_ON);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    sp_set_rts(m_pSerialPort, SP_RTS_OFF);
    sp_set_dtr(m_pSerialPort, SP_DTR_OFF);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    sp_flush(m_pSerialPort, SP_BUF_BOTH);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
#endif
}

bool ZeDMDComm::StreamBytes(ZeDMDFrame* pFrame)
{
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  uint8_t* data;
  int size;

  if (pFrame->size == 0)
  {
    size = CTRL_CHARS_HEADER_SIZE + 1;
    data = (uint8_t*)malloc(size);
    memcpy(data, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
    data[CTRL_CHARS_HEADER_SIZE] = pFrame->command;
  }
  else
  {
    mz_ulong compressedSize = mz_compressBound(pFrame->size);
    data = (uint8_t*)malloc(CTRL_CHARS_HEADER_SIZE + 3 + compressedSize);
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
    sp_blocking_read(m_pSerialPort, &flowControlCounter, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT);
  } while (flowControlCounter != 0 && flowControlCounter != m_flowControlCounter);

  if (flowControlCounter == m_flowControlCounter)
  {
    int position = 0;
    success = true;
    m_noReadySignalCounter = 0;

    while (position < size && success)
    {
      sp_blocking_write(m_pSerialPort, data + position,
                        ((size - position) < ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE) ? (size - position)
                                                                                  : ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE,
                        ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);

      uint8_t response;
      do
      {
        sp_blocking_read(m_pSerialPort, &response, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT);
      } while (response == flowControlCounter);

      if (response == 'A')
      {
        position += ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE;
      }
      else
      {
        success = false;
        Log("Write bytes failure: response=%c", response);
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
    Log("No Ready Signal, counter is %d", ++m_noReadySignalCounter);
    if (m_noReadySignalCounter > ZEDMD_COMM_NO_READY_SIGNAL_MAX)
    {
      Log("Too many missing Ready Signals, try to reconnect ...");
      Disconnect();
      Connect();
    }
  }

  free(data);

  return success;
#else
  return false;
#endif
}

uint16_t const ZeDMDComm::GetWidth() { return m_width; }

uint16_t const ZeDMDComm::GetHeight() { return m_height; }

bool const ZeDMDComm::IsS3() { return m_s3; }
