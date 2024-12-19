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
  if (!m_wsaStarted)
  {
    WSADATA wsaData;
    m_wsaStarted = (WSAStartup(MAKEWORD(2, 2), &wsaData) == NO_ERROR);
  }

  if (!m_wsaStarted) return false;
#endif

  struct addrinfo hints, *res = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        // Use IPv4
  hints.ai_socktype = SOCK_STREAM;  // Use TCP for hostname resolution

  // Resolve the hostname to an IP address
  if (0 != getaddrinfo(name_or_ip, nullptr, &hints, &res))
  {
    // Try to use the IP directly if resolution fails
    return DoConnect(name_or_ip, port);
  }

  struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
  bool result = DoConnect(inet_ntoa(ipv4->sin_addr), port);
  freeaddrinfo(res);

  return result;
}

bool ZeDMDWiFi::DoConnect(const char* ip, int port)
{
  m_udpSocket = socket(AF_INET, SOCK_DGRAM, 0);  // UDP
  if (m_udpSocket < 0) return false;

  m_udpServer.sin_family = AF_INET;  // Use IPv4 and UDP
  m_udpServer.sin_port = htons(port);
  m_udpServer.sin_addr.s_addr = inet_addr(ip);

  // Check if the IP address is valid
  if (m_udpServer.sin_addr.s_addr == INADDR_NONE)
  {
#if defined(_WIN32) || defined(_WIN64)
    if (m_udpSocket >= 0) closesocket(m_udpSocket);
#else
    if (m_udpSocket >= 0) close(m_udpSocket);
#endif
    m_udpSocket = -1;

    return false;
  }

  m_connected = true;

  m_tcpServer.sin_family = AF_INET;
  m_tcpServer.sin_port = htons(80);
  m_tcpServer.sin_addr.s_addr = inet_addr(ip);

  // For whatever reason, the response of the first request could not be read. So we ask for the version string before
  // anything else.
  SendGetRequest("/get_version");

  if (SendGetRequest("/get_width")) m_width = (uint16_t)ReceiveIntegerPayload();
  if (SendGetRequest("/get_height")) m_height = (uint16_t)ReceiveIntegerPayload();
  if (SendGetRequest("/get_s3")) m_s3 = (ReceiveIntegerPayload() == 1);

  m_zoneWidth = m_width / 16;
  m_zoneHeight = m_height / 8;

  return true;
}

void ZeDMDWiFi::Disconnect()
{
#if defined(_WIN32) || defined(_WIN64)
  if (m_udpSocket >= 0) closesocket(m_udpSocket);
  if (m_tcpSocket >= 0) closesocket(m_tcpSocket);
  if (m_wsaStarted) WSACleanup();
#else
  if (m_udpSocket >= 0) close(m_udpSocket);
  if (m_tcpSocket >= 0) close(m_tcpSocket);
#endif

  m_udpSocket = -1;
  m_tcpSocket = -1;
  m_connected = false;
}

