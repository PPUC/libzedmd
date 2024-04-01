#include "ZeDMD.h"

#include <cstring>

#include "FrameUtil.h"
#include "ZeDMDComm.h"
#include "ZeDMDWiFi.h"

const int endian_check = 1;
#define is_bigendian() ((*(char*)&endian_check) == 0)

ZeDMD::ZeDMD() {
  m_romWidth = 0;
  m_romHeight = 0;

  memset(m_palette4, 0, sizeof(m_palette4));
  memset(m_palette16, 0, sizeof(m_palette16));
  memset(m_palette64, 0, sizeof(m_palette64));

  m_pFrameBuffer = nullptr;
  m_pScaledFrameBuffer = nullptr;
  m_pCommandBuffer = nullptr;
  m_pPlanes = nullptr;
  m_pRgb565Buffer = nullptr;

  m_pZeDMDComm = new ZeDMDComm();
  m_pZeDMDWiFi = new ZeDMDWiFi();
}

ZeDMD::~ZeDMD() {
  delete m_pZeDMDComm;
  delete m_pZeDMDWiFi;

  if (m_pFrameBuffer) {
    delete m_pFrameBuffer;
  }

  if (m_pScaledFrameBuffer) {
    delete m_pScaledFrameBuffer;
  }

  if (m_pCommandBuffer) {
    delete m_pCommandBuffer;
  }

  if (m_pPlanes) {
    delete m_pPlanes;
  }

  if (m_pRgb565Buffer) {
    delete m_pRgb565Buffer;
  }
}

void ZeDMD::SetLogCallback(ZeDMD_LogCallback callback, const void* userData) {
  m_pZeDMDComm->SetLogCallback(callback, userData);
  m_pZeDMDWiFi->SetLogCallback(callback, userData);
}

void ZeDMD::Close() {
  m_pZeDMDComm->Disconnect();
  m_pZeDMDWiFi->Disconnect();
}

void ZeDMD::Reset() { m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::Reset); }

void ZeDMD::IgnoreDevice(const char* const ignore_device) {
  m_pZeDMDComm->IgnoreDevice(ignore_device);
}

void ZeDMD::SetDevice(const char* const device) {
  m_pZeDMDComm->SetDevice(device);
}

void ZeDMD::SetFrameSize(uint16_t width, uint16_t height) {
  m_romWidth = width;
  m_romHeight = height;

  if (m_usb) {
    uint16_t frameWidth = m_pZeDMDComm->GetWidth();
    uint16_t frameHeight = m_pZeDMDComm->GetHeight();
    uint8_t size[4];

    if ((m_downscaling && (width > frameWidth || height > frameHeight)) ||
        (m_upscaling && (width < frameWidth || height < frameHeight))) {
      size[0] = (uint8_t)(frameWidth & 0xFF);
      size[1] = (uint8_t)((frameWidth >> 8) & 0xFF);
      size[2] = (uint8_t)(frameHeight & 0xFF);
      size[3] = (uint8_t)((frameHeight >> 8) & 0xFF);
    } else {
      size[0] = (uint8_t)(width & 0xFF);
      size[1] = (uint8_t)((width >> 8) & 0xFF);
      size[2] = (uint8_t)(height & 0xFF);
      size[3] = (uint8_t)((height >> 8) & 0xFF);
    }

    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::FrameSize, size, 4);
  }
}

uint16_t const ZeDMD::GetWidth() { return m_pZeDMDComm->GetWidth(); }

uint16_t const ZeDMD::GetHeight() { return m_pZeDMDComm->GetHeight(); }

bool const ZeDMD::IsS3() { return m_pZeDMDComm->IsS3(); }

void ZeDMD::LedTest() {
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::LEDTest);
}

void ZeDMD::EnableDebug() {
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::EnableDebug);
}

void ZeDMD::DisableDebug() {
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::DisableDebug);
}

void ZeDMD::SetRGBOrder(uint8_t rgbOrder) {
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGBOrder, rgbOrder);
}

void ZeDMD::SetBrightness(uint8_t brightness) {
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::Brightness, brightness);
}

void ZeDMD::SaveSettings() {
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SaveSettings);
  // Avoid that client resets the device before settings are saved.
  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}

