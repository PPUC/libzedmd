#include "ZeDMD.h"

#include <cstring>

#include "FrameUtil.h"
#include "ZeDMDComm.h"
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
}

ZeDMD::~ZeDMD()
{
  delete m_pZeDMDComm;
  delete m_pZeDMDWiFi;

  if (m_pFrameBuffer)
  {
    delete m_pFrameBuffer;
  }

  if (m_pScaledFrameBuffer)
  {
    delete m_pScaledFrameBuffer;
  }

  if (m_pRgb565Buffer)
  {
    delete m_pRgb565Buffer;
  }
}

void ZeDMD::SetLogCallback(ZeDMD_LogCallback callback, const void* userData)
{
  m_pZeDMDComm->SetLogCallback(callback, userData);
  m_pZeDMDWiFi->SetLogCallback(callback, userData);
}

void ZeDMD::Close()
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
    //m_pZeDMDComm->SoftReset();
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
  }

  m_pZeDMDComm->Disconnect();
  m_pZeDMDWiFi->Disconnect();
}

void ZeDMD::Reset()
{
  if (m_usb)
  {
    m_pZeDMDComm->SoftReset();
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->SoftReset();
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
  if (m_wifi)
  {
    return m_pZeDMDWiFi->GetWidth();
  }
  return m_pZeDMDComm->GetWidth();
}

uint16_t const ZeDMD::GetHeight()
{
  if (m_wifi)
  {
    return m_pZeDMDWiFi->GetHeight();
  }
  return m_pZeDMDComm->GetHeight();
}

bool const ZeDMD::IsS3()
{
  if (m_wifi)
  {
    return m_pZeDMDWiFi->IsS3();
  }
  return m_pZeDMDComm->IsS3();
}

const char* ZeDMD::GetVersion() { return ZEDMD_VERSION; }

const char* ZeDMD::GetFirmwareVersion()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetFirmwareVersion();
  }
  return m_pZeDMDWiFi->GetFirmwareVersion();
}

const char* ZeDMD::GetWiFiSSID()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetWiFiSSID();
  }
  return m_pZeDMDWiFi->GetWiFiSSID();
}

int ZeDMD::GetWiFiPort()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetWiFiPort();
  }
  return m_pZeDMDWiFi->GetWiFiPort();
}

void ZeDMD::StoreWiFiPassword()
{
  if (m_usb)
  {
    return m_pZeDMDComm->StoreWiFiPassword();
  }
  return m_pZeDMDWiFi->StoreWiFiPassword();
}

uint8_t ZeDMD::GetRGBOrder()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetRGBOrder();
  }
  return m_pZeDMDWiFi->GetRGBOrder();
}

uint8_t ZeDMD::GetBrightness()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetBrightness();
  }
  return m_pZeDMDWiFi->GetBrightness();
}

uint8_t ZeDMD::GetPanelClockPhase()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetPanelClockPhase();
  }
  return m_pZeDMDWiFi->GetPanelClockPhase();
}

uint8_t ZeDMD::GetPanelI2sSpeed()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetPanelI2sSpeed();
  }
  return m_pZeDMDWiFi->GetPanelI2sSpeed();
}

uint8_t ZeDMD::GetPanelLatchBlanking()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetPanelLatchBlanking();
  }
  return m_pZeDMDWiFi->GetPanelLatchBlanking();
}

uint8_t ZeDMD::GetPanelMinRefreshRate()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetPanelMinRefreshRate();
  }
  return m_pZeDMDWiFi->GetPanelMinRefreshRate();
}

uint8_t ZeDMD::GetPanelDriver()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetPanelDriver();
  }
  return m_pZeDMDWiFi->GetPanelDriver();
}

uint8_t ZeDMD::GetTransport()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetTransport();
  }
  return m_pZeDMDWiFi->GetTransport();
}

uint8_t ZeDMD::GetUdpDelay()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetUdpDelay();
  }
  return m_pZeDMDWiFi->GetUdpDelay();
}

uint16_t ZeDMD::GetUsbPackageSize()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetUsbPackageSize();
  }
  return m_pZeDMDWiFi->GetUsbPackageSize();
}

uint8_t ZeDMD::GetYOffset()
{
  if (m_usb)
  {
    return m_pZeDMDComm->GetYOffset();
  }
  return m_pZeDMDWiFi->GetYOffset();
}

