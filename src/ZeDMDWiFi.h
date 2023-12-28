#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#define CALLBACK __stdcall
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define CALLBACK
#endif
#include "ZeDMDComm.h"

#define ZEDMD_WIFI_ZONES_BYTES_LIMIT 1800

class ZeDMDWiFi : public ZeDMDComm
{
public:
	ZeDMDWiFi() : ZeDMDComm()
	{
		m_zonesBytesLimit = ZEDMD_WIFI_ZONES_BYTES_LIMIT;
	}

	virtual bool Connect(const char *ip, int port);
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