void ZeDMD::EnablePreDownscaling() { m_downscaling = true; }

void ZeDMD::DisablePreDownscaling() { m_downscaling = false; }

void ZeDMD::EnablePreUpscaling() {
  m_upscaling = true;
  m_hd = (m_pZeDMDComm->GetWidth() == 256);
}

void ZeDMD::DisablePreUpscaling() { m_upscaling = false; }

void ZeDMD::EnableUpscaling() {
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::EnableUpscaling);
}

void ZeDMD::DisableUpscaling() {
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::DisableUpscaling);
}

void ZeDMD::SetWiFiSSID(const char* const ssid) {
  int size = strlen(ssid);
  uint8_t data[33] = {0};
  data[0] = (uint8_t)size;
  memcpy(&data[1], (uint8_t*)ssid, size);
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiSSID, data, size + 1);
}

void ZeDMD::SetWiFiPassword(const char* const password) {
  int size = strlen(password);
  uint8_t data[33] = {0};
  data[0] = (uint8_t)size;
  memcpy(&data[1], (uint8_t*)password, size);
  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPassword, data,
                             size + 1);
}

void ZeDMD::SetWiFiPort(int port) {
  uint8_t data[2];
  data[0] = (uint8_t)(port >> 8 & 0xFF);
  data[1] = (uint8_t)(port & 0xFF);

  m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::SetWiFiPort, data, 2);
}

bool ZeDMD::OpenWiFi(const char* ip, int port) {
  m_wifi = m_pZeDMDWiFi->Connect(ip, port);

  // @todo allow parallel mode for USB commands
  if (m_wifi && !m_usb) {
    m_pFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pScaledFrameBuffer =
        (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pPlanes = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);

    m_pZeDMDWiFi->Run();
  }

  return m_wifi;
}

bool ZeDMD::Open() {
  m_usb = m_pZeDMDComm->Connect();

  // @todo allow parallel connection for USB commands
  if (m_usb && !m_wifi) {
    m_pFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pScaledFrameBuffer =
        (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pCommandBuffer =
        (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3 + 192);
    m_pPlanes = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
    m_pRgb565Buffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 2);

    m_hd = (m_pZeDMDComm->GetWidth() == 256);

    m_pZeDMDComm->Run();
  }

  return m_usb;
}

bool ZeDMD::Open(uint16_t width, uint16_t height) {
  if (Open()) {
    SetFrameSize(width, height);
  }

  return m_usb;
}

void ZeDMD::SetPalette(uint8_t* pPalette) { SetPalette(pPalette, 64); }

void ZeDMD::SetPalette(uint8_t* pPalette, uint8_t numColors) {
  m_paletteChanged = false;

  uint8_t* pPaletteNumber;
  switch (numColors) {
    case 4:
      pPaletteNumber = m_palette4;
      break;
    case 16:
      pPaletteNumber = m_palette16;
      break;
    case 64:
      pPaletteNumber = m_palette64;
      break;
    default:
      return;
  }

  if (memcmp(pPaletteNumber, pPalette, numColors * 3)) {
    memcpy(pPaletteNumber, pPalette, numColors * 3);
    m_paletteChanged = true;
  }
}

void ZeDMD::SetDefaultPalette(uint8_t bitDepth) {
  switch (bitDepth) {
    case 2:
      SetPalette(m_DmdDefaultPalette2Bit, 4);
      break;

    default:
      SetPalette(m_DmdDefaultPalette4Bit, 16);
  }
}

uint8_t* ZeDMD::GetDefaultPalette(uint8_t bitDepth) {
  switch (bitDepth) {
    case 2:
      return m_DmdDefaultPalette2Bit;
      break;

    default:
      return m_DmdDefaultPalette4Bit;
  }
}

void ZeDMD::EnforceStreaming() { m_streaming = true; }

void ZeDMD::DisableRGB24Streaming() { m_rgb24Streaming = false; }

