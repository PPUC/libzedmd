#include "ZeDMD.h"

#include <cstring>

#include "ZeDMDComm.h"
#include "ZeDMDWiFi.h"

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
}

void ZeDMD::SetLogCallback(ZeDMD_LogCallback callback, const void* userData) {
  m_pZeDMDComm->SetLogCallback(callback, userData);
  m_pZeDMDWiFi->SetLogCallback(callback, userData);
}

void ZeDMD::Close() {
  m_pZeDMDComm->Disconnect();
  m_pZeDMDWiFi->Disconnect();
}

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

void ZeDMD::DisablePreUpscaling() {
  m_upscaling = false;
  m_hd = false;
}

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

void ZeDMD::ClearScreen() {
  if (m_usb) {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
  } else if (m_wifi) {
    m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::ClearScreen);
  }
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
    ConvertToRgb24(m_pPlanes, m_pScaledFrameBuffer, bufferSize, m_palette4);

    if (m_wifi) {
      m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream,
                                 m_pPlanes, bufferSize * 3, width, height);
    }
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize * 3, width, height);
  } else if (m_usb) {
    Split(m_pPlanes, width, height, 2, m_pScaledFrameBuffer);

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
    ConvertToRgb24(m_pPlanes, m_pScaledFrameBuffer, bufferSize, m_palette16);

    if (m_wifi) {
      m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream,
                                 m_pPlanes, bufferSize * 3, width, height);
    }
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize * 3, width, height);
  } else if (m_usb) {
    // Review: why?
    bufferSize /= 2;
    Split(m_pPlanes, width, height, 4, m_pScaledFrameBuffer);

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
    ConvertToRgb24(m_pPlanes, m_pScaledFrameBuffer, bufferSize, m_palette64);

    if (m_wifi) {
      m_pZeDMDWiFi->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream,
                                 m_pPlanes, bufferSize * 3, width, height);
    }
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize * 3, width, height);
  } else if (m_usb) {
    Split(m_pPlanes, width, height, 6, m_pScaledFrameBuffer);

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
  } else if (m_hd || m_streaming) {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24ZonesStream, m_pPlanes,
                               bufferSize, width, height);
  } else if (m_usb) {
    m_pZeDMDComm->QueueCommand(ZEDMD_COMM_COMMAND::RGB24, m_pPlanes,
                               bufferSize);
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

/**
 * Derived from
 * https://github.com/freezy/dmd-extensions/blob/master/LibDmd/Common/FrameUtil.cs
 */

void ZeDMD::Split(uint8_t* pPlanes, uint16_t width, uint16_t height,
                  uint8_t bitlen, uint8_t* pFrame) {
  int planeSize = width * height / 8;
  int pos = 0;
  uint8_t* bd = (uint8_t*)malloc(bitlen);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x += 8) {
      memset(bd, 0, bitlen * sizeof(uint8_t));

      for (int v = 7; v >= 0; v--) {
        uint8_t pixel = pFrame[(y * width) + (x + v)];
        for (int i = 0; i < bitlen; i++) {
          bd[i] <<= 1;
          if ((pixel & (1 << i)) != 0) {
            bd[i] |= 1;
          }
        }
      }

      for (int i = 0; i < bitlen; i++) {
        pPlanes[i * planeSize + pos] = bd[i];
      }

      pos++;
    }
  }

  free(bd);
}

void ZeDMD::ConvertToRgb24(uint8_t* pFrameRgb24, uint8_t* pFrame, int size,
                           uint8_t* pPalette) {
  for (int i = 0; i < size; i++) {
    memcpy(&pFrameRgb24[i * 3], &pPalette[pFrame[i] * 3], 3);
  }
}

bool ZeDMD::CmpColor(uint8_t* px1, uint8_t* px2, uint8_t colors) {
  if (colors == 3) {
    return (px1[0] == px2[0]) && (px1[1] == px2[1]) && (px1[2] == px2[2]);
  }

  return px1[0] == px2[0];
}

void ZeDMD::SetColor(uint8_t* px1, uint8_t* px2, uint8_t colors) {
  px1[0] = px2[0];

  if (colors == 3) {
    px1[1] = px2[1];
    px1[2] = px2[2];
  }
}

