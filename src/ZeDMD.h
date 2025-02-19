/** @file ZeDMD.h
 *  @brief ZeDMD client library.
 *
 *  Connecting ZeDMD devices.
 */
#pragma once

#define ZEDMD_VERSION_MAJOR 0  // X Digits
#define ZEDMD_VERSION_MINOR 9  // Max 2 Digits
#define ZEDMD_VERSION_PATCH 6  // Max 2 Digits

#define _ZEDMD_STR(x) #x
#define ZEDMD_STR(x) _ZEDMD_STR(x)

#define ZEDMD_VERSION            \
  ZEDMD_STR(ZEDMD_VERSION_MAJOR) \
  "." ZEDMD_STR(ZEDMD_VERSION_MINOR) "." ZEDMD_STR(ZEDMD_VERSION_PATCH)
#define ZEDMD_MINOR_VERSION ZEDMD_STR(ZEDMD_VERSION_MAJOR) "." ZEDMD_STR(ZEDMD_VERSION_MINOR)

#define ZEDMD_MAX_WIDTH 256
#define ZEDMD_MAX_HEIGHT 64

#ifdef _MSC_VER
#define ZEDMDAPI __declspec(dllexport)
#define ZEDMDCALLBACK __stdcall
#else
#define ZEDMDAPI __attribute__((visibility("default")))
#define ZEDMDCALLBACK
#endif

#include <inttypes.h>
#include <stdarg.h>

#include <cstdio>

typedef void(ZEDMDCALLBACK* ZeDMD_LogCallback)(const char* format, va_list args, const void* userData);

class ZeDMDComm;
class ZeDMDWiFi;

class ZEDMDAPI ZeDMD
{
 public:
  ZeDMD();
  ~ZeDMD();

  /** @brief Set the log callback
   *
   *  Set the log callback.
   *
   *  @param callback
   *  @param userData
   */
  void SetLogCallback(ZeDMD_LogCallback callback, const void* userData);

  /** @brief Ignore a serial device when searching for ZeDMD
   *
   *  While searching for a ZeDMD any serial ports are tested.
   *  This could cause trouble with other devices attached via
   *  USB or serial ports.
   *  Using this mfunction, a serial device could be excluded
   *  from the scan.
   *  This function could be called multiple times to ignore
   *  multiple devices.
   *  Another option to limit the searching is to use SetDevice().
   *  @see SetDevice()
   *  @see Open()
   *
   *  @param ignore_device the device to ignore
   */
  void IgnoreDevice(const char* const ignore_device);

  /** @brief Use a specific serial device for ZeDMD
   *
   *  Instead of serching through all serial devices for a ZeDMD,
   *  just use this device.
   *  @see Open()
   *
   *  @param device the device
   */
  void SetDevice(const char* const device);

  /** @brief Open the connection to ZeDMD
   *
   *  Open a cennection to ZeDMD. Therefore all serial ports will be
   *  scanned. Use IgnoreDevice() to exclude one or more specific
   *  serial devices during that scan. Use SetDevice() to omit the
   *  the scan and to use a specific seriel device instead.
   *  @see IgnoreDevice()
   *  @see SetDevice()
   */
  bool Open();

  /** @brief Open the connection to ZeDMD
   *
   *  Backward compatibiility version of Open() which additionally
   *  sets the frame size. Use Open() and SetFrameSize() instead.
   *  @see Open()
   *  @see SetFrameSize()
   *
   *  @deprecated
   *
   *  @param width the frame width
   *  @param height the frame height
   */
  bool Open(uint16_t width, uint16_t height);

  /** @brief Open a WiFi connection to ZeDMD
   *
   *  ZeDMD could be connected via WiFi instead of USB.
   *  The WiFi settings need to be stored in ZeDMD's EEPROM
   *  first using a USB connection or via the web interface.
   *  @see Open()
   *  @see SetWiFiSSID()
   *  @see SetWiFiPassword()
   *  @see SetWiFiPort()
   *  @see SaveSettings()
   *
   *  @param ip the IPv4 address of the ZeDMD device
   */
  bool OpenWiFi(const char* ip);

