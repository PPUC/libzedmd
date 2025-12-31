#include "ZeDMD.h"

#include <cstdlib>
#include <cstring>

#include "FrameUtil.h"
#include "ZeDMDComm.h"
#include "ZeDMDSpi.h"
#include "ZeDMDWiFi.h"

const int endian_check = 1;
#define is_bigendian() ((*(char*)&endian_check) == 0)

ZeDMD::ZeDMD()
{
  m_romWidth = 0;
  m_romHeight = 0;

  m_pFrameBuffer = nullptr;
  m_pScaledFrameBuffer = nullptr;
  m_pRgb565Buffer = nullptr;

  m_pZeDMDComm = new ZeDMDComm();
  m_pZeDMDWiFi = new ZeDMDWiFi();
  m_pZeDMDSpi = new ZeDMDSpi();
}

ZeDMD::~ZeDMD()
{
  delete m_pZeDMDComm;
  delete m_pZeDMDWiFi;
  delete m_pZeDMDSpi;

  if (m_pFrameBuffer)
  {
    free(m_pFrameBuffer);
    m_pFrameBuffer = nullptr;
  }

  if (m_pScaledFrameBuffer)
  {
    free(m_pScaledFrameBuffer);
    m_pScaledFrameBuffer = nullptr;
  }

  if (m_pRgb565Buffer)
  {
    free(m_pRgb565Buffer);
    m_pRgb565Buffer = nullptr;
  }
}

void ZeDMD::SetActiveZeDMD(ZeDMDComm* pActive, bool usb, bool wifi, bool spi)
{
  m_pActiveZeDMD = pActive;
  m_usb = usb;
  m_wifi = wifi;
  m_spi = spi;
}

ZeDMDComm* ZeDMD::GetActiveZeDMD() const { return m_pActiveZeDMD; }

ZeDMDWiFi* ZeDMD::GetActiveZeDMDWiFi() const
{
  if (m_wifi)
  {
    return m_pZeDMDWiFi;
  }
  return nullptr;
}

ZeDMDSpi* ZeDMD::GetActiveZeDMDSpi() const
{
  if (m_spi)
  {
    return m_pZeDMDSpi;
  }
  return nullptr;
}

void ZeDMD::SetLogCallback(ZeDMD_LogCallback callback, const void* userData)
{
  m_pZeDMDComm->SetLogCallback(callback, userData);
  m_pZeDMDWiFi->SetLogCallback(callback, userData);
  m_pZeDMDSpi->SetLogCallback(callback, userData);
}

void ZeDMD::Close()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::Close");

  if (!pActive)
  {
    return;
  }

  if (m_usb)
  {
    pActive->SoftReset(false);
    pActive->Disconnect();
  }
  else if (ZeDMDWiFi* pWiFi = GetActiveZeDMDWiFi())
  {
    pWiFi->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
    pWiFi->Flush(false);
    pWiFi->Disconnect();
  }
  else if (ZeDMDSpi* pSpi = GetActiveZeDMDSpi())
  {
    pSpi->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
    pSpi->Flush(false);
    pSpi->Disconnect();
  }

  SetActiveZeDMD(nullptr, false, false, false);
}

void ZeDMD::Reset()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::Reset");

  if (pActive && !m_spi)
  {
    pActive->SoftReset();
  }
}

void ZeDMD::RebootToBootloader()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::RebootToBootloader");

  if (m_usb && pActive)
  {
    pActive->RebootToBootloader();
  }
}

void ZeDMD::IgnoreDevice(const char* const ignore_device) { m_pZeDMDComm->IgnoreDevice(ignore_device); }

void ZeDMD::SetDevice(const char* const device) { m_pZeDMDComm->SetDevice(device); }

void ZeDMD::SetFrameSize(uint16_t width, uint16_t height)
{
  m_romWidth = width;
  m_romHeight = height;
}

uint16_t const ZeDMD::GetWidth()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive)
  {
    return pActive->GetWidth();
  }

  return 0;
}