int ZeDMD::Scale(uint8_t* pScaledFrame, uint8_t* pFrame, uint8_t colors,
                 uint16_t* width, uint16_t* height) {
  int xoffset = 0;
  int yoffset = 0;
  uint8_t scale = 0;  // 0 - no scale, 1 - half scale, 2 - double scale
  uint16_t frameWidth = m_pZeDMDComm->GetWidth();
  uint16_t frameHeight = m_pZeDMDComm->GetHeight();
  int bufferSize = m_romWidth * m_romHeight * colors;

  if (m_upscaling && m_romWidth == 192 && frameWidth == 256) {
    (*width) = frameWidth;
    (*height) = frameHeight;

    xoffset = 32;
  } else if (m_downscaling && m_romWidth == 192) {
    (*width) = frameWidth;
    (*height) = frameHeight;

    xoffset = 16;
    scale = 1;
  } else if (m_upscaling && m_romHeight == 16 && frameHeight == 32) {
    (*width) = frameWidth;
    (*height) = frameHeight;

    yoffset = 8;
  } else if (m_upscaling && m_romHeight == 16 && frameHeight == 64) {
    (*width) = frameWidth;
    (*height) = frameHeight;

    yoffset = 16;
    scale = 2;
  } else if (m_downscaling && m_romWidth == 256 && frameWidth == 128) {
    (*width) = frameWidth;
    (*height) = frameHeight;

    scale = 1;
  } else if (m_upscaling && m_romWidth == 128 && frameWidth == 256) {
    (*width) = frameWidth;
    (*height) = frameHeight;

    scale = 2;
  } else {
    (*width) = m_romWidth;
    (*height) = m_romHeight;

    memcpy(pScaledFrame, pFrame, bufferSize);
    return bufferSize;
  }

  bufferSize = frameWidth * frameHeight * colors;
  memset(pScaledFrame, 0, bufferSize);

  if (scale == 1) {
    // for half scaling we take the 4 points and look if there is one colour
    // repeated
    for (int y = 0; y < m_romHeight; y += 2) {
      for (int x = 0; x < m_romWidth; x += 2) {
        uint16_t upper_left = y * m_romWidth * colors + x * colors;
        uint16_t upper_right = upper_left + colors;
        uint16_t lower_left = upper_left + m_romWidth * colors;
        uint16_t lower_right = lower_left + colors;
        uint16_t target = (xoffset + (x / 2) + (y / 2) * frameWidth) * colors;

        // Prefer most outer upper_lefts.
        if (x < m_romWidth / 2) {
          if (y < m_romHeight / 2) {
            if (CmpColor(&pFrame[upper_left], &pFrame[upper_right], colors) ||
                CmpColor(&pFrame[upper_left], &pFrame[lower_left], colors) ||
                CmpColor(&pFrame[upper_left], &pFrame[lower_right], colors)) {
              SetColor(&pScaledFrame[target], &pFrame[upper_left], colors);
            } else if (CmpColor(&pFrame[upper_right], &pFrame[lower_left],
                                colors) ||
                       CmpColor(&pFrame[upper_right], &pFrame[lower_right],
                                colors)) {
              SetColor(&pScaledFrame[target], &pFrame[upper_right], colors);
            } else if (CmpColor(&pFrame[lower_left], &pFrame[lower_right],
                                colors)) {
              SetColor(&pScaledFrame[target], &pFrame[lower_left], colors);
            } else {
              SetColor(&pScaledFrame[target], &pFrame[upper_left], colors);
            }
          } else {
            if (CmpColor(&pFrame[lower_left], &pFrame[lower_right], colors) ||
                CmpColor(&pFrame[lower_left], &pFrame[upper_left], colors) ||
                CmpColor(&pFrame[lower_left], &pFrame[upper_right], colors)) {
              SetColor(&pScaledFrame[target], &pFrame[lower_left], colors);
            } else if (CmpColor(&pFrame[lower_right], &pFrame[upper_left],
                                colors) ||
                       CmpColor(&pFrame[lower_right], &pFrame[upper_right],
                                colors)) {
              SetColor(&pScaledFrame[target], &pFrame[lower_right], colors);
            } else if (CmpColor(&pFrame[upper_left], &pFrame[upper_right],
                                colors)) {
              SetColor(&pScaledFrame[target], &pFrame[upper_left], colors);
            } else {
              SetColor(&pScaledFrame[target], &pFrame[lower_left], colors);
            }
          }
        } else {
          if (y < m_romHeight / 2) {
            if (CmpColor(&pFrame[upper_right], &pFrame[upper_left], colors) ||
                CmpColor(&pFrame[upper_right], &pFrame[lower_right], colors) ||
                CmpColor(&pFrame[upper_right], &pFrame[lower_left], colors)) {
              SetColor(&pScaledFrame[target], &pFrame[upper_right], colors);
            } else if (CmpColor(&pFrame[upper_left], &pFrame[lower_right],
                                colors) ||
                       CmpColor(&pFrame[upper_left], &pFrame[lower_left],
                                colors)) {
              SetColor(&pScaledFrame[target], &pFrame[upper_left], colors);
            } else if (CmpColor(&pFrame[lower_right], &pFrame[lower_left],
                                colors)) {
              SetColor(&pScaledFrame[target], &pFrame[lower_right], colors);
            } else {
              SetColor(&pScaledFrame[target], &pFrame[upper_right], colors);
            }
          } else {
            if (CmpColor(&pFrame[lower_right], &pFrame[lower_left], colors) ||
                CmpColor(&pFrame[lower_right], &pFrame[upper_right], colors) ||
                CmpColor(&pFrame[lower_right], &pFrame[upper_left], colors)) {
              SetColor(&pScaledFrame[target], &pFrame[lower_right], colors);
            } else if (CmpColor(&pFrame[lower_left], &pFrame[upper_right],
                                colors) ||
                       CmpColor(&pFrame[lower_left], &pFrame[upper_left],
                                colors)) {
              SetColor(&pScaledFrame[target], &pFrame[lower_left], colors);
            } else if (CmpColor(&pFrame[upper_right], &pFrame[upper_left],
                                colors)) {
              SetColor(&pScaledFrame[target], &pFrame[upper_right], colors);
            } else {
              SetColor(&pScaledFrame[target], &pFrame[lower_right], colors);
            }
          }
        }
      }
    }
  } else if (scale == 2) {
    // we implement scale2x http://www.scale2x.it/algorithm
    uint16_t row = m_romWidth * colors;
    uint8_t* a = (uint8_t*)malloc(colors);
    uint8_t* b = (uint8_t*)malloc(colors);
    uint8_t* c = (uint8_t*)malloc(colors);
    uint8_t* d = (uint8_t*)malloc(colors);
    uint8_t* e = (uint8_t*)malloc(colors);
    uint8_t* f = (uint8_t*)malloc(colors);
    uint8_t* g = (uint8_t*)malloc(colors);
    uint8_t* h = (uint8_t*)malloc(colors);
    uint8_t* i = (uint8_t*)malloc(colors);

    for (int x = 0; x < m_romHeight; x++) {
      for (int y = 0; y < m_romWidth; y++) {
        for (uint8_t tc = 0; tc < colors; tc++) {
          if (y == 0 && x == 0) {
            a[tc] = b[tc] = d[tc] = e[tc] = pFrame[tc];
            c[tc] = f[tc] = pFrame[colors + tc];
            g[tc] = h[tc] = pFrame[row + tc];
            i[tc] = pFrame[row + colors + tc];
          } else if ((y == 0) && (x == m_romHeight - 1)) {
            a[tc] = b[tc] = pFrame[(x - 1) * row + tc];
            c[tc] = pFrame[(x - 1) * row + colors + tc];
            d[tc] = g[tc] = h[tc] = e[tc] = pFrame[x * row + tc];
            f[tc] = i[tc] = pFrame[x * row + colors + tc];
          } else if ((y == m_romWidth - 1) && (x == 0)) {
            a[tc] = d[tc] = pFrame[y * colors - colors + tc];
            b[tc] = c[tc] = f[tc] = e[tc] = pFrame[y * colors + tc];
            g[tc] = pFrame[row + y * colors - colors + tc];
            h[tc] = i[tc] = pFrame[row + y * colors + tc];
          } else if ((y == m_romWidth - 1) && (x == m_romHeight - 1)) {
            a[tc] = pFrame[x * row - 2 * colors + tc];
            b[tc] = c[tc] = pFrame[x * row - colors + tc];
            d[tc] = g[tc] = pFrame[m_romHeight * row - 2 * colors + tc];
            e[tc] = f[tc] = h[tc] = i[tc] =
                pFrame[m_romHeight * row - colors + tc];
          } else if (y == 0) {
            a[tc] = b[tc] = pFrame[(x - 1) * row + tc];
            c[tc] = pFrame[(x - 1) * row + colors + tc];
            d[tc] = e[tc] = pFrame[x * row + tc];
            f[tc] = pFrame[x * row + colors + tc];
            g[tc] = h[tc] = pFrame[(x + 1) * row + tc];
            i[tc] = pFrame[(x + 1) * row + colors + tc];
          } else if (y == m_romWidth - 1) {
            a[tc] = pFrame[x * row - 2 * colors + tc];
            b[tc] = c[tc] = pFrame[x * row - colors + tc];
            d[tc] = pFrame[(x + 1) * row - 2 * colors + tc];
            e[tc] = f[tc] = pFrame[(x + 1) * row - colors + tc];
            g[tc] = pFrame[(x + 2) * row - 2 * colors + tc];
            h[tc] = i[tc] = pFrame[(x + 2) * row - colors + tc];
          } else if (x == 0) {
            a[tc] = d[tc] = pFrame[y * colors - colors + tc];
            b[tc] = e[tc] = pFrame[y * colors + tc];
            c[tc] = f[tc] = pFrame[y * colors + colors + tc];
            g[tc] = pFrame[row + y * colors - colors + tc];
            h[tc] = pFrame[row + y * colors + tc];
            i[tc] = pFrame[row + y * colors + colors + tc];
          } else if (x == m_romHeight - 1) {
            a[tc] = pFrame[(x - 1) * row + y * colors - colors + tc];
            b[tc] = pFrame[(x - 1) * row + y * colors + tc];
            c[tc] = pFrame[(x - 1) * row + y * colors + colors + tc];
            d[tc] = g[tc] = pFrame[x * row + y * colors - colors + tc];
            e[tc] = h[tc] = pFrame[x * row + y * colors + tc];
            f[tc] = i[tc] = pFrame[x * row + y * colors + colors + tc];
          } else {
            a[tc] = pFrame[(x - 1) * row + y * colors - colors + tc];
            b[tc] = pFrame[(x - 1) * row + y * colors + tc];
            c[tc] = pFrame[(x - 1) * row + y * colors + colors + tc];
            d[tc] = pFrame[x * row + y * colors - colors + tc];
            e[tc] = pFrame[x * row + y * colors + tc];
            f[tc] = pFrame[x * row + y * colors + colors + tc];
            g[tc] = pFrame[(x + 1) * row + y * colors - colors + tc];
            h[tc] = pFrame[(x + 1) * row + y * colors + tc];
            i[tc] = pFrame[(x + 1) * row + y * colors + colors + tc];
          }
        }

        if (!CmpColor(b, h, colors) && !CmpColor(d, f, colors)) {
          SetColor(&pScaledFrame[(yoffset * frameWidth + x * 2 * frameWidth +
                                  y * 2 + xoffset) *
                                 colors],
                   CmpColor(d, b, colors) ? d : e, colors);
          SetColor(&pScaledFrame[(yoffset * frameWidth + x * 2 * frameWidth +
                                  y * 2 + 1 + xoffset) *
                                 colors],
                   CmpColor(b, f, colors) ? f : e, colors);
          SetColor(&pScaledFrame[(yoffset * frameWidth +
                                  (x * 2 + 1) * frameWidth + y * 2 + xoffset) *
                                 colors],
                   CmpColor(d, h, colors) ? d : e, colors);
          SetColor(
              &pScaledFrame[(yoffset * frameWidth + (x * 2 + 1) * frameWidth +
                             y * 2 + 1 + xoffset) *
                            colors],
              CmpColor(h, f, colors) ? f : e, colors);
        } else {
          SetColor(&pScaledFrame[(yoffset * frameWidth + x * 2 * frameWidth +
                                  y * 2 + xoffset) *
                                 colors],
                   e, colors);
          SetColor(&pScaledFrame[(yoffset * frameWidth + x * 2 * frameWidth +
                                  y * 2 + 1 + xoffset) *
                                 colors],
                   e, colors);
          SetColor(&pScaledFrame[(yoffset * frameWidth +
                                  (x * 2 + 1) * frameWidth + y * 2 + xoffset) *
                                 colors],
                   e, colors);
          SetColor(
              &pScaledFrame[(yoffset * frameWidth + (x * 2 + 1) * frameWidth +
                             y * 2 + 1 + xoffset) *
                            colors],
              e, colors);
        }
      }
    }

    free(a);
    free(b);
    free(c);
    free(d);
    free(e);
    free(f);
    free(g);
    free(h);
    free(i);
  } else  // offset!=0
  {
    for (int y = 0; y < m_romHeight; y++) {
      for (int x = 0; x < m_romWidth; x++) {
        for (uint8_t c = 0; c < colors; c++) {
          pScaledFrame[((yoffset + y) * frameWidth + xoffset + x) * colors +
                       c] = pFrame[(y * m_romWidth + x) * colors + c];
        }
      }
    }
  }

  return bufferSize;
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
