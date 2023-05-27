#pragma once

#define ZEDMD_VERSION_MAJOR 0  // X Digits
#define ZEDMD_VERSION_MINOR 1  // Max 2 Digits
#define ZEDMD_VERSION_PATCH 0  // Max 2 Digits

#define _ZEDMD_STR(x)    #x
#define ZEDMD_STR(x)     _ZEDMD_STR(x)

#define ZEDMD_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR) "." ZEDMD_STR(ZEDMD_VERSION_PATCH)
#define ZEDMD_MINOR_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR)

#include <inttypes.h>
#include "ZeDMDComm.h"

class ZeDMD
{
public:
   ZeDMD();
   ~ZeDMD();

   void Open(int width, int height);

   void SetPalette(uint8_t* pPalette);

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

   int m_numColors;

   bool m_debug = false;
   int m_rgbOrder = -1;
   int m_brightness = -1;
};