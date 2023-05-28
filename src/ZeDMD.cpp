#include "ZeDMD.h"

ZeDMD::ZeDMD()
{
   m_width = 0;
   m_height = 0;

   m_numColors = 0;

   memset(&m_palette, 0, sizeof(m_palette));

   m_pFrameBuffer = NULL;
   m_pCommandBuffer = NULL;
   m_pPlanes = NULL;
}

ZeDMD::~ZeDMD()
{
   m_zedmdComm.Disconnect();

   if (m_pFrameBuffer)
      delete m_pFrameBuffer;

   if (m_pCommandBuffer)
      delete m_pCommandBuffer;

   if (m_pPlanes)
      delete m_pPlanes;
}

void ZeDMD::IgnoreDevice(const char* ignore_device)
{
   m_zedmdComm.IgnoreDevice(ignore_device);
}

void ZeDMD::SetFrameSize(uint8_t width, uint8_t height)
{
      m_width = width;
      m_height = height;

      uint8_t size[4] = { (uint8_t)(width & 0xFF), (uint8_t)((width >> 8) & 0xFF),
                        (uint8_t)(height & 0xFF), (uint8_t)((height >> 8) & 0xFF) };

      m_zedmdComm.QueueCommand(ZEDMD_COMMAND::FrameSize, size, 4);
}

void ZeDMD::Open()
{
   m_available = m_zedmdComm.Connect();

   if (m_available) {

      m_pFrameBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
      m_pCommandBuffer = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);
      m_pPlanes = (uint8_t*)malloc(ZEDMD_MAX_WIDTH * ZEDMD_MAX_HEIGHT * 3);

      if (m_debug) {
         m_zedmdComm.QueueCommand(ZEDMD_COMMAND::EnableDebug);
         //PLOGI.printf("ZeDMD debug enabled");
      }

      if (m_rgbOrder != -1) {
         m_zedmdComm.QueueCommand(ZEDMD_COMMAND::RGBOrder, m_rgbOrder);
         //PLOGI.printf("ZeDMD RGB order override: rgbOrder=%d", rgbOrder);
      }

      if (m_brightness != -1) {
         m_zedmdComm.QueueCommand(ZEDMD_COMMAND::Brightness, m_brightness);
         //PLOGI.printf("ZeDMD brightness override: brightness=%d", brightness);
      }

      m_zedmdComm.Run();
   }
}

void ZeDMD::Open(int width, int height)
{
   Open();
   SetFrameSize(width, height);
}

void ZeDMD::SetPalette(uint8_t* pPalette)
{
   if (!m_available)
      return;

   memcpy(&m_palette, pPalette, sizeof(m_palette));
}

void ZeDMD::RenderGray2(uint8_t* pFrame)
{
   if (!m_available || !UpdateFrameBuffer8(pFrame))
      return;

   int bufferSize = (m_width * m_height) / 8 * 2;

   Split(m_pPlanes, m_width, m_height, 2, m_pFrameBuffer);

   memcpy(m_pCommandBuffer, &m_palette, 12);
   memcpy(m_pCommandBuffer + 12, m_pPlanes, bufferSize);

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::Gray2, m_pCommandBuffer, 12 + bufferSize);
}

void ZeDMD::RenderGray4(uint8_t* pFrame)
{
   if (!m_available || !UpdateFrameBuffer8(pFrame))
      return;

   int bufferSize = (m_width * m_height) / 8 * 4;

   Split(m_pPlanes, m_width, m_height, 4, m_pFrameBuffer);

   memcpy(m_pCommandBuffer, m_palette, 48);
   memcpy(m_pCommandBuffer + 48, m_pPlanes, bufferSize);

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::ColGray4, m_pCommandBuffer, 48 + bufferSize);
}

void ZeDMD::RenderColoredGray6(uint8_t* pFrame, uint8_t* pPalette, uint8_t* pRotations)
{
   if (!m_available)
      return;

   bool change = UpdateFrameBuffer8(pFrame);

   if (memcmp(&m_palette, pPalette, 192)) {
      memcpy(&m_palette, pPalette, 192);
      change = true;
   }

   if (!change)
      return;

   int bufferSize = (m_width * m_height) / 8 * 6;

   Split(m_pPlanes, m_width, m_height, 6, m_pFrameBuffer);

   memcpy(m_pCommandBuffer, pPalette, 192);
   memcpy(m_pCommandBuffer + 192, m_pPlanes, bufferSize);

   if (pRotations)
      memcpy(m_pCommandBuffer + 192 + bufferSize, pRotations, 24);
   else
      memset(m_pCommandBuffer + 192 + bufferSize, 255, 24);

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::ColGray6, m_pCommandBuffer, 192 + bufferSize + 24);
}

void ZeDMD::RenderRgb24(uint8_t* pFrame)
{
   if (!m_available || !UpdateFrameBuffer24(pFrame))
      return;

   m_zedmdComm.QueueCommand(ZEDMD_COMMAND::RGB24, m_pFrameBuffer, m_width * m_height * 3);
}

bool ZeDMD::UpdateFrameBuffer8(uint8_t* pFrame)
{
   if (!memcmp(m_pFrameBuffer, pFrame, m_width * m_height))
      return false;

   memcpy(m_pFrameBuffer, pFrame, m_width * m_height);
   return true;
}

bool ZeDMD::UpdateFrameBuffer24(uint8_t* pFrame)
{
   if (!memcmp(m_pFrameBuffer, pFrame, m_width * m_height * 3))
      return false;

   memcpy(m_pFrameBuffer, pFrame, m_width * m_height * 3);
   return true;
}

/**
 * Derived from https://github.com/freezy/dmd-extensions/blob/master/LibDmd/Common/FrameUtil.cs
 */

void ZeDMD::Split(uint8_t* pPlanes, int width, int height, int bitlen, uint8_t* pFrame)
{
   int planeSize = width * height / 8;
   int pos = 0;
   uint8_t bd[bitlen];

   for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x += 8) {
          memset(bd, 0, bitlen * sizeof(uint8_t));

          for (int v = 7; v >= 0; v--) {
             uint8_t pixel = pFrame[(y * width) + (x + v)];
             for (int i = 0; i < bitlen; i++) {
                bd[i] <<= 1;
                if ((pixel & (1 << i)) != 0)
                   bd[i] |= 1;
             }
          }

          for (int i = 0; i < bitlen; i++)
             pPlanes[i * planeSize + pos] = bd[i];

          pos++;
       }
    }
}