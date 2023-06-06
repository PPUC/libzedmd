#pragma once

#define ZEDMD_VERSION_MAJOR 0 // X Digits
#define ZEDMD_VERSION_MINOR 1 // Max 2 Digits
#define ZEDMD_VERSION_PATCH 0 // Max 2 Digits

#define _ZEDMD_STR(x) #x
#define ZEDMD_STR(x) _ZEDMD_STR(x)

#define ZEDMD_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR) "." ZEDMD_STR(ZEDMD_VERSION_PATCH)
#define ZEDMD_MINOR_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR)

#define ZEDMD_MAX_WIDTH 256
#define ZEDMD_MAX_HEIGHT 64

#if defined(_WIN32) || defined(_WIN64)
#define ZEDMDAPI __declspec(dllexport)
#define CALLBACK __stdcall
#else
#define ZEDMDAPI __attribute__ ((visibility ("default")))
#define CALLBACK
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __ANDROID__
typedef void* (*ZeDMD_AndroidGetJNIEnvFunc)();
#endif

typedef void (CALLBACK *ZeDMD_LogMessageCallback)(const char* format, va_list args, const void* userData);

class ZeDMDComm;

class ZEDMDAPI ZeDMD
{
public:
   ZeDMD();
   ~ZeDMD();

#ifdef __ANDROID__
   void SetAndroidGetJNIEnvFunc(ZeDMD_AndroidGetJNIEnvFunc func);
#endif

   void SetLogMessageCallback(ZeDMD_LogMessageCallback callback, const void* userData);

   void IgnoreDevice(const char *ignore_device);
   bool Open(int width, int height);
   bool Open();

   void SetFrameSize(uint8_t width, uint8_t height);
   void SetPalette(uint8_t *pPalette);
   void SetPaletteWithRGB(int bitDepth, uint8_t r, uint8_t g, uint8_t b);
   void SetDefaultPalette(int bitDepth);
   uint8_t *GetDefaultPalette(int bitDepth);
   void EnableDebug();
   void DisableDebug();
   void SetRGBOrder(int rgbOrder);
   void SetBrightness(int brightness);
   void SaveSettings();
   void EnablePreDownscaling();
   void DisablePreDownscaling();
   void EnablePreUpscaling();
   void DisablePreUpscaling();
   void EnableUpscaling();
   void DisableUpscaling();

   void RenderGray2(uint8_t *frame);
   void RenderGray4(uint8_t *frame);
   void RenderColoredGray6(uint8_t *frame, uint8_t *palette, uint8_t *rotations);
   void RenderRgb24(uint8_t *frame);

private:
   bool UpdateFrameBuffer8(uint8_t *pFrame);
   bool UpdateFrameBuffer24(uint8_t *pFrame);

   void Split(uint8_t *planes, int width, int height, int bitlen, uint8_t *frame);
   bool CmpColor(uint8_t *px1, uint8_t *px2, uint8_t colors);
   void SetColor(uint8_t *px1, uint8_t *px2, uint8_t colors);
   int Scale(uint8_t *pScaledFrame, uint8_t *pFrame, uint8_t colors);

   ZeDMDComm* m_pZeDMDComm;

   int m_width;
   int m_height;

   bool m_available;
   bool m_downscaling = true;
   bool m_upscaling = true;

   uint8_t *m_pFrameBuffer;
   uint8_t *m_pScaledFrameBuffer;
   uint8_t *m_pCommandBuffer;
   uint8_t *m_pPlanes;

   uint8_t m_palette[64 * 3];
   uint8_t m_DmdDefaultPalette2Bit[12] = {0, 0, 0, 144, 34, 0, 192, 76, 0, 255, 127, 0};
   uint8_t m_DmdDefaultPalette4Bit[48] = {0, 0, 0, 51, 25, 0, 64, 32, 0, 77, 38, 0,
                                          89, 44, 0, 102, 51, 0, 115, 57, 0, 128, 64, 0,
                                          140, 70, 0, 153, 76, 0, 166, 83, 0, 179, 89, 0,
                                          191, 95, 0, 204, 102, 0, 230, 114, 0, 255, 127, 0};
};
