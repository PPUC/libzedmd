#include "ZeDMDComm.h"

#include "komihash/komihash.h"
#include "miniz/miniz.h"

ZeDMDComm::ZeDMDComm()
{
  m_stopFlag.store(false, std::memory_order_release);
  m_fullFrameFlag.store(false, std::memory_order_release);

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
  m_stopFlag.store(true, std::memory_order_release);

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
        m_stopFlag.load(std::memory_order_acquire);

        while (IsConnected() && !m_stopFlag.load(std::memory_order_relaxed))
        {
          m_frameQueueMutex.lock();

          if (m_frames.empty())
          {
            m_delayedFrameMutex.lock();
            // All frames are sent, move delayed frame into the frames queue.
            if (m_delayedFrameReady)
            {
              m_frames.push(std::move(m_delayedFrame));
              m_delayedFrameReady = false;
              m_delayedFrameMutex.unlock();
              m_frameQueueMutex.unlock();

              continue;
            }
            m_delayedFrameMutex.unlock();
            m_frameQueueMutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            continue;
          }

          if (m_frames.front().data.empty())
          {
            // In case of a simple command, add metadata to indicate that the payload data size is 0.
            m_frames.front().data.emplace_back(nullptr, 0);
          }
          bool success = StreamBytes(&(m_frames.front()));
          m_frames.pop();

          m_frameQueueMutex.unlock();

          if (!success)
          {
            Log("ZeDMD StreamBytes failed");

            // Allow ZeDMD to empty its buffers.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
          }
        }

        Log("ZeDMDComm run thread finished");
      });
}

void ZeDMDComm::QueueCommand(char command, uint8_t* data, int size)
{
  if (!IsConnected())
  {
    return;
  }

  // "Delete" delayed frame.
  m_delayedFrameMutex.lock();
  m_delayedFrameReady = false;
  m_delayedFrameMutex.unlock();

  ZeDMDFrame frame(command, data, size);

  m_frameQueueMutex.lock();
  m_frames.push(std::move(frame));
  m_frameQueueMutex.unlock();

  // Next streaming needs to be complete, except black zones.
  memset(m_zoneHashes, ZEDMD_COMM_COMMAND::ClearScreen == command ? 1 : 0, sizeof(m_zoneHashes));
}

void ZeDMDComm::QueueCommand(char command, uint8_t value) { QueueCommand(command, &value, 1); }

void ZeDMDComm::QueueCommand(char command) { QueueCommand(command, nullptr, 0); }

