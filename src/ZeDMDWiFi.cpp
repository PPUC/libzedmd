#include "ZeDMDWiFi.h"

#if defined(_WIN32) || defined(_WIN64)
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/tcp.h>
#include <unistd.h>
#endif
#include <fcntl.h>

#include <sstream>

#include "komihash/komihash.h"
#include "miniz/miniz.h"

bool ZeDMDWiFi::Connect(const char* name_or_ip)
{
#if defined(_WIN32) || defined(_WIN64)
  if (!m_wsaStarted)
  {
    WSADATA wsaData;
    m_wsaStarted = (WSAStartup(MAKEWORD(2, 2), &wsaData) == NO_ERROR);
  }

  if (!m_wsaStarted) return false;
#endif

  struct sockaddr_in sa;
  struct sockaddr_in6 sa6;
  struct addrinfo hints, *res = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        // Use IPv4
  hints.ai_socktype = SOCK_STREAM;  // Use TCP for hostname resolution

  // Check if IPv4 or IPv6
  if (inet_pton(AF_INET, name_or_ip, &(sa.sin_addr)) == 1 || inet_pton(AF_INET6, name_or_ip, &(sa6.sin6_addr)) == 1)
  {
    // We got an IP address. Use it.
    return DoConnect(name_or_ip);
  }

  // We have a hostname, resolve it to an IP address
  if (0 != getaddrinfo(name_or_ip, nullptr, &hints, &res))
  {
    return false;
  }

  struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
  bool result = DoConnect(inet_ntoa(ipv4->sin_addr));
  freeaddrinfo(res);

  return result;
}