  /** @brief Open default WiFi connection to ZeDMD.
   *
   *  ZeDMD could be connected via WiFi instead of USB.
   *  The WiFi settings need to be stored in ZeDMD's EEPROM.
   *  Connect to http://zedmd-wifi.local to configure the device.
   *  For the first time configuration, establish a connection to
   *  this network:
   *  SSID: ZeDMD-WiFi
   *  Password: zedmd1234
   */
  bool OpenDefaultWiFi();

  /** @brief Close connection to ZeDMD
   *
   *  Close connection to ZeDMD.
   */
  void Close();

  /** @brief Reset ZeDMD
   *
   *  Reset ZeDMD.
   */
  void Reset();

  /** @brief Set the frame size
   *
   *  Set the frame size of the content that will be displayed
   *  next on the ZeDMD device. Depending on the settings and
   *  the physical dimensions of the LED panels, the content
   *  will by centered and scaled correctly.
   *  @see EnableUpscaling()
   *  @see DisableUpscaling()
   *
   *  @param width the frame width
   *  @param height the frame height
   */
  void SetFrameSize(uint16_t width, uint16_t height);

  /** @brief Get the configured DMD width
   *
   *  Get the configured width of the DMD.
   *
   *  @return width
   */
  uint16_t const GetWidth();

  /** @brief Get the configured DMD height
   *
   *  Get the configured height of the DMD.
   *
   *  @return height
   */
  uint16_t const GetHeight();

  /** @brief Get the physical panel width
   *
   *  Get the width of the physical dimensions of the LED panels.
   *
   *  @return width
   */
  uint16_t const GetPanelWidth();

  /** @brief Get the physical panel height
   *
   *  Get the height of the physical dimensions of the LED panels.
   *
   *  @return height
   */
  uint16_t const GetPanelHeight();

  /** @brief Does ZeDMD run on an ESP32 S3?
   *
   *  On an ESP32 S3 a native USB connection is used to increase
   *  the bandwidth. Furthermore double buffering is active.
   *
   *  @return true if an ESP32 S3 is used.
   */
  bool const IsS3();

  /** @brief Get the libezedmd version
   *
   *  Get the version of the library.
   *
   *  @return version string
   */
  const char* GetVersion();

  /** @brief Get the ZeDMD firmware version
   *
   *  Get the version of the ZeDMD firmware.
   *
   *  @return version string
   */
  const char* GetFirmwareVersion();

  /** @brief Get the RGB order
   *
   *  ZeDMD supports different LED panels.
   *  Depending on the panel, the RGB order needs to be adjusted.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 0 and 5
   */
  uint8_t GetRGBOrder();

  /** @brief Get the brightness
   *
   *  Get the brightness of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 0 and 15
   */
  uint8_t GetBrightness();

  /** @brief Get the WiFi SSID
   *
   *  The WiFi SSID ZeDMD should connect with.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return the SSID
   */
  const char* GetWiFiSSID();

  /** @brief Get the WiFi Password
   *
   *  Get the WiFi Password ZeDMD should use to connect and store it internally.
   *  @see SetStoredWifiPassword()
   */
  void StoreWiFiPassword();

  /** @brief Get the WiFi Port
   *
   *  Get the Port ZeDMD should listen at over WiFi.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return the port
   */
  int GetWiFiPort();

  /** @brief Get the panel clock phase
   *
   *  Get the clock phase of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 0 and 1
   */
  uint8_t GetPanelClockPhase();

  /** @brief Get the panel i2s speed
   *
   *  Get the i2s speed of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value of 8, 16 or 20
   */
  uint8_t GetPanelI2sSpeed();

  /** @brief Get the panel latch blanking
   *
   *  Get the latch blanking of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 0 and 4
   */
  uint8_t GetPanelLatchBlanking();

  /** @brief Get the panel minimal refresh rate
   *
   *  Get the minimal refresh rate of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 30..120
   */
  uint8_t GetPanelMinRefreshRate();

