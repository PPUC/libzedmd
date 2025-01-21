#include "ZeDMDWiFi.h"

#if defined(_WIN32) || defined(_WIN64)
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <unistd.h>
#endif
#include <netinet/tcp.h>

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

  struct addrinfo hints, *res = nullptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        // Use IPv4
  hints.ai_socktype = SOCK_STREAM;  // Use TCP for hostname resolution

  // Resolve the hostname to an IP address
  if (0 != getaddrinfo(name_or_ip, nullptr, &hints, &res))
  {
    // Try to use the IP directly if resolution fails
    return DoConnect(name_or_ip);
  }

  struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
  bool result = DoConnect(inet_ntoa(ipv4->sin_addr));
  freeaddrinfo(res);

  return result;
}

bool ZeDMDWiFi::DoConnect(const char* ip)
{
  Log("Attempting to connect to IP: %s", ip);

  m_httpSocket = socket(AF_INET, SOCK_STREAM, 0);  // TCP

  m_httpServer.sin_family = AF_INET;
  m_httpServer.sin_port = htons(80);
  m_httpServer.sin_addr.s_addr = inet_addr(ip);

  if (SendGetRequest("/get_version"))
  {
    strncpy(m_firmwareVersion, ReceiveStringPayload(), sizeof(m_firmwareVersion) - 1);
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

  int port = 3333;
  if (SendGetRequest("/get_port"))
  {
    port = (int)ReceiveIntegerPayload();
  }
  else
  {
    Log("ZeDMD port could not be detected");
    return false;
  }

  if ((m_width != 128 && m_width != 256) || (m_height != 32 && m_height != 64))
  {
    Log("Invalid dimensions reported from ZeDMD: %dx%d", m_width, m_height);
    return false;
  }

  m_zoneWidth = m_width / 16;
  m_zoneHeight = m_height / 8;

  Log("ZeDMD %s found: %sWiFi, width=%d, height=%d", m_firmwareVersion, m_s3 ? "S3 " : "", m_width, m_height);

  m_tcpConnector = new sockpp::tcp_connector({ip, (in_port_t)port});
  if (!m_tcpConnector)
  {
    Log("Unable to connect ZeDMD TCP streaming port %s:%d", ip, port);
    return false;
  }

  int flag = 1;  // Disable Nagle's algorithm
  m_tcpConnector->set_option(IPPROTO_TCP, TCP_NODELAY, flag);

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
    m_tcpConnector->close();
    delete m_tcpConnector;
  }
  m_tcpConnector = nullptr;
  m_connected = false;
}

bool ZeDMDWiFi::openTcpConnection(int sock, sockaddr_in server, int16_t timeout)
{
#if defined(_WIN32) || defined(_WIN64)
  if (sock >= 0) closesocket(sock);
#else
  if (sock >= 0) close(sock);
#endif

  sock = socket(AF_INET, SOCK_STREAM, 0);  // TCP
  if (sock < 0) return false;

#if defined(_WIN32) || defined(_WIN64)
  DWORD to = timeout;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&to, sizeof(to));
#else
  struct timeval to;
  to.tv_sec = timeout / 1000;
  to.tv_usec = 0;  // 0 microseconds
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) != 0)
  {
    Log("Socket error: %s", strerror(errno));
  }
#endif

  if (server.sin_addr.s_addr == INADDR_NONE || connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
  {
    Log("Failed to connect: %s", strerror(errno));

#if defined(_WIN32) || defined(_WIN64)
    if (sock >= 0) closesocket(sock);
#else
    if (sock >= 0) close(sock);
#endif
    sock = -1;

    return false;
  }

  return true;
}

bool ZeDMDWiFi::SendGetRequest(const std::string& path)
{
  if (!openTcpConnection(m_httpSocket, m_httpServer, 3000)) return false;

  std::string request = "GET " + path + " HTTP/1.1\r\n";
  request += "Host: " + std::string(inet_ntoa(m_httpServer.sin_addr)) + "\r\n";
  request += "Connection: close\r\n\r\n";

  int sentBytes = send(m_httpSocket, request.c_str(), request.length(), 0);
  return sentBytes == request.length();
}

bool ZeDMDWiFi::SendPostRequest(const std::string& path, const std::string& data)
{
  if (!openTcpConnection(m_httpSocket, m_httpServer, 3000)) return false;

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

const char* ZeDMDWiFi::ReceiveStringPayload()
{
  std::string response = ReceiveResponse();

  size_t headerEnd = response.find("\r\n\r\n");
  if (headerEnd == std::string::npos)
  {
    // std::cerr << "Invalid HTTP response (no headers found)." << std::endl;
    return "";  // Or another error code
  }

  // The payload starts after "\r\n\r\n"
  std::string payload = response.substr(headerEnd + 4);

  return payload.c_str();
}

bool ZeDMDWiFi::IsConnected() { return m_connected; }

void ZeDMDWiFi::Reset() {}

bool ZeDMDWiFi::KeepAlive()
{
  static auto lastRun = std::chrono::steady_clock::now();
  const auto minInterval = std::chrono::milliseconds(100);

  auto now = std::chrono::steady_clock::now();
  if (now - lastRun < minInterval)
  {
    // Skip this call
    return true;
  }

  lastRun = now;

  uint16_t size = CTRL_CHARS_HEADER_SIZE + 3;
  uint8_t* pData = (uint8_t*)malloc(size);
  memcpy(pData, CTRL_CHARS_HEADER, CTRL_CHARS_HEADER_SIZE);
  pData[CTRL_CHARS_HEADER_SIZE] = ZEDMD_COMM_COMMAND::KeepAlive;
  pData[CTRL_CHARS_HEADER_SIZE + 1] = 0;
  pData[CTRL_CHARS_HEADER_SIZE + 2] = 0;
  SendChunks(pData, size);
  free(pData);

  return true;
}

bool ZeDMDWiFi::SendChunks(uint8_t* pData, uint16_t size)
{
  if (m_tcpConnector->write_n(pData, size) < 0)
  {
    Log("TCP stream error: %s", m_tcpConnector->last_error_str().c_str());
    return false;
  }

  return true;
}