uint16_t const ZeDMD::GetHeight()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive)
  {
    return pActive->GetHeight();
  }

  return 0;
}

uint16_t const ZeDMD::GetPanelWidth() { return GetWidth(); }

uint16_t const ZeDMD::GetPanelHeight()
{
  bool half = false;

  if (ZeDMDComm* pActive = GetActiveZeDMD())
  {
    half = pActive->IsHalf();
  }

  return (half ? (GetHeight() * 2) : GetHeight());
}

bool const ZeDMD::IsS3()
{
  if (ZeDMDComm* pActive = GetActiveZeDMD())
  {
    return pActive->IsS3();
  }

  return false;
}

const char* ZeDMD::GetVersion() { return ZEDMD_VERSION; }

const char* ZeDMD::GetFirmwareVersion()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetFirmwareVersion();
  }
  return "SPI";
}

uint16_t const ZeDMD::GetId()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetId();
  }
  return 0;
}

const char* ZeDMD::GetIdString()
{
  static char id[5] = {0};
  snprintf(id, 5, "%04X", GetId());
  return id;
}

const char* ZeDMD::GetWiFiSSID()
{
  if (ZeDMDWiFi* pWiFi = GetActiveZeDMDWiFi())
  {
    return pWiFi->GetWiFiSSID();
  }
  return "";
}

const char* ZeDMD::GetIp()
{
  if (ZeDMDWiFi* pWiFi = GetActiveZeDMDWiFi())
  {
    return pWiFi->GetIp();
  }
  return "";
}

const char* ZeDMD::GetDevice()
{
  if (m_usb && GetActiveZeDMD())
  {
    return GetActiveZeDMD()->GetDevice();
  }
  return "";
}

int ZeDMD::GetWiFiPort()
{
  if (ZeDMDWiFi* pWiFi = GetActiveZeDMDWiFi())
  {
    return pWiFi->GetWiFiPort();
  }
  return 0;
}

uint8_t ZeDMD::GetWiFiPower()
{
  if (ZeDMDWiFi* pWiFi = GetActiveZeDMDWiFi())
  {
    return pWiFi->GetWiFiPower();
  }
  return 0;
}

void ZeDMD::StoreWiFiPassword()
{
  if (ZeDMDWiFi* pWiFi = GetActiveZeDMDWiFi())
  {
    return pWiFi->StoreWiFiPassword();
  }
}

uint8_t ZeDMD::GetRGBOrder()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetRGBOrder();
  }
  return 0;
}

uint8_t ZeDMD::GetBrightness()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetBrightness();
  }
  return 0;
}

uint8_t ZeDMD::GetPanelClockPhase()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetPanelClockPhase();
  }
  return 0;
}

uint8_t ZeDMD::GetPanelI2sSpeed()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetPanelI2sSpeed();
  }
  return 0;
}

uint8_t ZeDMD::GetPanelLatchBlanking()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetPanelLatchBlanking();
  }
  return 0;
}

uint8_t ZeDMD::GetPanelMinRefreshRate()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetPanelMinRefreshRate();
  }
  return 0;
}

uint8_t ZeDMD::GetPanelDriver()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetPanelDriver();
  }
  return 0;
}

uint8_t ZeDMD::GetTransport()
{
  if (ZeDMDComm* pActive = GetActiveZeDMD())
  {
    return pActive->GetTransport();
  }
  return 0;
}

uint8_t ZeDMD::GetUdpDelay()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetUdpDelay();
  }
  return 0;
}

uint16_t ZeDMD::GetUsbPackageSize()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetUsbPackageSize();
  }
  return 0;
}

uint8_t ZeDMD::GetYOffset()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (pActive && !m_spi)
  {
    return pActive->GetYOffset();
  }
  return 0;
}

void ZeDMD::LedTest()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (!pActive)
  {
    return;
  }

  pActive->QueueCommand(ZEDMD_COMM_COMMAND::LEDTest);
  if (!m_spi)
  {
    pActive->DisableKeepAlive();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(8000));

  if (!m_spi)
  {
    pActive->EnableKeepAlive();
  }
}