bool ZeDMDWiFi::DoConnect(const char* ip)
{
  Log("Attempting to connect to IP address: %s", ip);

  m_httpServer.sin_family = AF_INET;
  m_httpServer.sin_port = htons(80);
  m_httpServer.sin_addr.s_addr = inet_addr(ip);

  m_port = 3333;
  m_udpDelay = 5;

  bool handshakeReceived = false;
  if (SendGetRequest("/handshake"))
  {
    handshakeReceived = true;

    std::string handshake = ReceiveStringPayload();
    std::stringstream ss(handshake);
    std::string item;

    // Log("Handshake: %s", handshake.c_str());

    for (uint8_t pos = 0; pos <= 19; pos++)
    {
      if (std::getline(ss, item, '|'))
      {
        switch (pos)
        {
          case 0:
          {
            m_width = std::stoi(item);
            break;
          }
          case 1:
          {
            m_height = std::stoi(item);
            break;
          }
          case 2:
          {
            strncpy(m_firmwareVersion, item.c_str(), sizeof(m_firmwareVersion) - 1);
            break;
          }
          case 3:
          {
            m_s3 = (std::stoi(item) == 1);
            break;
          }
          case 4:
          {
            if (strcmp("TCP", item.c_str()) == 0)
            {
              m_tcp = true;
            }
            break;
          }
          case 5:
          {
            m_port = std::stoi(item);
            break;
          }
          case 6:
          {
            m_udpDelay = std::stoi(item);
            break;
          }
          case 7:
          {
            m_writeAtOnce = std::stoi(item);
            break;
          }
          case 8:
          {
            m_brightness = std::stoi(item);
            break;
          }
          case 9:
          {
            m_rgbMode = std::stoi(item);
            break;
          }
          case 10:
          {
            m_panelClkphase = std::stoi(item);
            break;
          }
          case 11:
          {
            m_panelDriver = std::stoi(item);
            break;
          }
          case 12:
          {
            m_panelI2sspeed = std::stoi(item);
            break;
          }
          case 13:
          {
            m_panelLatchBlanking = std::stoi(item);
            break;
          }
          case 14:
          {
            m_panelMinRefreshRate = std::stoi(item);
            break;
          }
          case 15:
          {
            m_yOffset = std::stoi(item);
            break;
          }
          case 16:
          {
            strncpy(m_ssid, item.c_str(), sizeof(m_ssid) - 1);
            break;
          }
          case 17:
          {
            m_half = (std::stoi(item) == 1);
            break;
          }
          case 18:
          {
            m_id = std::stoi(item);
            break;
          }
          case 19:
          {
            m_power = std::stoi(item);
            break;
          }
        }
      }
    }
  }

  if (!handshakeReceived)
  {
    Log("ZeDMD fast handshake failed ... fallback to single requests");

    // Just get the essential things now for normal usage, not for the zedmd-client info.

    if (SendGetRequest("/get_version"))
    {
      strncpy(m_firmwareVersion, ReceiveCStringPayload(), sizeof(m_firmwareVersion) - 1);
    }
    else
    {
      Log("ZeDMD version could not be detected");
      return false;
    }

    if (SendGetRequest("/get_width"))
    {
      m_width = (uint16_t)ReceiveIntegerPayload();
    }
    else
    {
      Log("ZeDMD width could not be detected");
      return false;
    }

    if (SendGetRequest("/get_height"))
    {
      m_height = (uint16_t)ReceiveIntegerPayload();
    }
    else
    {
      Log("ZeDMD height could not be detected");
      return false;
    }

    if (SendGetRequest("/get_s3"))
    {
      m_s3 = (ReceiveIntegerPayload() == 1);
    }
    else
    {
      Log("ZeDMD ESP32 generation could not be detected");
      return false;
    }

    if (SendGetRequest("/get_protocol"))
    {
      if (strcmp("TCP", ReceiveCStringPayload()) == 0)
      {
        m_tcp = true;
      }
    }
    else
    {
      Log("ZeDMD port could not be detected");
      return false;
    }

    m_port = 3333;
    if (SendGetRequest("/get_port"))
    {
      m_port = (int)ReceiveIntegerPayload();
    }
    else
    {
      Log("ZeDMD port could not be detected");
      return false;
    }

    if (SendGetRequest("/get_udp_delay"))
    {
      m_udpDelay = (ReceiveIntegerPayload() == 1);
    }
    else
    {
      Log("ZeDMD UDP delay could not be detected, falling back to 5ms deafult");
    }
  }

  if (!(128 == m_width && 32 == m_height) && !(256 == m_width && 64 == m_height))
  {
    Log("Invalid dimensions reported from ZeDMD: %dx%d", m_width, m_height);
    return false;
  }

  m_zoneWidth = m_width / 16;
  m_zoneHeight = m_height / 8;

  Log("ZeDMD %s found: %sWiFi %s, width=%d, height=%d", m_firmwareVersion, m_s3 ? "S3 " : "", m_tcp ? "TCP" : "UDP",
      m_width, m_height);

  if (m_tcp)
  {
    m_tcpConnector = new sockpp::tcp_connector({ip, (in_port_t)m_port});
    if (!m_tcpConnector)
    {
      Log("Unable to connect ZeDMD TCP streaming port %s:%d", ip, m_port);
      return false;
    }

    int flag = 1;  // Disable Nagle's algorithm
    if (!m_tcpConnector->set_option(IPPROTO_TCP, TCP_NODELAY, flag))
    {
      Log("%s", m_tcpConnector->last_error_str().c_str());
    }
    // Set QoS (IP_TOS)
    int qos_value = 0x10;  // Low delay DSCP value
    if (!m_tcpConnector->set_option(IPPROTO_IP, IP_TOS, qos_value))
    {
      Log("%s", m_tcpConnector->last_error_str().c_str());
    }
    m_keepAliveInterval = std::chrono::milliseconds(ZEDMD_WIFI_TCP_KEEP_ALIVE_INTERVAL);
  }
  else
  {
    m_udpSocket = new sockpp::udp_socket();
    m_udpServer = new sockpp::inet_address(ntohl(m_httpServer.sin_addr.s_addr), (in_port_t)m_port);
    m_keepAliveInterval = std::chrono::milliseconds(ZEDMD_WIFI_UDP_KEEP_ALIVE_INTERVAL);
  }

  strcpy(m_ip, ip);
  m_connected = true;

  return true;
}

