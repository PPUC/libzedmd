#include "ZeDMDWiFi.h"

#if defined(_WIN32) || defined(_WIN64)
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <unistd.h>
#endif
#include "komihash/komihash.h"
#include "miniz/miniz.h"

bool ZeDMDWiFi::Connect(const char* name_or_ip, int port)
{
#if defined(_WIN32) || defined(_WIN64)
  if (!StartWSA()) return false;
#endif

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        // Use IPv4
  hints.ai_socktype = SOCK_STREAM;  // Use TCP

  // Resolve the hostname to an IP address
  if (0 != getaddrinfo(name_or_ip, nullptr, &hints, &res))
  {
    return DoConnect(name_or_ip, port);
  }

  struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
  return DoConnect(inet_ntoa(ipv4->sin_addr), port);
}

bool ZeDMDWiFi::DoConnect(const char* ip, int port)
{
  if ((m_wifiSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    return false;
  }

  m_wifiServer.sin_family = AF_INET;
  m_wifiServer.sin_port = htons(port);
  m_wifiServer.sin_addr.s_addr = inet_addr(ip);
  m_connected = true;

  return true;
}

void ZeDMDWiFi::Disconnect()
{
#if defined(_WIN32) || defined(_WIN64)
  closesocket(m_wifiSocket);
  WSACleanup();
#else
  close(m_wifiSocket);
#endif

  m_connected = false;
}

bool ZeDMDWiFi::IsConnected() { return m_connected; }

void ZeDMDWiFi::Reset() {}

bool ZeDMDWiFi::StreamBytes(ZeDMDFrame* pFrame)
{
  // An UDP package should not exceed the MTU (WiFi rx_buffer in ESP32 is 1460
  // bytes). We send 4 zones of 16 * 8 pixels: 512 pixels, 3 bytes RGB per
  // pixel, 4 bytes for zones index, 4 bytes command header: 1544 bytes As we
  // additionally use compression, that should be safe.

  if (pFrame->size < ZEDMD_COMM_FRAME_SIZE_COMMAND_LIMIT)
  {
    uint8_t data[ZEDMD_COMM_FRAME_SIZE_COMMAND_LIMIT + 4] = {0};
    data[0] = pFrame->command;  // command
    data[1] = 0;                // not compressed
    data[2] = (uint8_t)(pFrame->size >> 8 & 0xFF);
    data[3] = (uint8_t)(pFrame->size & 0xFF);
    if (pFrame->size > 0)
    {
      memcpy(&data[4], pFrame->data, pFrame->size);
    }

    // Send command three times in case an UDP packet gets lost.
    for (int i = 0; i < 3; i++)
    {
#if defined(_WIN32) || defined(_WIN64)
      sendto(m_wifiSocket, (const char*)data, pFrame->size + 4, 0, (struct sockaddr*)&m_wifiServer,
             sizeof(m_wifiServer));
#else
      sendto(m_wifiSocket, data, pFrame->size + 4, 0, (struct sockaddr*)&m_wifiServer, sizeof(m_wifiServer));
#endif
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
  }
  else
  {
    // If RGB565 streaming is active; the frame only needs 2 bytes per color
    uint8_t bytesPerPixel = (pFrame->command == 5) ? 2 : 3;

    uint8_t data[ZEDMD_WIFI_ZONES_BYTES_LIMIT] = {0};
    data[0] = pFrame->command;  // command
    data[1] =
        (uint8_t)(128 | (pFrame->size / (m_zoneWidth * m_zoneHeight * bytesPerPixel + 1)));  // compressed + zone index

    mz_ulong compressedSize = mz_compressBound(pFrame->size);
    int status = mz_compress(&data[4], &compressedSize, pFrame->data, pFrame->size);
    data[2] = (uint8_t)(compressedSize >> 8 & 0xFF);
    data[3] = (uint8_t)(compressedSize & 0xFF);

    if (status == MZ_OK)
    {
#if defined(_WIN32) || defined(_WIN64)
      sendto(m_wifiSocket, (const char*)data, compressedSize + 4, 0, (struct sockaddr*)&m_wifiServer,
             sizeof(m_wifiServer));
#else
      sendto(m_wifiSocket, data, compressedSize + 4, 0, (struct sockaddr*)&m_wifiServer, sizeof(m_wifiServer));
#endif
      return true;
    }
  }

  return false;
}