void ZeDMD::ClearScreen() {
  if (m_usb) {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
  } else if (m_wifi) {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
  }
  // "Blank" the frame buffer.
  memset(m_pFrameBuffer, 0, ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
}

void ZeDMD::RenderGray2(uint8_t* pFrame) {
  if (!(m_usb || m_wifi) || !(UpdateFrameBuffer8(pFrame) || m_paletteChanged)) {
    return;
  }

  uint16_t width;
  uint16_t height;

  int bufferSize =
      Scale(m_pScaledFrameBuffer, m_pFrameBuffer, 1, &width, &height);

  if (m_hd || m_wifi || m_streaming) {
    FrameUtil::Helper::ConvertToRgb24(m_pPlanes, m_pScaledFrameBuffer,
                                      bufferSize, m_palette4);

    if (m_wifi) {
      m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream,
                                 m_pPlanes, bufferSize * 3, width, height);
    }
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize * 3, width, height);
  } else if (m_usb) {
    FrameUtil::Helper::Split(m_pPlanes, width, height, 2, m_pScaledFrameBuffer);

    bufferSize = bufferSize / 8 * 2;

    memcpy(m_pCommandBuffer, m_palette4, 12);
    memcpy(m_pCommandBuffer + 12, m_pPlanes, bufferSize);

    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::Gray2, m_pCommandBuffer,
                               12 + bufferSize);
  }
}

void ZeDMD::RenderGray4(uint8_t* pFrame) {
  if (!(m_usb || m_wifi) || !(UpdateFrameBuffer8(pFrame) || m_paletteChanged)) {
    return;
  }

  uint16_t width;
  uint16_t height;

  int bufferSize =
      Scale(m_pScaledFrameBuffer, m_pFrameBuffer, 1, &width, &height);

  if (m_hd || m_wifi || m_streaming) {
    FrameUtil::Helper::ConvertToRgb24(m_pPlanes, m_pScaledFrameBuffer,
                                      bufferSize, m_palette16);

    if (m_wifi) {
      m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream,
                                 m_pPlanes, bufferSize * 3, width, height);
    }
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize * 3, width, height);
  } else if (m_usb) {
    // Review: why?
    bufferSize /= 2;
    FrameUtil::Helper::Split(m_pPlanes, width, height, 4, m_pScaledFrameBuffer);

    memcpy(m_pCommandBuffer, m_palette16, 48);
    memcpy(m_pCommandBuffer + 48, m_pPlanes, bufferSize);

    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::ColGray4, m_pCommandBuffer,
                               48 + bufferSize);
  }
}

void ZeDMD::RenderColoredGray6(uint8_t* pFrame, uint8_t* pPalette,
                               uint8_t* pRotations) {
  SetPalette(pPalette);
  RenderColoredGray6(pFrame, pRotations);
}

void ZeDMD::RenderColoredGray6(uint8_t* pFrame, uint8_t* pRotations) {
  if (!(m_usb || m_wifi) || !(UpdateFrameBuffer8(pFrame) || m_paletteChanged)) {
    return;
  }

  uint16_t width;
  uint16_t height;

  int bufferSize =
      Scale(m_pScaledFrameBuffer, m_pFrameBuffer, 1, &width, &height);

  if (m_hd || m_wifi || m_streaming) {
    FrameUtil::Helper::ConvertToRgb24(m_pPlanes, m_pScaledFrameBuffer,
                                      bufferSize, m_palette64);

    if (m_wifi) {
      m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream,
                                 m_pPlanes, bufferSize * 3, width, height);
    }
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize * 3, width, height);
  } else if (m_usb) {
    FrameUtil::Helper::Split(m_pPlanes, width, height, 6, m_pScaledFrameBuffer);

    bufferSize = bufferSize / 8 * 6;

    memcpy(m_pCommandBuffer, m_palette64, 192);
    memcpy(m_pCommandBuffer + 192, m_pPlanes, bufferSize);

    if (pRotations) {
      memcpy(m_pCommandBuffer + 192 + bufferSize, pRotations, 24);
    } else {
      memset(m_pCommandBuffer + 192 + bufferSize, 255, 24);
    }

    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::ColGray6, m_pCommandBuffer,
                               192 + bufferSize + 24);
  }
}