  /** @brief Get the panel driver
   *
   *  Get the driver of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return  a value between 0 and 6; 0(SHIFTREG), 1(FM6124), 2(FM6126A), 3(ICN2038S), 4(MBI5124), 5(SM5266P),
   * 6(DP3246_SM5368)
   */
  uint8_t GetPanelDriver();

  /** @brief Get the transport
   *
   *  Get the transport of ZeDMD.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 0 and 3; 0(USB), 1(UDP), 2(TCP), 3(SPI)
   */
  uint8_t GetTransport();

  /** @brief Get the UDP delay
   *
   *  Get the UDP Delay.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 0 and 9
   */
  uint8_t GetUdpDelay();

  /** @brief Get the USB package size
   *
   *  Get the USB package size.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 32 and 1920
   */
  uint16_t GetUsbPackageSize();

  /** @brief Get the Y-offset of 128x64 panels
   *
   *  Get the Y-offset of 128x64 panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @return a value between 0 and 32
   */
  uint8_t GetYOffset();

  /** @brief Test the panels attached to ZeDMD
   *
   *  Renders a sequence of full red, full green and full blue frames.
   */
  void LedTest();

  /** @brief Enable debug mode
   *
   *  ZeDMD will display various debug information as overlay to
   *  the displayed frame.
   *  @see https://github.com/PPUC/ZeDMD
   */
  void EnableDebug();

  /** @brief Disable debug mode
   *
   *  @see EnableDebug()
   */
  void DisableDebug();

  /** @brief Set the RGB order
   *
   *  ZeDMD supports different LED panels.
   *  Depending on the panel, the RGB order needs to be adjusted.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param rgbOrder a value between 0 and 5
   */
  void SetRGBOrder(uint8_t rgbOrder);

  /** @brief Set the brightness
   *
   *  Set the brightness of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param brightness a value between 0 and 15
   */
  void SetBrightness(uint8_t brightness);

  /** @brief Set the WiFi SSID
   *
   *  Set the WiFi SSID ZeDMD should connect with.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param ssid the SSID
   */
  void SetWiFiSSID(const char* const ssid);

  /** @brief Set the WiFi Password
   *
   *  Set the WiFi Password ZeDMD should use to connect.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param password the password
   */
  void SetWiFiPassword(const char* const password);

  /** @brief Set the stored WiFi Password
   *
   *  Set the WiFi Password ZeDMD should use to connect form internal storage.
   *  @see StoreWiFiPassword()
   *
   *  @param password the password
   */
  void SetStoredWiFiPassword();

  /** @brief Set the WiFi Port
   *
   *  Set the Port ZeDMD should listen at over WiFi.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param port the port
   */
  void SetWiFiPort(int port);

  /** @brief Set the panel clock phase
   *
   *  Set the clock phase of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param clockPhase a value between 0 and 1
   */
  void SetPanelClockPhase(uint8_t clockPhase);

  /** @brief Set the panel i2s speed
   *
   *  Set the i2s speed of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param i2sspeed a value of 8, 16 or 20
   */
  void SetPanelI2sSpeed(uint8_t i2sSpeed);

  /** @brief Set the panel latch blanking
   *
   *  Set the latch blanking of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param latchBlanking a value between 0 and 4
   */
  void SetPanelLatchBlanking(uint8_t latchBlanking);

  /** @brief Set the panel minimal refresh rate
   *
   *  Set the minimal refresh rate of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param minRefreshRate a value between 0 and 1
   */
  void SetPanelMinRefreshRate(uint8_t minRefreshRate);

  /** @brief Set the panel driver
   *
   *  Set the driver of the LED panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param driver a value between 0 and 6; 0(SHIFTREG), 1(FM6124), 2(FM6126A), 3(ICN2038S), 4(MBI5124), 5(SM5266P),
   * 6(DP3246_SM5368)
   */
  void SetPanelDriver(uint8_t driver);

  /** @brief Set the transport
   *
   *  Set the transport of ZeDMD. Note, this is just to change ZeDMD's settings, not for connecting.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param transport a value between 0 and 3; 0(USB), 1(UDP), 2(TCP), 3(SPI)
   */
  void SetTransport(uint8_t transport);