void ZeDMD::EnableDebug()
{
  if (ZeDMDComm* pActive = GetActiveZeDMD())
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::EnableDebug);
  }
}

void ZeDMD::DisableDebug()
{
  if (ZeDMDComm* pActive = GetActiveZeDMD())
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::DisableDebug);
  }
}

void ZeDMD::EnableVerbose()
{
  m_verbose = true;
  m_pZeDMDComm->SetVerbose(true);
  m_pZeDMDWiFi->SetVerbose(true);
  m_pZeDMDSpi->SetVerbose(true);
}

void ZeDMD::DisableVerbose()
{
  m_verbose = false;
  m_pZeDMDComm->SetVerbose(false);
  m_pZeDMDWiFi->SetVerbose(false);
  m_pZeDMDSpi->SetVerbose(false);
}

void ZeDMD::SetRGBOrder(uint8_t rgbOrder)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetRGBOrder %d", rgbOrder);

  if (pActive)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::RGBOrder, rgbOrder);
  }
}

void ZeDMD::SetBrightness(uint8_t brightness)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetBrightness %d", brightness);

  if (pActive)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::Brightness, brightness);
  }
}

void ZeDMD::SetPanelClockPhase(uint8_t clockPhase)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetPanelClockPhase %d", clockPhase);

  if (pActive)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetClkphase, clockPhase);
  }
}

void ZeDMD::SetPanelI2sSpeed(uint8_t i2sSpeed)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetPanelI2sSpeed %d", i2sSpeed);

  if (pActive)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetI2sspeed, i2sSpeed);
  }
}

void ZeDMD::SetPanelLatchBlanking(uint8_t latchBlanking)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetPanelLatchBlanking %d", latchBlanking);

  if (pActive)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetLatchBlanking, latchBlanking);
  }
}

void ZeDMD::SetPanelMinRefreshRate(uint8_t minRefreshRate)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetPanelMinRefreshRate %d", minRefreshRate);

  if (pActive)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetMinRefreshRate, minRefreshRate);
  }
}

void ZeDMD::SetPanelDriver(uint8_t driver)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetPanelDriver %d", driver);

  if (pActive)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetDriver, driver);
  }
}

void ZeDMD::SetTransport(uint8_t transport)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetTransport %d", transport);

  if (pActive && !m_spi)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetTransport, transport);
  }
}

void ZeDMD::SetUdpDelay(uint8_t udpDelay)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetUdpDelay %d", udpDelay);

  if (pActive && !m_spi)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetUdpDelay, udpDelay);
  }
}

void ZeDMD::SetUsbPackageSize(uint16_t usbPackageSize)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetUsbPackageSize %d", usbPackageSize);

  uint8_t multiplier = (uint8_t)(usbPackageSize / 32);
  if (pActive && !m_spi)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetUsbPackageSizeMultiplier, multiplier);
  }
}

void ZeDMD::SetYOffset(uint8_t yOffset)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetYOffset %d", yOffset);

  if (pActive)
  {
    if (m_spi)
    {
      pActive->QueueCommand(ZEDMD_COMM_COMMAND::DisableDebug);
    }
    else
    {
      pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetYOffset, yOffset);
    }
  }
}

void ZeDMD::SaveSettings()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SaveSettings");

  if (!pActive)
  {
    return;
  }

  if (m_spi)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::DisableDebug);
    return;
  }

  pActive->QueueCommand(ZEDMD_COMM_COMMAND::SaveSettings);
  // Avoid that client resets the device before settings are saved.
  pActive->Flush();
}

void ZeDMD::EnableUpscaling()
{
  m_upscaling = true;
  m_hd = (GetWidth() == 256);
}

void ZeDMD::DisableUpscaling() { m_upscaling = false; }

void ZeDMD::SetWiFiSSID(const char* const ssid)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetWiFiSSID");

  int size = strlen(ssid);
  if (size <= 32 && pActive && !m_spi)
  {
    uint8_t data[32] = {0};
    memcpy(data, (uint8_t*)ssid, size);
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiSSID, data, size);
  }
}

