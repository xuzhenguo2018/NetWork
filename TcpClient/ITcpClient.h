#ifndef _ITCPCLIENT_H_
#define _ITCPCLIENT_H_

#include "../Network.h"

class NETWORK_API ITcpClient
{
public:
    ITcpClient();
    virtual ~ITcpClient();

    virtual void OnOpen() = 0;
    virtual void OnClose() = 0;
    virtual void OnError() = 0;
	virtual int OnMessage(const string &msg) = 0;
	virtual void SendData(const string &strMsg) = 0;
};

#endif