void ZeDMDComm::QueueFrame(uint8_t* data, int size)
{
  if (0 == memcmp(data, m_allBlack, size) || m_fullFrameFlag.load(std::memory_order_relaxed))
  {
    m_fullFrameFlag.store(false, std::memory_order_release);

    // Queue a clear screen command. Don't call QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen) because we need to set
    // black hashes.
    ZeDMDFrame frame(ZEDMD_COMM_COMMAND::ClearScreen);

    // If ZeDMD is already behind, clear the screen immediately.
    if (FillDelayed())
    {
      m_frameQueueMutex.lock();
      while (!m_frames.empty())
      {
        m_frames.pop();
      }
      m_frameQueueMutex.unlock();

      // "Delete" delayed frame.
      m_delayedFrameMutex.lock();
      m_delayedFrameReady = false;
      m_delayedFrameMutex.unlock();
    }

    m_frameQueueMutex.lock();
    m_frames.push(std::move(frame));
    m_frameQueueMutex.unlock();

    // Use "1" as hash for black.
    memset(m_zoneHashes, 1, sizeof(m_zoneHashes));

    return;
  }

  uint8_t idx = 0;
  uint16_t zonesBytesLimit = ZEDMD_ZONES_BYTE_LIMIT;
  const uint16_t zoneBytes = m_zoneWidth * m_zoneHeight * 2;
  const uint16_t zoneBytesTotal = zoneBytes + 1;
  uint8_t* zone = (uint8_t*)malloc(zoneBytes);
  uint8_t* buffer = (uint8_t*)malloc(zonesBytesLimit);
  uint16_t bufferPosition = 0;
  const uint16_t bufferSizeThreshold = zonesBytesLimit - zoneBytesTotal;

  ZeDMDFrame frame(ZEDMD_COMM_COMMAND::RGB565ZonesStream);

  bool delayed = FillDelayed();
  if (delayed)
  {
    // A delayed frame needs to be complete.
    memset(m_zoneHashes, 0, sizeof(m_zoneHashes));
  }

  memset(buffer, 0, zonesBytesLimit);
  for (uint16_t y = 0; y < m_height; y += m_zoneHeight)
  {
    for (uint16_t x = 0; x < m_width; x += m_zoneWidth)
    {
      for (uint8_t z = 0; z < m_zoneHeight; z++)
      {
        memcpy(&zone[z * m_zoneWidth * 2], &data[((y + z) * m_width + x) * 2], m_zoneWidth * 2);
      }

      bool black = (0 == memcmp(zone, m_allBlack, zoneBytes));
      // Use "1" as hash for black.
      uint64_t hash = black ? 1 : komihash(zone, zoneBytes, 0);
      if (hash != m_zoneHashes[idx])
      {
        m_zoneHashes[idx] = hash;

        if (black)
        {
          // In case of a full black zone, just send the zone index ID and add 128.
          buffer[bufferPosition++] = idx + 128;
        }
        else
        {
          buffer[bufferPosition++] = idx;
          memcpy(&buffer[bufferPosition], zone, zoneBytes);
          bufferPosition += zoneBytes;
        }

        if (bufferPosition > bufferSizeThreshold)
        {
          frame.data.emplace_back(buffer, bufferPosition);
          memset(buffer, 0, zonesBytesLimit);
          bufferPosition = 0;
        }
      }

      idx++;
    }
  }

  if (bufferPosition > 0)
  {
    frame.data.emplace_back(buffer, bufferPosition);
  }

  free(buffer);
  free(zone);

  if (delayed)
  {
    m_delayedFrameMutex.lock();
    m_delayedFrame = std::move(frame);
    m_delayedFrameReady = true;
    m_delayedFrameMutex.unlock();
  }
  else
  {
    m_frameQueueMutex.lock();
    m_frames.push(std::move(frame));
    m_frameQueueMutex.unlock();
  }
}

bool ZeDMDComm::FillDelayed()
{
  uint8_t size = 0;
  bool delayed = false;
  m_frameQueueMutex.lock();
  size = m_frames.size();
  delayed = m_delayedFrameReady || (size >= ZEDMD_COMM_FRAME_QUEUE_SIZE_MAX);
  m_frameQueueMutex.unlock();
  if (delayed) Log("ZeDMD, next frame will be delayed");
  return delayed;
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
        enum sp_transport transport = sp_get_port_transport(ppPorts[i]);
        // Ignore SP_TRANSPORT_BLUETOOTH.
        if (SP_TRANSPORT_USB != transport && SP_TRANSPORT_NATIVE != transport) continue;

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
        // Ignore Bluetooth and debug on macOS.
        if (!ignored && !strstr(pDevice, "tooth") && !strstr(pDevice, "debug"))
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

  SoftReset();

#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
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
  enum sp_transport transport = sp_get_port_transport(m_pSerialPort);
  // Ignore SP_TRANSPORT_BLUETOOTH.
  if (result != SP_OK || SP_TRANSPORT_BLUETOOTH == transport)
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

  if (SP_TRANSPORT_USB == transport)
  {
    int usb_vid, usb_pid;
    result = sp_get_port_usb_vid_pid(m_pSerialPort, &usb_vid, &usb_pid);
    if (result != SP_OK)
    {
      sp_free_port(m_pSerialPort);
      m_pSerialPort = nullptr;

      return false;
    }

    if (0x303a == usb_vid && 0x1001 == usb_pid)
    {
      // USB JTAG/serial debug unit, hopefully an ESP32 S3.
      m_s3 = true;
      m_cdc = true;
    }
    else if (0x1a86 == usb_vid && 0x55d3 == usb_pid)
    {
      // QinHeng Electronics USB Single Serial, hopefully an ESP32 S3.
      m_s3 = true;
    }
    else if (0x10c4 == usb_vid && 0xea60 == usb_pid)
    {
      // CP210x, could be an ESP32.
      // On Windows, libserialport reports SP_TRANSPORT_USB, on Linux and macOS SP_TRANSPORT_NATIVE is reported and
      // handled below.
    }
    else
    {
      sp_free_port(m_pSerialPort);
      m_pSerialPort = nullptr;

      return false;
    }
  }
  else if (SP_TRANSPORT_NATIVE != transport)
  {
    // Bluetooth could not be a ZeDMD.
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }

  sp_set_baudrate(m_pSerialPort, m_cdc ? 115200 : (m_s3 ? ZEDMD_S3_COMM_BAUD_RATE : ZEDMD_COMM_BAUD_RATE));
  sp_set_bits(m_pSerialPort, 8);
  sp_set_parity(m_pSerialPort, SP_PARITY_NONE);
  sp_set_stopbits(m_pSerialPort, 1);
  sp_set_xon_xoff(m_pSerialPort, SP_XONXOFF_DISABLED);
  sp_set_flowcontrol(m_pSerialPort, SP_FLOWCONTROL_NONE);

  Reset();

  if (Handshake(pDevice)) return true;

  Disconnect();
#endif

  return false;
}