void ZeDMD::LedTest()
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::LEDTest);
    m_pZeDMDComm->DisableKeepAlive();
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::LEDTest);
    m_pZeDMDWiFi->DisableKeepAlive();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(8000));

  if (m_usb)
  {
    m_pZeDMDComm->EnableKeepAlive();
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->EnableKeepAlive();
  }
}

void ZeDMD::EnableDebug()
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::EnableDebug);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::EnableDebug);
  }
}

void ZeDMD::DisableDebug()
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::DisableDebug);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::DisableDebug);
  }
}

void ZeDMD::SetRGBOrder(uint8_t rgbOrder)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGBOrder, rgbOrder);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::RGBOrder, rgbOrder);
  }
}

void ZeDMD::SetBrightness(uint8_t brightness)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::Brightness, brightness);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::Brightness, brightness);
  }
}

void ZeDMD::SetPanelClockPhase(uint8_t clockPhase)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetClkphase, clockPhase);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetClkphase, clockPhase);
  }
}

void ZeDMD::SetPanelI2sSpeed(uint8_t i2sSpeed)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetI2sspeed, i2sSpeed);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetI2sspeed, i2sSpeed);
  }
}

void ZeDMD::SetPanelLatchBlanking(uint8_t latchBlanking)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetLatchBlanking, latchBlanking);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetLatchBlanking, latchBlanking);
  }
}

void ZeDMD::SetPanelMinRefreshRate(uint8_t minRefreshRate)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetMinRefreshRate, minRefreshRate);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetMinRefreshRate, minRefreshRate);
  }
}

void ZeDMD::SetPanelDriver(uint8_t driver)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetDriver, driver);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetDriver, driver);
  }
}

void ZeDMD::SetTransport(uint8_t transport)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetTransport, transport);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetTransport, transport);
  }
}

void ZeDMD::SetUdpDelay(uint8_t udpDelay)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetUdpDelay, udpDelay);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetUdpDelay, udpDelay);
  }
}

void ZeDMD::SetUsbPackageSize(uint16_t usbPackageSize)
{
  uint8_t multiplier = (uint8_t)(usbPackageSize / 32);
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetUsbPackageSizeMultiplier, multiplier);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetUsbPackageSizeMultiplier, multiplier);
  }
}

void ZeDMD::SetYOffset(uint8_t yOffset)
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetYOffset, yOffset);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetYOffset, yOffset);
  }
}

void ZeDMD::SaveSettings()
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SaveSettings);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SaveSettings);
  }
  // Avoid that client resets the device before settings are saved.
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

void ZeDMD::EnableUpscaling()
{
  m_upscaling = true;
  m_hd = (GetWidth() == 256);
}

void ZeDMD::DisableUpscaling() { m_upscaling = false; }

void ZeDMD::SetWiFiSSID(const char* const ssid)
{
  int size = strlen(ssid);
  if (size <= 32)
  {
    uint8_t data[32] = {0};
    memcpy(data, (uint8_t*)ssid, size);
    if (m_usb)
    {
      m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiSSID, data, size);
    }
    else if (m_wifi)
    {
      m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiSSID, data, size);
    }
  }
}

void ZeDMD::SetWiFiPassword(const char* const password)
{
  int size = strlen(password);
  if (size <= 32)
  {
    uint8_t data[32] = {0};
    memcpy(data, (uint8_t*)password, size);
    if (m_usb)
    {
      m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPassword, data, size);
    }
    else if (m_wifi)
    {
      m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPassword, data, size);
    }
  }
}

void ZeDMD::SetWiFiPort(int port)
{
  uint8_t data[2];
  data[0] = (uint8_t)(port >> 8 & 0xFF);
  data[1] = (uint8_t)(port & 0xFF);
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPort, data, 2);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPort, data, 2);
  }
}

