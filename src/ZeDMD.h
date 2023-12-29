#pragma once

#define ZEDMD_VERSION_MAJOR 0 // X Digits
#define ZEDMD_VERSION_MINOR 3 // Max 2 Digits
#define ZEDMD_VERSION_PATCH 0 // Max 2 Digits

#define _ZEDMD_STR(x) #x
#define ZEDMD_STR(x) _ZEDMD_STR(x)

#define ZEDMD_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR) "." ZEDMD_STR(ZEDMD_VERSION_PATCH)
#define ZEDMD_MINOR_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR)

#define ZEDMD_MAX_WIDTH 256
#define ZEDMD_MAX_HEIGHT 64
#define ZEDMD_MAX_PALETTE 192

#if defined(_WIN32) || defined(_WIN64)
#define ZEDMDAPI __declspec(dllexport)
#define CALLBACK __stdcall
#else
#define ZEDMDAPI __attribute__((visibility("default")))
#define CALLBACK
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __ANDROID__
typedef void *(*ZeDMD_AndroidGetJNIEnvFunc)();
#endif

typedef void(CALLBACK *ZeDMD_LogMessageCallback)(const char *format, va_list args, const void *userData);

class ZeDMDComm;
class ZeDMDWiFi;

class ZEDMDAPI ZeDMD
{
public:
   ZeDMD();
   ~ZeDMD();

#ifdef __ANDROID__
   void SetAndroidGetJNIEnvFunc(ZeDMD_AndroidGetJNIEnvFunc func);
#endif

   void SetLogMessageCallback(ZeDMD_LogMessageCallback callback, const void *userData);

   void IgnoreDevice(const char *const ignore_device);
   void SetDevice(const char *const device);
   bool Open(uint16_t width, uint16_t height);
   bool Open();
   bool OpenWiFi(const char *ip, int port);
   void Close();

   void SetFrameSize(uint16_t width, uint16_t height);
   void SetPalette(uint8_t *pPalette);
   void SetPalette(uint8_t *pPalette, uint8_t numColors);
   void SetDefaultPalette(uint8_t bitDepth);
   uint8_t *GetDefaultPalette(uint8_t bitDepth);
   void LedTest();
   void EnableDebug();
   void DisableDebug();
   void SetRGBOrder(uint8_t rgbOrder);
   void SetBrightness(uint8_t brightness);
   void SaveSettings();
   void EnablePreDownscaling();
   void DisablePreDownscaling();
   void EnablePreUpscaling();
   void DisablePreUpscaling();
   void EnableUpscaling();
   void DisableUpscaling();
   void SetWiFiSSID(const char *const ssid);
   void SetWiFiPassword(const char *const password);
   void SetWiFiPort(int port);
   void EnforceStreaming();

   void ClearScreen();
   void RenderGray2(uint8_t *frame);
   void RenderGray4(uint8_t *frame);
   void RenderColoredGray6(uint8_t *frame, uint8_t *palette, uint8_t *rotations);
   void RenderColoredGray6(uint8_t *frame, uint8_t *rotations);
   void RenderRgb24(uint8_t *frame);

private:
   bool UpdateFrameBuffer8(uint8_t *pFrame);
   bool UpdateFrameBuffer24(uint8_t *pFrame);

   void Split(uint8_t *planes, uint16_t width, uint16_t height, uint8_t bitlen, uint8_t *frame);
   void ConvertToRgb24(uint8_t *pFrameRgb24, uint8_t *pFrame, int size);
   void ConvertToRgb24(uint8_t *pFrameRgb24, uint8_t *pFrame, int size, uint8_t *pPalette);
   bool CmpColor(uint8_t *px1, uint8_t *px2, uint8_t colors);
   void SetColor(uint8_t *px1, uint8_t *px2, uint8_t colors);
   int Scale(uint8_t *pScaledFrame, uint8_t *pFrame, uint8_t colors, uint16_t *width, uint16_t *height);

   ZeDMDComm *m_pZeDMDComm;
   ZeDMDWiFi *m_pZeDMDWiFi;

   uint16_t m_romWidth;
   uint16_t m_romHeight;

   bool m_usb = false;
   bool m_wifi = false;
   bool m_hd = false;
   bool m_downscaling = false;
   bool m_upscaling = false;
   bool m_streaming = false;
   bool m_paletteChanged = false;

   uint8_t *m_pFrameBuffer;
   uint8_t *m_pScaledFrameBuffer;
   uint8_t *m_pCommandBuffer;
   uint8_t *m_pPlanes;

   uint8_t m_palette[ZEDMD_MAX_PALETTE] = {0};
   uint8_t m_DmdDefaultPalette2Bit[12] = {0, 0, 0, 144, 34, 0, 192, 76, 0, 255, 127, 0};
   uint8_t m_DmdDefaultPalette4Bit[48] = {0, 0, 0, 51, 25, 0, 64, 32, 0, 77, 38, 0,
                                          89, 44, 0, 102, 51, 0, 115, 57, 0, 128, 64, 0,
                                          140, 70, 0, 153, 76, 0, 166, 83, 0, 179, 89, 0,
                                          191, 95, 0, 204, 102, 0, 230, 114, 0, 255, 127, 0};
};

