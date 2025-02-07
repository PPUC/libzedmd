#pragma once

#include "ZeDMDComm.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include "sockpp/tcp_connector.h"
#include "sockpp/udp_socket.h"

#define ZEDMD_WIFI_TCP_KEEP_ALIVE_INTERVAL 100
#define ZEDMD_WIFI_UDP_KEEP_ALIVE_INTERVAL 3000
#define ZEDMD_WIFI_UDP_CHUNK_SIZE 1400

class ZeDMDWiFi : public ZeDMDComm
{
 public:
  ZeDMDWiFi() : ZeDMDComm() {}

  virtual bool Connect(const char* name_or_ip);
  virtual void Disconnect();
  virtual bool IsConnected();
  virtual uint8_t GetTransport();
  virtual const char* GetWiFiSSID();
  virtual void StoreWiFiPassword();
  virtual int GetWiFiPort();

 protected:
  bool DoConnect(const char* ip);
  virtual bool SendChunks(uint8_t* pData, uint16_t size);
  virtual void Reset();
  bool openTcpConnection(int sock, sockaddr_in server, int16_t timeout);
  bool SendGetRequest(const std::string& path);
  bool SendPostRequest(const std::string& path, const std::string& data);
  std::string ReceiveResponse();
  int ReceiveIntegerPayload();
  const char* ReceiveCStringPayload();
  std::string ReceiveStringPayload();

 private:
  char m_ssid[32] = {0};
  int m_httpSocket = -1;
  int m_port = -1;
  struct sockaddr_in m_httpServer;
  sockpp::udp_socket* m_udpSocket = nullptr;
  sockpp::inet_address* m_udpServer = nullptr;
  sockpp::tcp_connector* m_tcpConnector = nullptr;
  bool m_connected = false;
  bool m_tcp = false;
  bool m_wsaStarted = false;
};
