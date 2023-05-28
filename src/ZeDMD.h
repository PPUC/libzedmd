#pragma once

#define ZEDMD_VERSION_MAJOR 0  // X Digits
#define ZEDMD_VERSION_MINOR 1  // Max 2 Digits
#define ZEDMD_VERSION_PATCH 0  // Max 2 Digits

#define _ZEDMD_STR(x)    #x
#define ZEDMD_STR(x)     _ZEDMD_STR(x)

#define ZEDMD_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR) "." ZEDMD_STR(ZEDMD_VERSION_PATCH)
#define ZEDMD_MINOR_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR)

#define ZEDMD_MAX_WIDTH 256
#define ZEDMD_MAX_HEIGHT 64

#include <inttypes.h>
#include "ZeDMDComm.h"

class ZeDMD
{
public:
   ZeDMD();
   ~ZeDMD();

   void IgnoreDevice(const char* ignore_device);
   bool Open(int width, int height);
   bool Open();

   void SetFrameSize(uint8_t width, uint8_t height);
   void SetPalette(uint8_t* pPalette);
   void SetDefaultPalette(int bitDepth);

   void RenderGray2(uint8_t* frame);
   void RenderGray4(uint8_t* frame);
   void RenderColoredGray6(uint8_t* frame, uint8_t* palette, uint8_t* rotations);
   void RenderRgb24(uint8_t* frame);

private:
   bool UpdateFrameBuffer8(uint8_t* pFrame);
   bool UpdateFrameBuffer24(uint8_t* PFNGLFRAMEBUFFERTEXTURE3DPROC);

   void Split(uint8_t* planes, int width, int height, int bitlen, uint8_t* frame);

   ZeDMDComm m_zedmdComm;

   int m_width;
   int m_height;

   bool m_available;

   uint8_t* m_pFrameBuffer;
   uint8_t* m_pCommandBuffer;
   uint8_t* m_pPlanes;

   uint8_t m_palette[64 * 3];
   uint8_t m_DmdDefaultPalette2Bit[12] = { 0, 0, 0, 144, 34, 0, 192, 76, 0, 255, 127 ,0 };
   uint8_t m_DmdDefaultPalette4Bit[48] = { 0, 0, 0, 51, 25, 0, 64, 32, 0, 77, 38, 0,
                                           89, 44, 0, 102, 51, 0, 115, 57, 0, 128, 64, 0,
                                           140, 70, 0, 153, 76, 0, 166, 83, 0, 179, 89, 0,
                                           191, 95, 0, 204, 102, 0, 230, 114, 0, 255, 127, 0 };

   bool m_debug = false;
   int m_rgbOrder = -1;
   int m_brightness = -1;
};