void ZeDMD::RenderRgb24(uint8_t* pFrame) {
  if (!(m_usb || m_wifi) || !UpdateFrameBuffer24(pFrame)) {
    return;
  }

  uint16_t width;
  uint16_t height;

  int bufferSize = Scale(m_pPlanes, m_pFrameBuffer, 3, &width, &height);

  if (m_wifi) {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize, width, height);
  } else if (m_hd || m_rgb24Streaming || m_streaming) {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize, width, height);
  } else if (m_usb) {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24, m_pPlanes,
                               bufferSize);
  }
}

void ZeDMD::RenderRgb24EncodedAs565(uint8_t* pFrame) {
  if (!m_usb || !UpdateFrameBuffer24(pFrame)) {
    return;
  }

  uint16_t width;
  uint16_t height;

  int bufferSize = Scale(m_pPlanes, m_pFrameBuffer, 3, &width, &height);
  int rgb565Size = bufferSize / 3;
  for (uint16_t i = 0; i < rgb565Size; i++) {
    uint16_t tmp = (((uint16_t)(m_pPlanes[i * 3] & 0xF8)) << 8) |
                   (((uint16_t)(m_pPlanes[i * 3 + 1] & 0xFC)) << 3) |
                   (m_pPlanes[i * 3 + 2] >> 3);
    m_pRgb565Buffer[i * 2 + 1] = tmp >> 8;
    m_pRgb565Buffer[i * 2] = tmp & 0xFF;
  }

  if (m_usb) {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB565ZonesStream,
                               m_pRgb565Buffer, rgb565Size * 2, width, height,
                               2);
  }
}

void ZeDMD::RenderRgb565(uint16_t* pFrame) {
  if (!m_usb || !UpdateFrameBuffer565(pFrame)) {
    return;
  }

  uint16_t width;
  uint16_t height;

  int rgb565Size = Scale16(m_pPlanes, pFrame, &width, &height, is_bigendian());

  if (m_usb) {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB565ZonesStream, m_pPlanes,
                               rgb565Size, width, height, 2);
  }
}

bool ZeDMD::UpdateFrameBuffer8(uint8_t* pFrame) {
  if (!memcmp(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight)) {
    return false;
  }

  memcpy(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight);
  return true;
}

bool ZeDMD::UpdateFrameBuffer24(uint8_t* pFrame) {
  if (!memcmp(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 3)) {
    return false;
  }

  memcpy(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 3);
  return true;
}

bool ZeDMD::UpdateFrameBuffer565(uint16_t* pFrame) {
  if (!memcmp(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 2)) {
    return false;
  }

  memcpy(m_pFrameBuffer, pFrame, m_romWidth * m_romHeight * 2);
  return true;
}

uint8_t ZeDMD::GetScaleMode(uint16_t frameWidth, uint16_t frameHeight,
                            uint16_t* pWidth, uint16_t* pHeight,
                            uint8_t* pXOffset, uint8_t* pYOffset) {
  if (m_upscaling && m_romWidth == 192 && frameWidth == 256) {
    (*pWidth) = frameWidth;
    (*pHeight) = frameHeight;
    (*pXOffset) = 32;
    return 0;
  } else if (m_downscaling && m_romWidth == 192) {
    (*pWidth) = frameWidth;
    (*pHeight) = frameHeight;
    (*pXOffset) = 16;
    return 1;
  } else if (m_upscaling && m_romHeight == 16 && frameHeight == 32) {
    (*pWidth) = frameWidth;
    (*pHeight) = frameHeight;
    (*pYOffset) = 8;
  } else if (m_upscaling && m_romHeight == 16 && frameHeight == 64) {
    (*pWidth) = frameWidth;
    (*pHeight) = frameHeight;
    (*pYOffset) = 16;
    return 2;
  } else if (m_downscaling && m_romWidth == 256 && frameWidth == 128) {
    (*pWidth) = frameWidth;
    (*pHeight) = frameHeight;
    return 1;
  } else if (m_upscaling && m_romWidth == 128 && frameWidth == 256) {
    (*pWidth) = frameWidth;
    (*pHeight) = frameHeight;
    return 2;
  } else if (!m_upscaling && m_romWidth == 128 && frameWidth == 256) {
    (*pWidth) = frameWidth;
    (*pHeight) = frameHeight;
    (*pXOffset) = 64;
    (*pYOffset) = 16;
    return 0;
  }

  (*pWidth) = m_romWidth;
  (*pHeight) = m_romHeight;
  return 255;
}