#ifdef __cplusplus
extern "C"
{
#endif

   extern ZEDMDAPI ZeDMD *ZeDMD_GetInstance() { return new ZeDMD(); };
   extern ZEDMDAPI void ZeDMD_IgnoreDevice(ZeDMD *pZeDMD, const char *const ignore_device) { return pZeDMD->IgnoreDevice(ignore_device); };
   extern ZEDMDAPI void ZeDMD_SetDevice(ZeDMD *pZeDMD, const char *const device) { return pZeDMD->SetDevice(device); };
   extern ZEDMDAPI bool ZeDMD_Open(ZeDMD *pZeDMD) { return pZeDMD->Open(); };
   extern ZEDMDAPI bool ZeDMD_OpenWiFi(ZeDMD *pZeDMD, const char *ip, int port) { return pZeDMD->OpenWiFi(ip, port); };
   extern ZEDMDAPI void ZeDMD_Close(ZeDMD *pZeDMD) { return pZeDMD->Close(); };

   extern ZEDMDAPI void ZeDMD_SetFrameSize(ZeDMD *pZeDMD, uint16_t width, uint16_t height) { return pZeDMD->SetFrameSize(width, height); };
   extern ZEDMDAPI void ZeDMD_SetPalette(ZeDMD *pZeDMD, uint8_t *pPalette, uint8_t numColors) { return pZeDMD->SetPalette(pPalette, numColors); };
   extern ZEDMDAPI void ZeDMD_SetDefaultPalette(ZeDMD *pZeDMD, uint8_t bitDepth) { return pZeDMD->SetDefaultPalette(bitDepth); };
   extern ZEDMDAPI uint8_t *ZeDMD_GetDefaultPalette(ZeDMD *pZeDMD, uint8_t bitDepth) { return pZeDMD->GetDefaultPalette(bitDepth); };
   extern ZEDMDAPI void ZeDMD_LedTest(ZeDMD *pZeDMD) { return pZeDMD->LedTest(); };
   extern ZEDMDAPI void ZeDMD_EnableDebug(ZeDMD *pZeDMD) { return pZeDMD->EnableDebug(); };
   extern ZEDMDAPI void ZeDMD_DisableDebug(ZeDMD *pZeDMD) { return pZeDMD->DisableDebug(); };
   extern ZEDMDAPI void ZeDMD_SetRGBOrder(ZeDMD *pZeDMD, uint8_t rgbOrder) { return pZeDMD->SetRGBOrder(rgbOrder); };
   extern ZEDMDAPI void ZeDMD_SetBrightness(ZeDMD *pZeDMD, uint8_t brightness) { return pZeDMD->SetBrightness(brightness); };
   extern ZEDMDAPI void ZeDMD_SaveSettings(ZeDMD *pZeDMD) { return pZeDMD->SaveSettings(); };
   extern ZEDMDAPI void ZeDMD_EnablePreDownscaling(ZeDMD *pZeDMD) { return pZeDMD->EnablePreDownscaling(); };
   extern ZEDMDAPI void ZeDMD_DisablePreDownscaling(ZeDMD *pZeDMD) { return pZeDMD->DisablePreDownscaling(); };
   extern ZEDMDAPI void ZeDMD_EnablePreUpscaling(ZeDMD *pZeDMD) { return pZeDMD->EnablePreUpscaling(); };
   extern ZEDMDAPI void ZeDMD_DisablePreUpscaling(ZeDMD *pZeDMD) { return pZeDMD->DisablePreUpscaling(); };
   extern ZEDMDAPI void ZeDMD_EnableUpscaling(ZeDMD *pZeDMD) { return pZeDMD->EnableUpscaling(); };
   extern ZEDMDAPI void ZeDMD_DisableUpscaling(ZeDMD *pZeDMD) { return pZeDMD->DisableUpscaling(); };
   extern ZEDMDAPI void ZeDMD_SetWiFiSSID(ZeDMD *pZeDMD, const char *const ssid) { return pZeDMD->SetWiFiSSID(ssid); };
   extern ZEDMDAPI void ZeDMD_SetWiFiPassword(ZeDMD *pZeDMD, const char *const password) { return pZeDMD->SetWiFiPassword(password); };
   extern ZEDMDAPI void ZeDMD_SetWiFiPort(ZeDMD *pZeDMD, int port) { return pZeDMD->SetWiFiPort(port); };
   extern ZEDMDAPI void ZeDMD_EnforceStreaming(ZeDMD *pZeDMD) { return pZeDMD->EnforceStreaming(); };

   extern ZEDMDAPI void ZeDMD_ClearScreen(ZeDMD *pZeDMD) { return pZeDMD->ClearScreen(); };
   extern ZEDMDAPI void ZeDMD_RenderGray2(ZeDMD *pZeDMD, uint8_t *frame) { return pZeDMD->RenderGray2(frame); };
   extern ZEDMDAPI void ZeDMD_RenderGray4(ZeDMD *pZeDMD, uint8_t *frame) { return pZeDMD->RenderGray4(frame); };
   extern ZEDMDAPI void ZeDMD_RenderColoredGray6(ZeDMD *pZeDMD, uint8_t *frame, uint8_t *rotations) { return pZeDMD->RenderColoredGray6(frame, rotations); };
   extern ZEDMDAPI void ZeDMD_RenderRgb24(ZeDMD *pZeDMD, uint8_t *frame) { return pZeDMD->RenderRgb24(frame); };

#ifdef __cplusplus
}
#endif
