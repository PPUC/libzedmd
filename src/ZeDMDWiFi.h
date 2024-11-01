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
  virtual bool DoConnect(const char* ip, int port);
  virtual bool StreamBytes(ZeDMDFrame* pFrame);
  virtual void Reset();
#if defined(_WIN32) || defined(_WIN64)
  bool StartWSA()
  {
    if (!m_wsaStarted)
    {
      WSADATA wsaData;
      m_wsaStarted = (WSAStartup(MAKEWORD(2, 2), &wsaData) == NO_ERROR);
    }
    return m_wsaStarted;
  }
#endif

 private:
  int m_wifiSocket;
  struct sockaddr_in m_wifiServer;
  bool m_connected = false;
  bool m_wsaStarted = false;
};