int ZeDMD::Scale(uint8_t* pScaledFrame, uint8_t* pFrame, uint8_t bytes,
                 uint16_t* width, uint16_t* height) {
  uint8_t xoffset = 0;
  uint8_t yoffset = 0;
  uint16_t frameWidth = m_pZeDMDComm->GetWidth();
  uint16_t frameHeight = m_pZeDMDComm->GetHeight();
  int bufferSize = m_romWidth * m_romHeight * bytes;
  uint8_t scale =
      GetScaleMode(frameWidth, frameHeight, width, height, &xoffset, &yoffset);

  if (scale == 255) {
    memcpy(pScaledFrame, pFrame, bufferSize);
    return bufferSize;
  }

  bufferSize = frameWidth * frameHeight * bytes;
  memset(pScaledFrame, 0, bufferSize);

  if (scale == 1) {
    FrameUtil::Helper::ScaleDown(pScaledFrame, *width, *height, pFrame,
                                 m_romWidth, m_romHeight, bytes);
  } else if (scale == 2) {
    FrameUtil::Helper::ScaleUp(pScaledFrame, pFrame, m_romWidth, m_romHeight,
                               bytes);
    if (*width > (m_romWidth * 2) || *height > (m_romHeight * 2)) {
      uint8_t* pUncenteredFrame = (uint8_t*)malloc(bufferSize);
      memcpy(pUncenteredFrame, pScaledFrame, bufferSize);
      FrameUtil::Helper::Center(pScaledFrame, *width, *height, pUncenteredFrame,
                                m_romWidth * 2, m_romHeight * 2, bytes);
      free(pUncenteredFrame);
    }
  } else {
    FrameUtil::Helper::Center(pScaledFrame, *width, *height, pFrame, m_romWidth,
                              m_romHeight, bytes);
  }

  return bufferSize;
}

int ZeDMD::Scale16(uint8_t* pScaledFrame, uint16_t* pFrame, uint16_t* width,
                   uint16_t* height, bool bigEndian) {
  int bufferSize = m_romWidth * m_romHeight;
  uint8_t* pConvertedFrame = (uint8_t*)malloc(bufferSize * 2);
  for (int i = 0; i < bufferSize; i++) {
    pConvertedFrame[i * 2 + !bigEndian] = pFrame[i] >> 8;
    pConvertedFrame[i * 2 + bigEndian] = pFrame[i] & 0xFF;
  }

  return Scale(pScaledFrame, pConvertedFrame, 2, width, height);
}

ZEDMDAPI ZeDMD* ZeDMD_GetInstance() { return new ZeDMD(); }

ZEDMDAPI void ZeDMD_IgnoreDevice(ZeDMD* pZeDMD,
                                 const char* const ignore_device) {
  return pZeDMD->IgnoreDevice(ignore_device);
}

ZEDMDAPI void ZeDMD_SetDevice(ZeDMD* pZeDMD, const char* const device) {
  return pZeDMD->SetDevice(device);
}

ZEDMDAPI bool ZeDMD_Open(ZeDMD* pZeDMD) { return pZeDMD->Open(); }

ZEDMDAPI bool ZeDMD_OpenWiFi(ZeDMD* pZeDMD, const char* ip, int port) {
  return pZeDMD->OpenWiFi(ip, port);
}

ZEDMDAPI void ZeDMD_Close(ZeDMD* pZeDMD) { return pZeDMD->Close(); }

ZEDMDAPI void ZeDMD_SetFrameSize(ZeDMD* pZeDMD, uint16_t width,
                                 uint16_t height) {
  return pZeDMD->SetFrameSize(width, height);
}

ZEDMDAPI void ZeDMD_SetPalette(ZeDMD* pZeDMD, uint8_t* pPalette,
                               uint8_t numColors) {
  return pZeDMD->SetPalette(pPalette, numColors);
}

