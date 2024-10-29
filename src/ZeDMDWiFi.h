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

  virtual bool Connect(const char* ip, int port);
  virtual bool Connect(int port);
  virtual void Disconnect();
  virtual bool IsConnected();

 protected:
  virtual bool StreamBytes(ZeDMDFrame* pFrame);
  virtual void Reset();

 private:
  int m_wifiSocket;
  struct sockaddr_in m_wifiServer;
  bool m_connected = false;
};