bool ZeDMD::OpenWiFi(const char* ip)
{
  m_wifi = m_pZeDMDWiFi->Connect(ip);

  if (m_wifi && !m_usb)
  {
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

  if (m_usb && !m_wifi)
  {
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

void ZeDMD::ClearScreen()
{
  if (m_usb)
  {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
  }
  else if (m_wifi)
  {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
  }
  // "Blank" the frame buffer.
  memset(m_pFrameBuffer, 0, ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
}

void ZeDMD::RenderRgb888(uint8_t* pFrame)
{
  if (!(m_usb || m_wifi) || !UpdateFrameBuffer888(pFrame))
  {
    return;
  }

  int bufferSize = Scale888(m_pScaledFrameBuffer, m_pFrameBuffer, 3);
  int rgb565Size = bufferSize / 3;
  for (uint16_t i = 0; i < rgb565Size; i++)
  {
    uint16_t tmp = (((uint16_t)(m_pScaledFrameBuffer[i * 3] & 0xF8)) << 8) |
                   (((uint16_t)(m_pScaledFrameBuffer[i * 3 + 1] & 0xFC)) << 3) | (m_pScaledFrameBuffer[i * 3 + 2] >> 3);
    m_pRgb565Buffer[i * 2 + 1] = tmp >> 8;
    m_pRgb565Buffer[i * 2] = tmp & 0xFF;
  }

  if (m_wifi)
  {
    m_pZeDMDWiFi->QueueFrame(m_pRgb565Buffer, rgb565Size * 2);
  }
  else if (m_usb)
  {
    m_pZeDMDComm->QueueFrame(m_pRgb565Buffer, rgb565Size * 2);
  }
}

void ZeDMD::RenderRgb565(uint16_t* pFrame)
{
  if (!(m_usb || m_wifi) || !UpdateFrameBuffer565(pFrame))
  {
    return;
  }

  int size = Scale565(m_pScaledFrameBuffer, pFrame, is_bigendian());

  if (m_wifi)
  {
    m_pZeDMDWiFi->QueueFrame(m_pScaledFrameBuffer, size);
  }
  else if (m_usb)
  {
    m_pZeDMDComm->QueueFrame(m_pScaledFrameBuffer, size);
  }
}
bool ZeDMD::UpdateFrameBuffer888(uint8_t* pFrame)
{
  if (0 == memcmp(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 3))
  {
    return false;
  }

  memcpy(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 3);
  return true;
}

bool ZeDMD::UpdateFrameBuffer565(uint16_t* pFrame)
{
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

ZEDMDAPI const char* ZeDMD_GetFirmwareVersion(ZeDMD* pZeDMD) { return pZeDMD->GetFirmwareVersion(); };

ZEDMDAPI uint8_t ZeDMD_GetRGBOrder(ZeDMD* pZeDMD) { return pZeDMD->GetRGBOrder(); };

ZEDMDAPI uint8_t ZeDMD_GetWidth(ZeDMD* pZeDMD) { return pZeDMD->GetWidth(); };

ZEDMDAPI uint8_t ZeDMD_GetHeight(ZeDMD* pZeDMD) { return pZeDMD->GetHeight(); };

ZEDMDAPI uint8_t ZeDMD_GetBrightness(ZeDMD* pZeDMD) { return pZeDMD->GetBrightness(); };

ZEDMDAPI const char* ZeDMD_GetWiFiSSID(ZeDMD* pZeDMD) { return pZeDMD->GetWiFiSSID(); };

ZEDMDAPI void ZeDMD_StoreWiFiPassword(ZeDMD* pZeDMD) { return pZeDMD->StoreWiFiPassword(); };

ZEDMDAPI int ZeDMD_GetWiFiPort(ZeDMD* pZeDMD) { return pZeDMD->GetWiFiPort(); };

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
  return pZeDMD->IgnoreDevice(ignore_device);
}

ZEDMDAPI void ZeDMD_SetDevice(ZeDMD* pZeDMD, const char* const device) { return pZeDMD->SetDevice(device); }

ZEDMDAPI bool ZeDMD_Open(ZeDMD* pZeDMD) { return pZeDMD->Open(); }

ZEDMDAPI bool ZeDMD_OpenWiFi(ZeDMD* pZeDMD, const char* ip) { return pZeDMD->OpenWiFi(ip); }

ZEDMDAPI bool ZeDMD_OpenDefaultWiFi(ZeDMD* pZeDMD) { return pZeDMD->OpenDefaultWiFi(); }

ZEDMDAPI void ZeDMD_Close(ZeDMD* pZeDMD) { return pZeDMD->Close(); }

ZEDMDAPI void ZeDMD_SetFrameSize(ZeDMD* pZeDMD, uint16_t width, uint16_t height)
{
  return pZeDMD->SetFrameSize(width, height);
}

ZEDMDAPI void ZeDMD_LedTest(ZeDMD* pZeDMD) { return pZeDMD->LedTest(); }

ZEDMDAPI void ZeDMD_EnableDebug(ZeDMD* pZeDMD) { return pZeDMD->EnableDebug(); }

ZEDMDAPI void ZeDMD_DisableDebug(ZeDMD* pZeDMD) { return pZeDMD->DisableDebug(); }

ZEDMDAPI void ZeDMD_SetRGBOrder(ZeDMD* pZeDMD, uint8_t rgbOrder) { return pZeDMD->SetRGBOrder(rgbOrder); }

ZEDMDAPI void ZeDMD_SetBrightness(ZeDMD* pZeDMD, uint8_t brightness) { return pZeDMD->SetBrightness(brightness); }

ZEDMDAPI void ZeDMD_SaveSettings(ZeDMD* pZeDMD) { return pZeDMD->SaveSettings(); }

ZEDMDAPI void ZeDMD_EnableUpscaling(ZeDMD* pZeDMD) { return pZeDMD->EnableUpscaling(); }

ZEDMDAPI void ZeDMD_DisableUpscaling(ZeDMD* pZeDMD) { return pZeDMD->DisableUpscaling(); }

ZEDMDAPI void ZeDMD_SetWiFiSSID(ZeDMD* pZeDMD, const char* const ssid) { return pZeDMD->SetWiFiSSID(ssid); }

ZEDMDAPI void ZeDMD_SetWiFiPassword(ZeDMD* pZeDMD, const char* const password)
{
  return pZeDMD->SetWiFiPassword(password);
}

ZEDMDAPI void ZeDMD_SetPanelClockPhase(ZeDMD* pZeDMD, uint8_t clockPhase)
{
  return pZeDMD->SetPanelClockPhase(clockPhase);
}

ZEDMDAPI void ZeDMD_SetPanelI2sSpeed(ZeDMD* pZeDMD, uint8_t i2sSpeed) { return pZeDMD->SetPanelI2sSpeed(i2sSpeed); }

ZEDMDAPI void ZeDMD_SetPanelLatchBlanking(ZeDMD* pZeDMD, uint8_t latchBlanking)
{
  return pZeDMD->SetPanelLatchBlanking(latchBlanking);
}

ZEDMDAPI void ZeDMD_SetPanelMinRefreshRate(ZeDMD* pZeDMD, uint8_t minRefreshRate)
{
  return pZeDMD->SetPanelMinRefreshRate(minRefreshRate);
}

ZEDMDAPI void ZeDMD_SetPanelDriver(ZeDMD* pZeDMD, uint8_t driver) { return pZeDMD->SetPanelDriver(driver); }

ZEDMDAPI void ZeDMD_SetTransport(ZeDMD* pZeDMD, uint8_t transport) { return pZeDMD->SetTransport(transport); }

ZEDMDAPI void ZeDMD_SetUdpDelay(ZeDMD* pZeDMD, uint8_t udpDelay) { return pZeDMD->SetUdpDelay(udpDelay); }

ZEDMDAPI void ZeDMD_SetUsbPackageSize(ZeDMD* pZeDMD, uint16_t usbPackageSize)
{
  return pZeDMD->SetUsbPackageSize(usbPackageSize);
}

ZEDMDAPI void ZeDMD_SetYOffset(ZeDMD* pZeDMD, uint8_t yOffset) { return pZeDMD->SetYOffset(yOffset); }

ZEDMDAPI void ZeDMD_SetWiFiPort(ZeDMD* pZeDMD, int port) { return pZeDMD->SetWiFiPort(port); }

ZEDMDAPI void ZeDMD_ClearScreen(ZeDMD* pZeDMD) { return pZeDMD->ClearScreen(); }

ZEDMDAPI void ZeDMD_RenderRgb888(ZeDMD* pZeDMD, uint8_t* frame) { return pZeDMD->RenderRgb888(frame); }

ZEDMDAPI void ZeDMD_RenderRgb565(ZeDMD* pZeDMD, uint16_t* frame) { return pZeDMD->RenderRgb565(frame); }