  /** @brief Set the UDP delay
   *
   *  Set the UDP Delay.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param udpDelay a value between 0 and 9
   */
  void SetUdpDelay(uint8_t udpDelay);

  /** @brief Set the USB package size
   *
   *  Set the USB package size.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param usbPackageSize a value between 32 and 1920, but only multiple of 32
   */
  void SetUsbPackageSize(uint16_t usbPackageSize);

  /** @brief Set the Y-offset of 128x64 panels
   *
   *  Set the Y-offset of 128x64 panels.
   *  @see https://github.com/PPUC/ZeDMD
   *
   *  @param yOffset a value between 0 and 32
   */
  void SetYOffset(uint8_t yOffset);

  /** @brief Save the current setting
   *
   *  Saves all current setting within ZeDMD's EEPROM to be used
   *  as defualt at its next start.
   *  @see https://github.com/PPUC/ZeDMD
   *  @see SetRGBOrder()
   *  @see SetBrightness()
   *  @see SetWiFiSSID()
   *  @see SetWiFiPassword()
   *  @see SetWiFiPort()
   *
   *  @param brightness a value between 0 and 15
   */
  void SaveSettings();

  /** @brief Enable upscaling on the client side
   *
   *  If enabled, the content will centered and scaled up to
   *  fit into the physical dimensions of the ZeDMD panels,
   *  before the content gets send to ZeDMD, if required.
   */
  void EnableUpscaling();

  /** @brief Disable upscaling on the client side
   *
   *  @see EnableUpscaling()
   */
  void DisableUpscaling();

  /** @brief Clear the screen
   *
   *  Turn off all pixels of ZeDMD, so a blank black screen will be shown.
   */
  void ClearScreen();
  /** @brief Render a RGB24 frame
   *
   *  Renders a true color RGB frame. By default the zone streaming mode is
   *  used. The encoding is RGB888.
   *  @see DisableRGB24Streaming()
   *
   *  @param frame the RGB frame
   */
  void RenderRgb888(uint8_t* frame);

  /** @brief Render a RGB565 frame
   *
   *  Renders a true color RGB565 frame. Only zone streaming mode is supported.
   *
   *  @param frame the RGB565 frame
   */
  void RenderRgb565(uint16_t* frame);

 private:
  bool UpdateFrameBuffer888(uint8_t* pFrame);
  bool UpdateFrameBuffer565(uint16_t* pFrame);
  uint8_t GetScaleMode(uint16_t frameWidth, uint16_t frameHeight, uint8_t* pXOffset, uint8_t* pYOffset);
  int Scale888(uint8_t* pScaledFrame, uint8_t* pFrame, uint8_t bytes);
  int Scale565(uint8_t* pScaledFrame, uint16_t* pFrame, bool bigEndian);

  ZeDMDComm* m_pZeDMDComm;
  ZeDMDWiFi* m_pZeDMDWiFi;

  uint16_t m_romWidth;
  uint16_t m_romHeight;

  bool m_usb = false;
  bool m_wifi = false;
  bool m_hd = false;
  bool m_upscaling = false;

  uint8_t* m_pFrameBuffer;
  uint8_t* m_pScaledFrameBuffer;
  uint8_t* m_pRgb565Buffer;
};