ZEDMDAPI void ZeDMD_SetDefaultPalette(ZeDMD* pZeDMD, uint8_t bitDepth) {
  return pZeDMD->SetDefaultPalette(bitDepth);
}

ZEDMDAPI uint8_t* ZeDMD_GetDefaultPalette(ZeDMD* pZeDMD, uint8_t bitDepth) {
  return pZeDMD->GetDefaultPalette(bitDepth);
}

ZEDMDAPI void ZeDMD_LedTest(ZeDMD* pZeDMD) { return pZeDMD->LedTest(); }

ZEDMDAPI void ZeDMD_EnableDebug(ZeDMD* pZeDMD) { return pZeDMD->EnableDebug(); }

ZEDMDAPI void ZeDMD_DisableDebug(ZeDMD* pZeDMD) {
  return pZeDMD->DisableDebug();
}

ZEDMDAPI void ZeDMD_SetRGBOrder(ZeDMD* pZeDMD, uint8_t rgbOrder) {
  return pZeDMD->SetRGBOrder(rgbOrder);
}

ZEDMDAPI void ZeDMD_SetBrightness(ZeDMD* pZeDMD, uint8_t brightness) {
  return pZeDMD->SetBrightness(brightness);
}

ZEDMDAPI void ZeDMD_SaveSettings(ZeDMD* pZeDMD) {
  return pZeDMD->SaveSettings();
}

ZEDMDAPI void ZeDMD_EnablePreDownscaling(ZeDMD* pZeDMD) {
  return pZeDMD->EnablePreDownscaling();
}

ZEDMDAPI void ZeDMD_DisablePreDownscaling(ZeDMD* pZeDMD) {
  return pZeDMD->DisablePreDownscaling();
}

ZEDMDAPI void ZeDMD_EnablePreUpscaling(ZeDMD* pZeDMD) {
  return pZeDMD->EnablePreUpscaling();
}

ZEDMDAPI void ZeDMD_DisablePreUpscaling(ZeDMD* pZeDMD) {
  return pZeDMD->DisablePreUpscaling();
}

ZEDMDAPI void ZeDMD_EnableUpscaling(ZeDMD* pZeDMD) {
  return pZeDMD->EnableUpscaling();
}

ZEDMDAPI void ZeDMD_DisableUpscaling(ZeDMD* pZeDMD) {
  return pZeDMD->DisableUpscaling();
}

ZEDMDAPI void ZeDMD_SetWiFiSSID(ZeDMD* pZeDMD, const char* const ssid) {
  return pZeDMD->SetWiFiSSID(ssid);
}

ZEDMDAPI void ZeDMD_SetWiFiPassword(ZeDMD* pZeDMD, const char* const password) {
  return pZeDMD->SetWiFiPassword(password);
}

ZEDMDAPI void ZeDMD_SetWiFiPort(ZeDMD* pZeDMD, int port) {
  return pZeDMD->SetWiFiPort(port);
}

ZEDMDAPI void ZeDMD_EnforceStreaming(ZeDMD* pZeDMD) {
  return pZeDMD->EnforceStreaming();
}

ZEDMDAPI void ZeDMD_ClearScreen(ZeDMD* pZeDMD) { return pZeDMD->ClearScreen(); }

ZEDMDAPI void ZeDMD_RenderGray2(ZeDMD* pZeDMD, uint8_t* frame) {
  return pZeDMD->RenderGray2(frame);
}

ZEDMDAPI void ZeDMD_RenderGray4(ZeDMD* pZeDMD, uint8_t* frame) {
  return pZeDMD->RenderGray4(frame);
}

ZEDMDAPI void ZeDMD_RenderColoredGray6(ZeDMD* pZeDMD, uint8_t* frame,
                                       uint8_t* rotations) {
  return pZeDMD->RenderColoredGray6(frame, rotations);
}

ZEDMDAPI void ZeDMD_RenderRgb24(ZeDMD* pZeDMD, uint8_t* frame) {
  return pZeDMD->RenderRgb24(frame);
}

ZEDMDAPI void ZeDMD_RenderRgb24EncodedAs565(ZeDMD* pZeDMD, uint8_t* frame) {
  return pZeDMD->RenderRgb24EncodedAs565(frame);
}
