#pragma once

#include "ZeDMDComm.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#define ZEDMD_WIFI_ZONES_BYTES_LIMIT 1600

class ZeDMDWiFi : public ZeDMDComm
{
 public:
  ZeDMDWiFi() : ZeDMDComm() { m_zonesBytesLimit = ZEDMD_WIFI_ZONES_BYTES_LIMIT; }

  virtual bool Connect(const char* name_or_ip, int port);
  virtual void Disconnect();
  virtual bool IsConnected();

 protected:
  bool DoConnect(const char* ip, int port);
  virtual bool StreamBytes(ZeDMDFrame* pFrame);
  virtual void Reset();
  bool SendGetRequest(const std::string& path);
  bool SendPostRequest(const std::string& path, const std::string& data);
  std::string ReceiveResponse();
  uint8_t ReceiveBytePayload();

 private:
  int m_udpSocket = -1;
  int m_tcpSocket = -1;
  struct sockaddr_in m_udpServer;
  struct sockaddr_in m_tcpServer;
  bool m_connected = false;
  bool m_wsaStarted = false;
};