#ifdef __cplusplus
extern "C"
{
#endif

  extern ZEDMDAPI ZeDMD* ZeDMD_GetInstance();
  extern ZEDMDAPI const char* ZeDMD_GetVersion();
  extern ZEDMDAPI void ZeDMD_SetLogCallback(ZeDMD* pZeDMD, ZeDMD_LogCallback callback, const void* pUserData);
  extern ZEDMDAPI const char* ZeDMD_FormatLogMessage(const char* format, va_list args, const void* pUserData);
  extern ZEDMDAPI const char* ZeDMD_GetFirmwareVersion(ZeDMD* pZeDMD);
  extern ZEDMDAPI bool ZeDMD_IsS3(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint16_t ZeDMD_GetWidth(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint16_t ZeDMD_GetHeight(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetRGBOrder(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetBrightness(ZeDMD* pZeDMD);
  extern ZEDMDAPI const char* ZeDMD_GetWiFiSSID(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_StoreWiFiPassword(ZeDMD* pZeDMD);
  extern ZEDMDAPI int ZeDMD_GetWiFiPort(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint16_t ZeDMD_GetPanelWidth(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint16_t ZeDMD_GetPanelHeight(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetPanelClockPhase(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetPanelI2sSpeed(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetPanelLatchBlanking(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetPanelMinRefreshRate(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetPanelDriver(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetTransport(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetUdpDelay(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint16_t ZeDMD_GetUsbPackageSize(ZeDMD* pZeDMD);
  extern ZEDMDAPI uint8_t ZeDMD_GetYOffset(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_IgnoreDevice(ZeDMD* pZeDMD, const char* const ignore_device);
  extern ZEDMDAPI void ZeDMD_SetDevice(ZeDMD* pZeDMD, const char* const device);
  extern ZEDMDAPI bool ZeDMD_Open(ZeDMD* pZeDMD);
  extern ZEDMDAPI bool ZeDMD_OpenWiFi(ZeDMD* pZeDMD, const char* ip);
  extern ZEDMDAPI bool ZeDMD_OpenDefaultWiFi(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_Close(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_SetLogCallback(ZeDMD* pZeDMD, ZeDMD_LogCallback callback, const void* userData);
  extern ZEDMDAPI void ZeDMD_SetFrameSize(ZeDMD* pZeDMD, uint16_t width, uint16_t height);
  extern ZEDMDAPI void ZeDMD_SetDefaultPalette(ZeDMD* pZeDMD, uint8_t bitDepth);
  extern ZEDMDAPI void ZeDMD_LedTest(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_EnableDebug(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_DisableDebug(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_SetRGBOrder(ZeDMD* pZeDMD, uint8_t rgbOrder);
  extern ZEDMDAPI void ZeDMD_SetBrightness(ZeDMD* pZeDMD, uint8_t brightness);
  extern ZEDMDAPI void ZeDMD_SaveSettings(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_EnableUpscaling(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_DisableUpscaling(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_SetWiFiSSID(ZeDMD* pZeDMD, const char* const ssid);
  extern ZEDMDAPI void ZeDMD_SetWiFiPassword(ZeDMD* pZeDMD, const char* const password);
  extern ZEDMDAPI void ZeDMD_SetWiFiPort(ZeDMD* pZeDMD, int port);
  extern ZEDMDAPI void ZeDMD_SetPanelClockPhase(ZeDMD* pZeDMD, uint8_t clockPhase);
  extern ZEDMDAPI void ZeDMD_SetPanelI2sSpeed(ZeDMD* pZeDMD, uint8_t i2sSpeed);
  extern ZEDMDAPI void ZeDMD_SetPanelLatchBlanking(ZeDMD* pZeDMD, uint8_t latchBlanking);
  extern ZEDMDAPI void ZeDMD_SetPanelMinRefreshRate(ZeDMD* pZeDMD, uint8_t minRefreshRate);
  extern ZEDMDAPI void ZeDMD_SetPanelDriver(ZeDMD* pZeDMD, uint8_t driver);
  extern ZEDMDAPI void ZeDMD_SetTransport(ZeDMD* pZeDMD, uint8_t transport);
  extern ZEDMDAPI void ZeDMD_SetUdpDelay(ZeDMD* pZeDMD, uint8_t udpDelay);
  extern ZEDMDAPI void ZeDMD_SetUsbPackageSize(ZeDMD* pZeDMD, uint16_t usbPackageSize);
  extern ZEDMDAPI void ZeDMD_SetYOffset(ZeDMD* pZeDMD, uint8_t yOffset);
  extern ZEDMDAPI void ZeDMD_ClearScreen(ZeDMD* pZeDMD);
  extern ZEDMDAPI void ZeDMD_RenderRgb888(ZeDMD* pZeDMD, uint8_t* frame);
  extern ZEDMDAPI void ZeDMD_RenderRgb565(ZeDMD* pZeDMD, uint16_t* frame);

#ifdef __cplusplus
}
#endif