void ZeDMD::SetWiFiPassword(const char* const password)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetWiFiPassword");

  int size = strlen(password);
  if (size <= 32 && pActive && !m_spi)
  {
    uint8_t data[32] = {0};
    memcpy(data, (uint8_t*)password, size);
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPassword, data, size);
  }
}

void ZeDMD::SetWiFiPort(int port)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetWiFiPort %d", port);

  uint8_t data[2];
  data[0] = (uint8_t)(port >> 8 & 0xFF);
  data[1] = (uint8_t)(port & 0xFF);
  if (pActive && !m_spi)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPort, data, 2);
  }
}

void ZeDMD::SetWiFiPower(uint8_t power)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::SetWiFiPower %d", power);

  uint8_t data[1];
  data[0] = power;
  if (pActive && !m_spi)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPower, data, 1);
  }
}

bool ZeDMD::OpenWiFi(const char* ip)
{
  if (m_verbose) m_pZeDMDComm->Log("ZeDMD::OpenWiFi %s", ip);

  m_wifi = m_pZeDMDWiFi->Connect(ip);

  if (m_wifi)
  {
    SetActiveZeDMD(m_pZeDMDWiFi, false, true, false);
    uint16_t width = m_pZeDMDWiFi->GetWidth();
    uint16_t height = m_pZeDMDWiFi->GetHeight();
    m_hd = (width == 256);

    m_pFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pScaledFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pRgb565Buffer = (uint8_t*)malloc(width * height * 2);

    m_pZeDMDWiFi->Run();
  }

  return m_wifi;
}

bool ZeDMD::OpenDefaultWiFi() { return OpenWiFi("zedmd-wifi.local"); }

bool ZeDMD::Open()
{
  m_usb = m_pZeDMDComm->Connect();

  if (m_usb)
  {
    SetActiveZeDMD(m_pZeDMDComm, true, false, false);
    uint16_t width = m_pZeDMDComm->GetWidth();
    uint16_t height = m_pZeDMDComm->GetHeight();
    m_hd = (width == 256);

    m_pFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pScaledFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pRgb565Buffer = (uint8_t*)malloc(width * height * 2);

    m_pZeDMDComm->Run();
  }

  return m_usb;
}

bool ZeDMD::Open(uint16_t width, uint16_t height)
{
  if (Open())
  {
    SetFrameSize(width, height);
  }

  return m_usb;
}

bool ZeDMD::OpenSpi(uint32_t speed, uint8_t framePause, uint16_t width, uint16_t height)
{
  m_pZeDMDSpi->SetSpeed(speed);
  m_pZeDMDSpi->SetFramePause(framePause);
  m_pZeDMDSpi->SetWidth(width);
  m_pZeDMDSpi->SetHeight(height);

  m_spi = m_pZeDMDSpi->Connect();

  if (m_spi)
  {
    SetActiveZeDMD(m_pZeDMDSpi, false, false, true);
    m_hd = (width == 256);

    m_pFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pScaledFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pRgb565Buffer = (uint8_t*)malloc(width * height * 2);

    m_pZeDMDSpi->Run();
  }

  return m_spi;
}

void ZeDMD::ClearScreen()
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::ClearScreen");

  if (pActive)
  {
    pActive->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
  }
  // "Blank" the frame buffer.
  if (m_pFrameBuffer)
  {
    memset(m_pFrameBuffer, 0, ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
  }
}

void ZeDMD::EnableTrueRgb888(bool enable) { m_rgb888 = enable; }