bool ZeDMDWiFi::openTcpConnection()
{
#if defined(_WIN32) || defined(_WIN64)
  if (m_tcpSocket >= 0) closesocket(m_tcpSocket);
#else
  if (m_tcpSocket >= 0) close(m_tcpSocket);
#endif

  m_tcpSocket = socket(AF_INET, SOCK_STREAM, 0);  // TCP
  if (m_tcpSocket < 0) return false;

#if defined(_WIN32) || defined(_WIN64)
  DWORD timeout = 3000;  // 3 seconds
  setsockopt(m_tcpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
  setsockopt(m_tcpSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
  struct timeval timeout;
  timeout.tv_sec = 3;   // 3 seconds
  timeout.tv_usec = 0;  // 0 microseconds
  setsockopt(m_tcpSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

  if (m_tcpServer.sin_addr.s_addr == INADDR_NONE ||
      connect(m_tcpSocket, (struct sockaddr*)&m_tcpServer, sizeof(m_tcpServer)) < 0)
  {
#if defined(_WIN32) || defined(_WIN64)
    if (m_tcpSocket >= 0) closesocket(m_tcpSocket);
#else
    if (m_tcpSocket >= 0) close(m_tcpSocket);
#endif
    m_tcpSocket = -1;

    return false;
  }

  return true;
}

bool ZeDMDWiFi::SendGetRequest(const std::string& path)
{
  if (!m_connected || !openTcpConnection()) return false;

  std::string request = "GET " + path + " HTTP/1.1\r\n";
  request += "Host: " + std::string(inet_ntoa(m_tcpServer.sin_addr)) + "\r\n";
  request += "Connection: close\r\n\r\n";

  int sentBytes = send(m_tcpSocket, request.c_str(), request.length(), 0);
  return sentBytes == request.length();
}

bool ZeDMDWiFi::SendPostRequest(const std::string& path, const std::string& data)
{
  if (!m_connected || !openTcpConnection()) return false;

  std::string request = "POST " + path + " HTTP/1.1\r\n";
  request += "Host: " + std::string(inet_ntoa(m_tcpServer.sin_addr)) + "\r\n";
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: " + std::to_string(data.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += data;

  int sentBytes = send(m_tcpSocket, request.c_str(), request.length(), 0);
  return sentBytes == request.length();
}

std::string ZeDMDWiFi::ReceiveResponse()
{
  char buffer[1024];
  std::string response;
  int bytesReceived;

  while (true)
  {
    bytesReceived = recv(m_tcpSocket, buffer, sizeof(buffer), 0);

    if (bytesReceived > 0)
    {
      response.append(buffer, bytesReceived);
      if (bytesReceived < 1024) break;
    }
    else if (bytesReceived == 0)
    {
      // Connection closed by the server
      break;
    }
    else if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
      // No data available yet, we could sleep for a while or break
      break;
    }
    else
    {
      // std::cerr << "recv failed: " << strerror(errno) << std::endl;
      break;
    }
  }

  return response;
}

int ZeDMDWiFi::ReceiveIntegerPayload()
{
  std::string response = ReceiveResponse();

  size_t headerEnd = response.find("\r\n\r\n");
  if (headerEnd == std::string::npos)
  {
    // std::cerr << "Invalid HTTP response (no headers found)." << std::endl;
    return 0;  // Or another error code
  }

  // The payload starts after "\r\n\r\n"
  std::string payload = response.substr(headerEnd + 4);

  // Convert payload to an integer
  try
  {
    int value = std::stoi(payload);
    return value;
  }
  catch (const std::invalid_argument& e)
  {
    // std::cerr << "Invalid integer payload: " << e.what() << std::endl;
    return 0;  // Or another error code
  }
  catch (const std::out_of_range& e)
  {
    // std::cerr << "Integer out of range: " << e.what() << std::endl;
    return 0;  // Or another error code
  }
}

bool ZeDMDWiFi::IsConnected() { return m_connected; }

void ZeDMDWiFi::Reset() {}

bool ZeDMDWiFi::StreamBytes(ZeDMDFrame* pFrame)
{
  // An UDP package should not exceed the MTU (WiFi rx_buffer in ESP32 is 1460
  // bytes).
  for (auto it = pFrame->data.rbegin(); it != pFrame->data.rend(); ++it)
  {
    ZeDMDFrameData frameData = *it;

    if (frameData.size < ZEDMD_COMM_FRAME_SIZE_COMMAND_LIMIT && pFrame->command != ZEDMD_COMM_COMMAND::RGB565ZonesStream)
    {
      uint8_t data[ZEDMD_COMM_FRAME_SIZE_COMMAND_LIMIT + 1] = {0};
      data[0] = pFrame->command;  // command
      if (frameData.size > 0)
      {
        memcpy(&data[1], frameData.data, frameData.size);
      }

#if defined(_WIN32) || defined(_WIN64)
      sendto(m_udpSocket, (const char*)data, frameData.size + 4, 0, (struct sockaddr*)&m_udpServer,
             sizeof(m_udpServer));
#else
      sendto(m_udpSocket, data, frameData.size + 1, 0, (struct sockaddr*)&m_udpServer, sizeof(m_udpServer));
#endif
      continue;
    }
    else
    {
      uint8_t data[ZEDMD_WIFI_MTU] = {0};
      data[0] = pFrame->command;  // command

      mz_ulong compressedSize = mz_compressBound(ZEDMD_ZONES_BYTE_LIMIT);
      int status = mz_compress(&data[1], &compressedSize, frameData.data, frameData.size);

      if (compressedSize > (ZEDMD_WIFI_MTU - 1))
      {
        Log("ZeDMD Wifi error, compressed size of %d exceeds the MTU payload of %d", compressedSize, ZEDMD_WIFI_MTU);
        return false;
      }

      if (compressedSize > ZEDMD_ZONES_BYTE_LIMIT)
      {
        Log("ZeDMD Wifi error, compressed size of %d exceeds the ZEDMD_ZONES_BYTE_LIMIT of %d", compressedSize, ZEDMD_ZONES_BYTE_LIMIT);
        return false;
      }

      if (status == MZ_OK)
      {
#if defined(_WIN32) || defined(_WIN64)
        sendto(m_udpSocket, (const char*)data, compressedSize + 4, 0, (struct sockaddr*)&m_udpServer,
               sizeof(m_udpServer));
#else
        sendto(m_udpSocket, data, compressedSize + 1, 0, (struct sockaddr*)&m_udpServer, sizeof(m_udpServer));
#endif
        continue;
      }
      else
      {
        Log("ZeDMD Wifi compression error");
        return false;
      }
    }
  }

  return true;
}
