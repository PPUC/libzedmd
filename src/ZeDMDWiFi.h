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

class ZeDMDWiFi : public ZeDMDComm
{
 public:
  ZeDMDWiFi() : ZeDMDComm() {}

  virtual bool Connect(const char* name_or_ip);
  virtual void Disconnect();
  virtual bool IsConnected();

 protected:
  bool DoConnect(const char* ip);
  virtual bool SendChunks(uint8_t* pData, uint16_t size);
  virtual bool KeepAlive();
  virtual void Reset();
  bool openTcpConnection(int sock, sockaddr_in server, int16_t timeout);
  bool SendGetRequest(const std::string& path);
  bool SendPostRequest(const std::string& path, const std::string& data);
  std::string ReceiveResponse();
  int ReceiveIntegerPayload();
  const char* ReceiveStringPayload();

 private:
  int m_httpSocket = -1;
  struct sockaddr_in m_httpServer;
  sockpp::tcp_connector* m_tcpConnector = nullptr;
  bool m_connected = false;
  bool m_wsaStarted = false;
};