void ZeDMD::RenderRgb888(uint8_t* pFrame)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::RenderRgb888");

  if (!pActive || !UpdateFrameBuffer888(pFrame))
  {
    return;
  }

  int bufferSize = Scale888(m_pScaledFrameBuffer, m_pFrameBuffer, 3);

  if (m_rgb888)
  {
    pActive->QueueFrame(m_pScaledFrameBuffer, bufferSize, true);
  }
  else
  {
    int rgb565Size = bufferSize / 3;
    for (uint16_t i = 0; i < rgb565Size; i++)
    {
      uint16_t tmp = (((uint16_t)(m_pScaledFrameBuffer[i * 3] & 0xF8)) << 8) |
                     (((uint16_t)(m_pScaledFrameBuffer[i * 3 + 1] & 0xFC)) << 3) |
                     (m_pScaledFrameBuffer[i * 3 + 2] >> 3);
      m_pRgb565Buffer[i * 2 + 1] = tmp >> 8;
      m_pRgb565Buffer[i * 2] = tmp & 0xFF;
    }

    pActive->QueueFrame(m_pRgb565Buffer, rgb565Size * 2);
  }
}

void ZeDMD::RenderRgb565(uint16_t* pFrame)
{
  ZeDMDComm* pActive = GetActiveZeDMD();
  if (m_verbose && pActive) pActive->Log("ZeDMD::RenderRgb565");

  if (!pActive || !UpdateFrameBuffer565(pFrame))
  {
    return;
  }

  int size = Scale565(m_pScaledFrameBuffer, pFrame, is_bigendian());

  pActive->QueueFrame(m_pScaledFrameBuffer, size);
}

bool ZeDMD::UpdateFrameBuffer888(uint8_t* pFrame)
{
  if (!m_pFrameBuffer)
  {
    return false;
  }

  if (0 == memcmp(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 3))
  {
    return false;
  }

  memcpy(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 3);
  return true;
}

bool ZeDMD::UpdateFrameBuffer565(uint16_t* pFrame)
{
  if (!m_pFrameBuffer)
  {
    return false;
  }

  if (0 == memcmp(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 2))
  {
    return false;
  }

  memcpy(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 2);
  return true;
}

uint8_t ZeDMD::GetScaleMode(uint16_t frameWidth, uint16_t frameHeight, uint8_t* pXOffset, uint8_t* pYOffset)
{
  if (m_romWidth == 192 && frameWidth == 256)
  {
    (*pXOffset) = 32;
    return 0;
  }
  else if (m_romWidth == 192 && frameWidth == 128)
  {
    (*pXOffset) = 16;
    return 1;
  }
  else if (m_romHeight == 16 && frameHeight == 32)
  {
    (*pYOffset) = 8;
    return 0;
  }
  else if (m_romHeight == 16 && frameHeight == 64)
  {
    if (m_upscaling)
    {
      (*pYOffset) = 16;
      return 2;
    }
    (*pXOffset) = 64;
    (*pYOffset) = 24;
    return 0;
  }
  else if (m_romWidth == 256 && frameWidth == 128)
  {
    return 1;
  }
  else if (m_romWidth == 128 && frameWidth == 256)
  {
    if (m_upscaling)
    {
      return 2;
    }
    (*pXOffset) = 64;
    (*pYOffset) = 16;
    return 0;
  }

  return 255;
}

int ZeDMD::Scale888(uint8_t* pScaledFrame, uint8_t* pFrame, uint8_t bytes)
{
  uint8_t bits = bytes * 8;
  uint8_t xoffset = 0;
  uint8_t yoffset = 0;
  uint16_t frameWidth = GetWidth();
  uint16_t frameHeight = GetHeight();
  int bufferSize = m_romWidth * m_romHeight * bytes;
  uint8_t scale = GetScaleMode(frameWidth, frameHeight, &xoffset, &yoffset);

  if (scale == 255)
  {
    memcpy(pScaledFrame, pFrame, bufferSize);
    return bufferSize;
  }

  bufferSize = frameWidth * frameHeight * bytes;
  memset(pScaledFrame, 0, bufferSize);

  if (scale == 1)
  {
    FrameUtil::Helper::ScaleDown(pScaledFrame, frameWidth, frameHeight, pFrame, m_romWidth, m_romHeight, bits);
  }
  else if (scale == 2)
  {
    FrameUtil::Helper::ScaleUp(pScaledFrame, pFrame, m_romWidth, m_romHeight, bits);
    if (frameWidth > (m_romWidth * 2) || frameHeight > (m_romHeight * 2))
    {
      uint8_t* pUncenteredFrame = (uint8_t*)malloc(bufferSize);
      memcpy(pUncenteredFrame, pScaledFrame, bufferSize);
      FrameUtil::Helper::Center(pScaledFrame, frameWidth, frameHeight, pUncenteredFrame, m_romWidth * 2,
                                m_romHeight * 2, bits);
      free(pUncenteredFrame);
    }
  }
  else
  {
    FrameUtil::Helper::Center(pScaledFrame, frameWidth, frameHeight, pFrame, m_romWidth, m_romHeight, bits);
  }

  return bufferSize;
}