void ZeDMDWiFi::Disconnect()
{
#if defined(_WIN32) || defined(_WIN64)
  if (m_httpSocket >= 0) closesocket(m_httpSocket);
  if (m_wsaStarted) WSACleanup();
#else
  if (m_httpSocket >= 0) close(m_httpSocket);
#endif
  m_httpSocket = -1;

  if (m_tcpConnector)
  {
    delete m_tcpConnector;
  }
  m_tcpConnector = nullptr;

  if (m_udpServer)
  {
    delete m_udpServer;
  }
  m_udpServer = nullptr;

  if (m_udpSocket)
  {
    delete m_udpSocket;
  }
  m_udpSocket = nullptr;

  m_connected = false;
}

bool ZeDMDWiFi::openTcpConnection()
{
  // The timeout of the operating system could be very long, like one minute or more.
  // Unfortunately, connect() ignores the shorter timeout we configure on the socket and uses the one from the operating
  // system if in blocking mode. So we need that complicated code to turn the socket in a non-blocking mode first and
  // perform a connect() with a short timout to see if ZeDMD WiFi is "online". If the connection could be established,
  // the socket will be closed and established again in a blocking mode. This way we get the information if ZeDMD is
  // "offline" within 3 seconds.

  if (m_httpSocket >= 0)
  {
#if defined(_WIN32) || defined(_WIN64)
    closesocket(m_httpSocket);
#else
    close(m_httpSocket);
#endif
  }
  else
  {
    m_httpSocket = socket(AF_INET, SOCK_STREAM, 0);  // TCP
    if (m_httpSocket < 0) return false;

    // Set socket to non-blocking mode
#if defined(_WIN32) || defined(_WIN64)
    u_long mode = 1;
    ioctlsocket(m_httpSocket, FIONBIO, &mode);
#else
    int flags = fcntl(m_httpSocket, F_GETFL, 0);
    fcntl(m_httpSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    // Start connect
    int result = connect(m_httpSocket, (struct sockaddr*)&m_httpServer, sizeof(m_httpServer));
    if (result != 0)
    {
      // Not connected immediately
#if defined(_WIN32) || defined(_WIN64)
      if (WSAGetLastError() != WSAEWOULDBLOCK)
      {
        if (m_httpSocket >= 0) closesocket(m_httpSocket);
        m_httpSocket = -1;
        return false;  // Immediate error
      }
#else
      if (errno != EINPROGRESS)
      {
        Log("Socket error: %d, %s", errno, strerror(errno));
        if (m_httpSocket >= 0) close(m_httpSocket);
        m_httpSocket = -1;
        return false;  // Immediate error
      }
#endif

      // Wait for the socket to become writable
      fd_set writefds;
      FD_ZERO(&writefds);
      FD_SET(m_httpSocket, &writefds);

      struct timeval tv;
      tv.tv_sec = 3;
      tv.tv_usec = 0;

      if (select(m_httpSocket + 1, nullptr, &writefds, nullptr, &tv) < 1)
      {
#if defined(_WIN32) || defined(_WIN64)
        if (m_httpSocket >= 0) closesocket(m_httpSocket);
#else
        if (m_httpSocket >= 0) close(m_httpSocket);
#endif
        m_httpSocket = -1;
        return false;  // Timeout or error
      }
    }

// Restore blocking mode
#if defined(_WIN32) || defined(_WIN64)
    mode = 0;
    ioctlsocket(m_httpSocket, FIONBIO, &mode);
    closesocket(m_httpSocket);
#else
    fcntl(m_httpSocket, F_SETFL, flags);
    close(m_httpSocket);
#endif
  }

  m_httpSocket = socket(AF_INET, SOCK_STREAM, 0);  // TCP
  if (m_httpSocket < 0) return false;

#if defined(_WIN32) || defined(_WIN64)
  DWORD to = 3000;
  setsockopt(m_httpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));
  setsockopt(m_httpSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&to, sizeof(to));
#else
  struct timeval to;
  to.tv_sec = 3;
  to.tv_usec = 0;  // 0 microseconds
  if (setsockopt(m_httpSocket, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) != 0)
  {
    Log("Socket error: %d, %s", errno, strerror(errno));
  }
  if (setsockopt(m_httpSocket, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to)) != 0)
  {
    Log("Socket error: %d, %s", errno, strerror(errno));
  }
