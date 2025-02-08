#include "ZeDMDComm.h"

#include "komihash/komihash.h"
#include "miniz/miniz.h"

ZeDMDComm::ZeDMDComm()
{
  m_keepAliveInterval = std::chrono::milliseconds(ZEDMD_COMM_KEEP_ALIVE_INTERVAL);

  m_stopFlag.store(false, std::memory_order_release);
  m_fullFrameFlag.store(false, std::memory_order_release);

  m_pThread = nullptr;
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
  m_pSerialPort = nullptr;
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
  m_lastKeepAlive = std::chrono::steady_clock::now();

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

            KeepAlive();
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
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
          }
        }

        Log("ZeDMDComm run thread finished");
      });
}

void ZeDMDComm::ClearFrames()
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

void ZeDMDComm::QueueCommand(char command, uint8_t* data, int size)
{
  if (!IsConnected())
  {
    return;
  }

  if (FillDelayed())
  {
    ClearFrames();
  }

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
  if (m_fullFrameFlag.load(std::memory_order_relaxed))
  {
    m_fullFrameFlag.store(false, std::memory_order_release);
    ClearFrames();
    memset(m_zoneHashes, 0, sizeof(m_zoneHashes));

    return;
  }

  if (0 == memcmp(data, m_allBlack, size))
  {
    // Queue a clear screen command. Don't call QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen) because we need to set
    // black hashes.
    ZeDMDFrame frame(ZEDMD_COMM_COMMAND::ClearScreen);

    // If ZeDMD is already behind, clear the screen immediately.
    if (FillDelayed())
    {
      ClearFrames();
    }

    m_frameQueueMutex.lock();
    m_frames.push(std::move(frame));
    m_frameQueueMutex.unlock();

    // Use "1" as hash for black.
    memset(m_zoneHashes, 1, sizeof(m_zoneHashes));

    return;
  }

  uint8_t idx = 0;
  uint16_t zonesBytesLimit = (m_s3 && !m_cdc) ? ZEDMD_S3_ZONES_BYTE_LIMIT : ZEDMD_ZONES_BYTE_LIMIT;
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

    // Probe USB devices first, before native devices.
    for (int usb = 1; usb >= 0; usb--)
    {
      struct sp_port** ppPorts;
      sp_return result = sp_list_ports(&ppPorts);
      if (result == SP_OK)
      {
        for (int i = 0; ppPorts[i]; i++)
        {
          sp_transport transport = sp_get_port_transport(ppPorts[i]);
          // Ignore SP_TRANSPORT_BLUETOOTH.
          if (SP_TRANSPORT_USB != transport && SP_TRANSPORT_NATIVE != transport) continue;

          if (usb == 1 && SP_TRANSPORT_USB != transport) continue;
          if (usb == 0 && SP_TRANSPORT_NATIVE != transport) continue;

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
      if (success)
      {
        break;
      }
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

#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))
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
  sp_return result = sp_get_port_by_name(pDevice, &m_pSerialPort);
  sp_transport transport = sp_get_port_transport(m_pSerialPort);
  // Ignore SP_TRANSPORT_BLUETOOTH.
  if (result != SP_OK || SP_TRANSPORT_BLUETOOTH == transport)
  {
    return false;
  }

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
      // handled below. Newer versions of libserialport seem to report SP_TRANSPORT_USB on macOS as well.
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

  Log("ZeDMD candidate: device=%s", pDevice);

  result = sp_open(m_pSerialPort, SP_MODE_READ_WRITE);
  if (result != SP_OK)
  {
    Log("Unable to open device %s, error code %d", pDevice, result);
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }
  if (SP_OK != sp_set_baudrate(m_pSerialPort, m_cdc ? 115200 : (m_s3 ? ZEDMD_S3_COMM_BAUD_RATE : ZEDMD_COMM_BAUD_RATE)))
  {
    Log("Unable to set baudrate on device %s, error code %d", pDevice, result);
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }
  if (SP_OK != sp_set_bits(m_pSerialPort, 8))
  {
    Log("Unable to set bits on device %s, error code %d", pDevice, result);
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }
  if (SP_OK != sp_set_parity(m_pSerialPort, SP_PARITY_NONE))
  {
    Log("Unable to set parity on device %s, error code %d", pDevice, result);
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }
  if (SP_OK != sp_set_stopbits(m_pSerialPort, 1))
  {
    Log("Unable to to set stopbits on device %s, error code %d", pDevice, result);
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }
  if (SP_OK != sp_set_xon_xoff(m_pSerialPort, SP_XONXOFF_DISABLED))
  {
    Log("Unable to set xon xoff on device %s, error code %d", pDevice, result);
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }
  if (SP_OK != sp_set_flowcontrol(m_pSerialPort, SP_FLOWCONTROL_NONE))
  {
    Log("Unable to set flowcontrol on device %s, error code %d", pDevice, result);
    sp_free_port(m_pSerialPort);
    m_pSerialPort = nullptr;

    return false;
  }

  // Reset();

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
  uint8_t* data = (uint8_t*)malloc(ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE);

  // Sometimes, the ESP sends some debug output after reset which is still in the buffer.
  sp_flush(m_pSerialPort, SP_BUF_BOTH);
  while (sp_input_waiting(m_pSerialPort) > 0)
  {
    sp_nonblocking_read(m_pSerialPort, data, ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE);
  }

  // For Linux and macOS, 200ms seem to be sufficient. But some Windows installations require a longer sleep here.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  memset(data, 0, ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE);
  memcpy(data, FRAME_HEADER, FRAME_HEADER_SIZE);
  memcpy(&data[FRAME_HEADER_SIZE], CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
  data[FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE] = ZEDMD_COMM_COMMAND::Handshake;
  data[FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE + 1] = 0;  // Size high byte
  data[FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE + 2] = 0;  // Size low byte
  data[FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE + 3] = 0;  // Compression flag
  int result =
      sp_blocking_write(m_pSerialPort, data, ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
  if (result >= ZEDMD_COMM_MIN_SERIAL_WRITE_AT_ONCE)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    memset(data, 0, ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE);
    result = sp_blocking_read(m_pSerialPort, data, 64, 500);

    if (result == 64 && memcmp(data, CTRL_CHARS_HEADER, 4) == 0)
    {
      if (data[57] == 'R')
      {
        m_width = data[4] + data[5] * 256;
        m_height = data[6] + data[7] * 256;
        m_zoneWidth = m_width / 16;
        m_zoneHeight = m_height / 8;
        snprintf(m_firmwareVersion, 12, "%d.%d.%d", data[8], data[9], data[10]);
        m_writeAtOnce = data[11] + data[12] * 256;

        m_brightness = data[13];
        m_rgbMode = data[14];
        m_yOffset = data[15];
        m_panelClkphase = data[16];
        m_panelDriver = data[17];
        m_panelI2sspeed = data[18];
        m_panelLatchBlanking = data[19];
        m_panelMinRefreshRate = data[20];
        m_udpDelay = data[21];

        // Store the device name for reconnects.
        SetDevice(pDevice);
        Log("ZeDMD %s found: %sdevice=%s, width=%d, height=%d", m_firmwareVersion, m_s3 ? "S3 " : "", pDevice, m_width,
            m_height);

        // Next streaming needs to be complete.
        memset(m_zoneHashes, 0, sizeof(m_zoneHashes));

        while (sp_input_waiting(m_pSerialPort) > 0)
        {
          sp_nonblocking_read(m_pSerialPort, data, ZEDMD_COMM_MAX_SERIAL_WRITE_AT_ONCE);
        }

        free(data);
        return true;
      }
      else
      {
        Log("ZeDMD found but ready signal is missing.");
      }
    }
  }
  free(data);
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
  DisableKeepAlive();
  QueueCommand(ZEDMD_COMM_COMMAND::Reset);
  // Wait a bit to let the reset command be transmitted.
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  EnableKeepAlive();
}

bool ZeDMDComm::StreamBytes(ZeDMDFrame* pFrame)
{
  static uint8_t payload[36864] = {0};
  memset(payload, 0, 36864);
  memcpy(payload, FRAME_HEADER, FRAME_HEADER_SIZE);
  uint16_t pos = FRAME_HEADER_SIZE;

  m_lastKeepAlive = std::chrono::steady_clock::now();

  for (auto it = pFrame->data.rbegin(); it != pFrame->data.rend(); ++it)
  {
    ZeDMDFrameData frameData = *it;

    if (pFrame->command != ZEDMD_COMM_COMMAND::RGB565ZonesStream)
    {
      memcpy(&payload[pos], CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
      pos += CTRL_CHARS_HEADER_SIZE;
      payload[pos++] = pFrame->command;
      payload[pos++] = (uint8_t)(frameData.size >> 8 & 0xFF);  // Size high byte
      payload[pos++] = (uint8_t)(frameData.size & 0xFF);       // Size low byte
      payload[pos++] = 0;                                      // Compression flag
      if (frameData.size > 0)
      {
        memcpy(&payload[pos], frameData.data, frameData.size);
        pos += frameData.size;
      }
    }
    else
    {
      mz_ulong compressedSize = mz_compressBound(frameData.size);
      memcpy(&payload[pos], CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
      pos += CTRL_CHARS_HEADER_SIZE;
      payload[pos++] = pFrame->command;
      int status = mz_compress(&payload[pos + 3], &compressedSize, frameData.data, frameData.size);
      if (status != MZ_OK || compressedSize > frameData.size || 0 >= compressedSize)
      {
        if (status != MZ_OK)
        {
          Log("Compression error. Status: %d, Frame Size: %d, Compressed Size: %d", status, frameData.size,
              compressedSize);
        }
        payload[pos++] = (uint8_t)(frameData.size >> 8 & 0xFF);  // Size high byte
        payload[pos++] = (uint8_t)(frameData.size & 0xFF);       // Size low byte
        payload[pos++] = 0;                                      // Compression flag
        if (frameData.size > 0)
        {
          memcpy(&payload[pos], frameData.data, frameData.size);
          pos += frameData.size;
        }
      }
      else
      {
        payload[pos++] = (uint8_t)(compressedSize >> 8 & 0xFF);  // Size high byte
        payload[pos++] = (uint8_t)(compressedSize & 0xFF);       // Size low byte
        payload[pos++] = 1;                                      // Compression flag
        pos += compressedSize;
      }
    }
  }

  if (pFrame->command == ZEDMD_COMM_COMMAND::RGB565ZonesStream)
  {
    memcpy(&payload[pos], CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
    pos += CTRL_CHARS_HEADER_SIZE;
    payload[pos++] = ZEDMD_COMM_COMMAND::RenderRGB565Frame;
    payload[pos++] = 0;  // Size high byte
    payload[pos++] = 0;  // Size low byte
    payload[pos++] = 0;  // Compression flag
  }

  if (!SendChunks(payload, pos)) return false;

  m_lastKeepAlive = std::chrono::steady_clock::now();

  return true;
}

bool ZeDMDComm::SendChunks(uint8_t* pData, uint16_t size)
{
#if !(                                                                                                                \
    (defined(__APPLE__) && ((defined(TARGET_OS_IOS) && TARGET_OS_IOS) || (defined(TARGET_OS_TV) && TARGET_OS_TV))) || \
    defined(__ANDROID__))

  static uint8_t guru[4] = {'G', 'u', 'r', 'u'};
  uint8_t ack[CTRL_CHARS_HEADER_SIZE + 1] = {0};
  int status = 0;
  uint16_t sent = 0;
  uint8_t message[64];

  while (sp_input_waiting(m_pSerialPort) > 0)
  {
    sp_nonblocking_read(m_pSerialPort, message, 64);
  }

  while (sent < size && !m_stopFlag.load(std::memory_order_relaxed))
  {
    // std::this_thread::sleep_for(std::chrono::milliseconds(8));
    int toSend = ((size - sent) < m_writeAtOnce) ? size - sent : m_writeAtOnce;
    if (toSend < m_writeAtOnce)
    {
      uint8_t* padded = (uint8_t*)malloc(m_writeAtOnce);
      memset(padded, 0, m_writeAtOnce);
      memcpy(padded, &pData[sent], toSend);
      if (m_cdc)
      {
        status = sp_blocking_write(m_pSerialPort, padded, m_writeAtOnce, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
      }
      else
      {
        status = sp_nonblocking_write(m_pSerialPort, padded, m_writeAtOnce);
      }
      free(padded);
    }
    else
    {
      if (m_cdc)
      {
        status = sp_blocking_write(m_pSerialPort, &pData[sent], toSend, ZEDMD_COMM_SERIAL_WRITE_TIMEOUT);
      }
      else
      {
        status = sp_nonblocking_write(m_pSerialPort, &pData[sent], toSend);
      }
    }

    if (status < toSend)
    {
      m_fullFrameFlag.store(true, std::memory_order_release);
      Log("Full frame forced, error %d", status);
      return false;
    }
    sent += status;

    memset(ack, 0, CTRL_CHARS_HEADER_SIZE + 1);
    status = sp_blocking_read(m_pSerialPort, ack, CTRL_CHARS_HEADER_SIZE + 1, ZEDMD_COMM_SERIAL_READ_TIMEOUT);

    if (status < (CTRL_CHARS_HEADER_SIZE + 1) || memcmp(ack, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE) != 0 ||
        ack[CTRL_CHARS_HEADER_SIZE] == 'F')
    {
      if (memcmp(ack, guru, 4) == 0 || memcmp(&ack[1], guru, 4) == 0 || memcmp(&ack[2], guru, 4) == 0)
      {
        Log("ZeDMD %s", ack);
        uint8_t message[64];
        while (sp_input_waiting(m_pSerialPort) > 0)
        {
          memset(message, ' ', 64);
          sp_nonblocking_read(m_pSerialPort, message, 64);
          Log("%s", message);
        }
      }
      else
      {
        Log("Full frame forced, error %d", status);
      }

      m_fullFrameFlag.store(true, std::memory_order_release);
      return false;
    }

    if (ack[CTRL_CHARS_HEADER_SIZE] != 'A')
    {
      m_fullFrameFlag.store(true, std::memory_order_release);
      Log("Full frame forced, error %d", status);
      return false;
    }
  }

  return true;

#endif
  return false;
}

void ZeDMDComm::KeepAlive()
{
  auto now = std::chrono::steady_clock::now();

  if (!m_keepAlive)
  {
    m_lastKeepAlive = now;
    return;
  }

  if (now - m_lastKeepAlive > m_keepAliveInterval)
  {
    m_lastKeepAlive = now;

    uint16_t size = FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE + 4;
    uint8_t* pData = (uint8_t*)malloc(size);
    memcpy(pData, FRAME_HEADER, FRAME_HEADER_SIZE);
    memcpy(&pData[FRAME_HEADER_SIZE], CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
    pData[FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE] = ZEDMD_COMM_COMMAND::KeepAlive;
    pData[FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE + 1] = 0;  // Size high byte
    pData[FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE + 2] = 0;  // Size low byte
    pData[FRAME_HEADER_SIZE + CTRL_CHARS_HEADER_SIZE + 3] = 0;  // Compression flag
    SendChunks(pData, size);
    free(pData);
  }
}

uint16_t const ZeDMDComm::GetWidth() { return m_width; }

uint16_t const ZeDMDComm::GetHeight() { return m_height; }

bool const ZeDMDComm::IsS3() { return m_s3; }

uint8_t ZeDMDComm::GetTransport() { return 0; }

const char* ZeDMDComm::GetWiFiSSID() { return ""; }

void ZeDMDComm::StoreWiFiPassword() {}

int ZeDMDComm::GetWiFiPort() { return 3333; };