int ZeDMD::Scale565(uint8_t* pScaledFrame, uint16_t* pFrame, bool bigEndian)
{
  int bufferSize = m_romWidth * m_romHeight;
  uint8_t* pConvertedFrame = (uint8_t*)malloc(bufferSize * 2);
  for (int i = 0; i < bufferSize; i++)
  {
    pConvertedFrame[i * 2 + !bigEndian] = pFrame[i] >> 8;
    pConvertedFrame[i * 2 + bigEndian] = pFrame[i] & 0xFF;
  }

  bufferSize = Scale888(pScaledFrame, pConvertedFrame, 2);
  free(pConvertedFrame);

  return bufferSize;
}

ZEDMDAPI ZeDMD* ZeDMD_GetInstance() { return new ZeDMD(); }

ZEDMDAPI const char* ZeDMD_GetVersion() { return ZEDMD_VERSION; };

ZEDMDAPI void ZeDMD_SetLogCallback(ZeDMD* pZeDMD, ZeDMD_LogCallback callback, const void* pUserData)
{
  pZeDMD->SetLogCallback(callback, pUserData);
}

ZEDMDAPI const char* ZeDMD_FormatLogMessage(const char* format, va_list args, const void* pUserData)
{
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);

  return buffer;
}

ZEDMDAPI const char* ZeDMD_GetFirmwareVersion(ZeDMD* pZeDMD) { return pZeDMD->GetFirmwareVersion(); };

ZEDMDAPI uint16_t ZeDMD_GetId(ZeDMD* pZeDMD) { return pZeDMD->GetId(); };

ZEDMDAPI const char* ZeDMD_GetIdString(ZeDMD* pZeDMD) { return pZeDMD->GetIdString(); };

ZEDMDAPI const char* ZeDMD_GetIp(ZeDMD* pZeDMD) { return pZeDMD->GetIp(); };

ZEDMDAPI const char* ZeDMD_GetDevice(ZeDMD* pZeDMD) { return pZeDMD->GetDevice(); };

ZEDMDAPI bool ZeDMD_IsS3(ZeDMD* pZeDMD) { return pZeDMD->IsS3(); };

ZEDMDAPI uint8_t ZeDMD_GetRGBOrder(ZeDMD* pZeDMD) { return pZeDMD->GetRGBOrder(); };

ZEDMDAPI uint16_t ZeDMD_GetWidth(ZeDMD* pZeDMD) { return pZeDMD->GetWidth(); };

ZEDMDAPI uint16_t ZeDMD_GetHeight(ZeDMD* pZeDMD) { return pZeDMD->GetHeight(); };

ZEDMDAPI uint8_t ZeDMD_GetBrightness(ZeDMD* pZeDMD) { return pZeDMD->GetBrightness(); };

ZEDMDAPI const char* ZeDMD_GetWiFiSSID(ZeDMD* pZeDMD) { return pZeDMD->GetWiFiSSID(); };

ZEDMDAPI void ZeDMD_StoreWiFiPassword(ZeDMD* pZeDMD) { pZeDMD->StoreWiFiPassword(); };

ZEDMDAPI int ZeDMD_GetWiFiPort(ZeDMD* pZeDMD) { return pZeDMD->GetWiFiPort(); };

ZEDMDAPI uint8_t ZeDMD_GetWiFiPower(ZeDMD* pZeDMD) { return pZeDMD->GetWiFiPower(); };