bool ZeDMDComm::Handshake(char* pDevice)
{
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  uint8_t data[8] = {0};

  data[0] = ZEDMD_COMM_COMMAND::Handshake;
  sp_nonblocking_write(m_pSerialPort, (void*)CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
  sp_blocking_write(m_pSerialPort, (void*)data, 1, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto handshake_start_time = std::chrono::steady_clock::now();
  std::chrono::milliseconds duration(ZEDMD_COMM_SERIAL_READ_TIMEOUT);

  // Sometimes, the driver seems to initilize the buffer with "garbage".
  // So we read until the first expected char occures or a timeout is reached.
  while (true)
  {
    sp_blocking_read(m_pSerialPort, &data[0], 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT);
    if (CTRL_CHARS_HEADER[0] == data[0])
    {
      break;
    }

    auto current_time = std::chrono::steady_clock::now();
    if (current_time - handshake_start_time >= duration || m_stopFlag.load(std::memory_order_relaxed))
    {
      return false;
    }
  }

  if (sp_blocking_read(m_pSerialPort, &data[1], 7, ZEDMD_COMM_SERIAL_READ_TIMEOUT))
  {
    if (memcmp(data, CTRL_CHARS_HEADER, 4) == 0)
    {
      m_width = data[5] + data[5] * 256;
      m_height = data[6] + data[7] * 256;
      m_zoneWidth = m_width / 16;
      m_zoneHeight = m_height / 8;

      if (sp_blocking_read(m_pSerialPort, data, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT) && data[0] == 'R')
      {
        // Store the device name for reconnects.
        SetDevice(pDevice);
        Log("ZeDMD found: %sdevice=%s, width=%d, height=%d", m_s3 ? "S3 " : "", pDevice, m_width, m_height);

        // Next streaming needs to be complete.
        memset(m_zoneHashes, 0, sizeof(m_zoneHashes));

        return true;
      }
    }
  }
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
    // At least under Linux and macOs we need to wait very long until the
    // USB JTAG port re-appears.
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
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

void ZeDMDComm::SoftReset()
{
  QueueCommand(ZEDMD_COMM_COMMAND::Reset);
  // Wait a bit to let the reset command be transmitted.
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

bool ZeDMDComm::StreamBytes(ZeDMDFrame* pFrame)
{
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))

  uint8_t* pData;
  uint16_t size;

  for (auto it = pFrame->data.rbegin(); it != pFrame->data.rend(); ++it)
  {
    ZeDMDFrameData frameData = *it;

    if (pFrame->command != ZEDMD_COMM_COMMAND::RGB565ZonesStream)
    {
      size = CTRL_CHARS_HEADER_SIZE + 1 + frameData.size;
      pData = (uint8_t*)malloc(size);
      memcpy(pData, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
      pData[CTRL_CHARS_HEADER_SIZE] = pFrame->command;
      if (frameData.size > 0)
      {
        memcpy(pData + CTRL_CHARS_HEADER_SIZE + 1, frameData.data, frameData.size);
      }
    }
    else
    {
      size = CTRL_CHARS_HEADER_SIZE + 1;
      pData = (uint8_t*)malloc(size);
      memcpy(pData, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
      pData[CTRL_CHARS_HEADER_SIZE] = ZEDMD_COMM_COMMAND::AnnounceRGB565ZonesStream;

      bool success = SendChunks(pData, size);
      free(pData);
      if (!success) return false;

      mz_ulong compressedSize = mz_compressBound(ZEDMD_ZONES_BYTE_LIMIT);
      pData = (uint8_t*)malloc(CTRL_CHARS_HEADER_SIZE + 3 + ZEDMD_ZONES_BYTE_LIMIT);
      memcpy(pData, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
      pData[CTRL_CHARS_HEADER_SIZE] = pFrame->command;
      mz_compress(pData + CTRL_CHARS_HEADER_SIZE + 3, &compressedSize, frameData.data, frameData.size);
      size = CTRL_CHARS_HEADER_SIZE + 3 + compressedSize;
      pData[CTRL_CHARS_HEADER_SIZE + 1] = (uint8_t)(compressedSize >> 8 & 0xFF);
      pData[CTRL_CHARS_HEADER_SIZE + 2] = (uint8_t)(compressedSize & 0xFF);
      if ((0 == pData[CTRL_CHARS_HEADER_SIZE + 1] && 0 == pData[CTRL_CHARS_HEADER_SIZE + 2]) ||
          compressedSize > (ZEDMD_ZONES_BYTE_LIMIT))
      {
        Log("Compression error");
        free(pData);
        return false;
      }
    }

    bool success = SendChunks(pData, size);
    free(pData);
    if (!success) return false;
  }

  if (m_s3 && pFrame->command == ZEDMD_COMM_COMMAND::RGB565ZonesStream)
  {
    size = CTRL_CHARS_HEADER_SIZE + 1;
    pData = (uint8_t*)malloc(size);
    memcpy(pData, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
    pData[CTRL_CHARS_HEADER_SIZE] = ZEDMD_COMM_COMMAND::RenderRGB565Frame;

    bool success = SendChunks(pData, size);
    free(pData);
    if (!success) return false;
  }

  return true;
#else
  return false;
#endif
}

bool ZeDMDComm::SendChunks(uint8_t* pData, uint16_t size)
{
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))

  int8_t status = 0;

  if (!m_stopFlag.load(std::memory_order_relaxed))
  {
    int position = 0;
    // USB CDC uses a fixed chunk size of 512 bytes.
    const uint16_t writeAtOnce =
        m_cdc ? 512 : (m_s3 ? ZEDMD_S3_COMM_MAX_SERIAL_WRITE_AT_ONCE : ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE);

    while (position < size)
    {
      sp_nonblocking_write(m_pSerialPort, &pData[position],
                           ((size - position) < writeAtOnce) ? (size - position) : writeAtOnce);

      position += writeAtOnce;
    }

    uint8_t response = 255;
    uint8_t timeouts = 0;
    do
    {
      status = sp_blocking_read(m_pSerialPort, &response, 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT);
      if (0 == status && ++timeouts >= ZEDMD_COMM_NUM_TIMEOUTS_TO_WAIT_FOR_ACKNOWLEDGE) break;
    } while ((status != 1 || (status == 1 && response != 'A' && response != 'E' && response != 'F')) &&
             !m_stopFlag.load(std::memory_order_relaxed));

    if (response == 'A')
    {
      if (m_noAcknowledgeCounter > 0) m_noAcknowledgeCounter--;
    }
    else if (response == 'F')
    {
      m_fullFrameFlag.store(true, std::memory_order_release);
    }
    else
    {
      if (++m_noAcknowledgeCounter > 64)
      {
        SoftReset();
        Log("Resetted device", response);
        Handshake(m_device);
      }
      else
      {
        Log("Write bytes failure: response=%d", response);
      }
      return false;
    }

    return true;
  }
#endif
  return false;
}

uint16_t const ZeDMDComm::GetWidth() { return m_width; }

uint16_t const ZeDMDComm::GetHeight() { return m_height; }

bool const ZeDMDComm::IsS3() { return m_s3; }