#endif

  if (m_httpServer.sin_addr.s_addr == INADDR_NONE ||
      connect(m_httpSocket, (struct sockaddr*)&m_httpServer, sizeof(m_httpServer)) < 0)
  {
    Log("Failed to connect: %s", strerror(errno));

#if defined(_WIN32) || defined(_WIN64)
    if (m_httpSocket >= 0) closesocket(m_httpSocket);
#else
    if (m_httpSocket >= 0) close(m_httpSocket);
#endif
    m_httpSocket = -1;

    return false;
  }

  return true;
}

bool ZeDMDWiFi::SendGetRequest(const std::string& path)
{
  if (!openTcpConnection()) return false;

  std::string request = "GET " + path + " HTTP/1.1\r\n";
  request += "Host: " + std::string(inet_ntoa(m_httpServer.sin_addr)) + "\r\n";
  request += "Connection: close\r\n\r\n";

  int sentBytes = send(m_httpSocket, request.c_str(), request.length(), 0);
  return sentBytes == request.length();
}

bool ZeDMDWiFi::SendPostRequest(const std::string& path, const std::string& data)
{
  if (!openTcpConnection()) return false;

  std::string request = "POST " + path + " HTTP/1.1\r\n";
  request += "Host: " + std::string(inet_ntoa(m_httpServer.sin_addr)) + "\r\n";
  request += "Content-Type: application/x-www-form-urlencoded\r\n";
  request += "Content-Length: " + std::to_string(data.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += data;

  int sentBytes = send(m_httpSocket, request.c_str(), request.length(), 0);
  return sentBytes == request.length();
}

std::string ZeDMDWiFi::ReceiveResponse()
{
  char buffer[1024];
  std::string response;
  int bytesReceived;

  while (true)
  {
    bytesReceived = recv(m_httpSocket, buffer, sizeof(buffer), 0);
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

std::string ZeDMDWiFi::ReceiveStringPayload()
{
  std::string response = ReceiveResponse();

  size_t headerEnd = response.find("\r\n\r\n");
  if (headerEnd == std::string::npos)
  {
    // std::cerr << "Invalid HTTP response (no headers found)." << std::endl;
    return "";  // Or another error code
  }

  // The payload starts after "\r\n\r\n"
  return response.substr(headerEnd + 4);
}

const char* ZeDMDWiFi::ReceiveCStringPayload() { return ReceiveStringPayload().c_str(); }

bool ZeDMDWiFi::IsConnected() { return m_connected; }

void ZeDMDWiFi::Reset() {}

bool ZeDMDWiFi::SendChunks(uint8_t* pData, uint16_t size)
{
  if (m_tcp)
  {
    if (m_tcpConnector->write_n(pData, size) < 0)
    {
      Log("TCP stream error: %s", m_tcpConnector->last_error_str().c_str());
      m_fullFrameFlag.store(true, std::memory_order_release);
      return false;
    }
  }
  else
  {
    int status = 0;
    uint16_t sent = 0;

    while (sent < size && !m_stopFlag.load(std::memory_order_relaxed))
    {
      int toSend = ((size - sent) < ZEDMD_WIFI_UDP_CHUNK_SIZE) ? size - sent : ZEDMD_WIFI_UDP_CHUNK_SIZE;
      status = m_udpSocket->send_to(&pData[sent], (size_t)toSend, *m_udpServer);
      if (status < toSend)
      {
        Log("UDP stream error: %s", m_udpSocket->last_error_str().c_str());
        m_fullFrameFlag.store(true, std::memory_order_release);
        return false;
      }
      sent += status;
      // ESP32 crashes if too many packages arrive in a short time, mainly in FlexDMD tables.
      // Even if the firmware has some protections and should ignore some packages, it crashes.
      // It seems like a bug in AsyncUDP. At the moment, only that delay seems to avoid the crash.
      std::this_thread::sleep_for(std::chrono::milliseconds(m_udpDelay));
    }
  }

  return true;
}

uint8_t ZeDMDWiFi::GetTransport() { return m_tcp ? 2 : 1; }

const char* ZeDMDWiFi::GetWiFiSSID() { return (const char*)m_ssid; }

void ZeDMDWiFi::StoreWiFiPassword() {}

int ZeDMDWiFi::GetWiFiPort() { return m_port; };

uint8_t ZeDMDWiFi::GetWiFiPower() { return m_power; };