ZEDMDAPI uint16_t ZeDMD_GetPanelWidth(ZeDMD* pZeDMD) { return pZeDMD->GetPanelWidth(); };

ZEDMDAPI uint16_t ZeDMD_GetPanelHeight(ZeDMD* pZeDMD) { return pZeDMD->GetPanelHeight(); };

ZEDMDAPI uint8_t ZeDMD_GetPanelClockPhase(ZeDMD* pZeDMD) { return pZeDMD->GetPanelClockPhase(); };

ZEDMDAPI uint8_t ZeDMD_GetPanelI2sSpeed(ZeDMD* pZeDMD) { return pZeDMD->GetPanelI2sSpeed(); };

ZEDMDAPI uint8_t ZeDMD_GetPanelLatchBlanking(ZeDMD* pZeDMD) { return pZeDMD->GetPanelLatchBlanking(); };

ZEDMDAPI uint8_t ZeDMD_GetPanelMinRefreshRate(ZeDMD* pZeDMD) { return pZeDMD->GetPanelMinRefreshRate(); };

ZEDMDAPI uint8_t ZeDMD_GetPanelDriver(ZeDMD* pZeDMD) { return pZeDMD->GetPanelDriver(); };

ZEDMDAPI uint8_t ZeDMD_GetTransport(ZeDMD* pZeDMD) { return pZeDMD->GetTransport(); };

ZEDMDAPI uint8_t ZeDMD_GetUdpDelay(ZeDMD* pZeDMD) { return pZeDMD->GetUdpDelay(); };

ZEDMDAPI uint16_t ZeDMD_GetUsbPackageSize(ZeDMD* pZeDMD) { return pZeDMD->GetUsbPackageSize(); };

ZEDMDAPI uint8_t ZeDMD_GetYOffset(ZeDMD* pZeDMD) { return pZeDMD->GetYOffset(); };

ZEDMDAPI void ZeDMD_IgnoreDevice(ZeDMD* pZeDMD, const char* const ignore_device)
{
  pZeDMD->IgnoreDevice(ignore_device);
}

ZEDMDAPI void ZeDMD_SetDevice(ZeDMD* pZeDMD, const char* const device) { pZeDMD->SetDevice(device); }

ZEDMDAPI bool ZeDMD_Open(ZeDMD* pZeDMD) { return pZeDMD->Open(); }

ZEDMDAPI bool ZeDMD_OpenWiFi(ZeDMD* pZeDMD, const char* ip) { return pZeDMD->OpenWiFi(ip); }

ZEDMDAPI bool ZeDMD_OpenDefaultWiFi(ZeDMD* pZeDMD) { return pZeDMD->OpenDefaultWiFi(); }

ZEDMDAPI void ZeDMD_Close(ZeDMD* pZeDMD) { pZeDMD->Close(); }

ZEDMDAPI void ZeDMD_Reset(ZeDMD* pZeDMD) { pZeDMD->Reset(); }

ZEDMDAPI void ZeDMD_RebootToBootloader(ZeDMD* pZeDMD) { pZeDMD->RebootToBootloader(); }

ZEDMDAPI void ZeDMD_SetFrameSize(ZeDMD* pZeDMD, uint16_t width, uint16_t height)
{
  return pZeDMD->SetFrameSize(width, height);
}

ZEDMDAPI void ZeDMD_LedTest(ZeDMD* pZeDMD) { pZeDMD->LedTest(); }

ZEDMDAPI void ZeDMD_EnableDebug(ZeDMD* pZeDMD) { pZeDMD->EnableDebug(); }

ZEDMDAPI void ZeDMD_DisableDebug(ZeDMD* pZeDMD) { pZeDMD->DisableDebug(); }

ZEDMDAPI void ZeDMD_EnableVerbose(ZeDMD* pZeDMD) { pZeDMD->EnableVerbose(); }

ZEDMDAPI void ZeDMD_DisableVerbose(ZeDMD* pZeDMD) { pZeDMD->DisableVerbose(); }

ZEDMDAPI void ZeDMD_SetRGBOrder(ZeDMD* pZeDMD, uint8_t rgbOrder) { pZeDMD->SetRGBOrder(rgbOrder); }

ZEDMDAPI void ZeDMD_SetBrightness(ZeDMD* pZeDMD, uint8_t brightness) { pZeDMD->SetBrightness(brightness); }

ZEDMDAPI void ZeDMD_SaveSettings(ZeDMD* pZeDMD) { pZeDMD->SaveSettings(); }

ZEDMDAPI void ZeDMD_EnableUpscaling(ZeDMD* pZeDMD) { pZeDMD->EnableUpscaling(); }

ZEDMDAPI void ZeDMD_DisableUpscaling(ZeDMD* pZeDMD) { pZeDMD->DisableUpscaling(); }

ZEDMDAPI void ZeDMD_SetWiFiSSID(ZeDMD* pZeDMD, const char* const ssid) { pZeDMD->SetWiFiSSID(ssid); }

ZEDMDAPI void ZeDMD_SetWiFiPassword(ZeDMD* pZeDMD, const char* const password) { pZeDMD->SetWiFiPassword(password); }

ZEDMDAPI void ZeDMD_SetPanelClockPhase(ZeDMD* pZeDMD, uint8_t clockPhase) { pZeDMD->SetPanelClockPhase(clockPhase); }

ZEDMDAPI void ZeDMD_SetPanelI2sSpeed(ZeDMD* pZeDMD, uint8_t i2sSpeed) { pZeDMD->SetPanelI2sSpeed(i2sSpeed); }

ZEDMDAPI void ZeDMD_SetPanelLatchBlanking(ZeDMD* pZeDMD, uint8_t latchBlanking)
{
  pZeDMD->SetPanelLatchBlanking(latchBlanking);
}

ZEDMDAPI void ZeDMD_SetPanelMinRefreshRate(ZeDMD* pZeDMD, uint8_t minRefreshRate)
{
  pZeDMD->SetPanelMinRefreshRate(minRefreshRate);
}

ZEDMDAPI void ZeDMD_SetPanelDriver(ZeDMD* pZeDMD, uint8_t driver) { pZeDMD->SetPanelDriver(driver); }

ZEDMDAPI void ZeDMD_SetTransport(ZeDMD* pZeDMD, uint8_t transport) { pZeDMD->SetTransport(transport); }

ZEDMDAPI void ZeDMD_SetUdpDelay(ZeDMD* pZeDMD, uint8_t udpDelay) { pZeDMD->SetUdpDelay(udpDelay); }

ZEDMDAPI void ZeDMD_SetUsbPackageSize(ZeDMD* pZeDMD, uint16_t usbPackageSize)
{
  pZeDMD->SetUsbPackageSize(usbPackageSize);
}

ZEDMDAPI void ZeDMD_SetYOffset(ZeDMD* pZeDMD, uint8_t yOffset) { pZeDMD->SetYOffset(yOffset); }

ZEDMDAPI void ZeDMD_SetWiFiPort(ZeDMD* pZeDMD, int port) { pZeDMD->SetWiFiPort(port); }

ZEDMDAPI void ZeDMD_SetWiFiPower(ZeDMD* pZeDMD, uint8_t power) { pZeDMD->SetWiFiPower(power); }

ZEDMDAPI void ZeDMD_ClearScreen(ZeDMD* pZeDMD) { pZeDMD->ClearScreen(); }

ZEDMDAPI void ZeDMD_EnableTrueRgb888(ZeDMD* pZeDMD, bool enable) { pZeDMD->EnableTrueRgb888(enable); }

ZEDMDAPI void ZeDMD_RenderRgb888(ZeDMD* pZeDMD, uint8_t* frame) { pZeDMD->RenderRgb888(frame); }

ZEDMDAPI void ZeDMD_RenderRgb565(ZeDMD* pZeDMD, uint16_t* frame) { pZeDMD->RenderRgb565(frame); }
