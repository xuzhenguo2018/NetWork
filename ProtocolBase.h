#ifndef _PROTOCOL_BASE_H_
#define _PROTOCOL_BASE_H_

#include "../Network.h"

class NETWORK_API CProtocolBase
{
public:
	CProtocolBase(void);
	virtual ~CProtocolBase(void);

	//分包（由通信层回调上来的），每个子包都必须调用CManager::RecvData()
	virtual int OnSeparatePacket(const string &msg) = 0;

	//获取消息类型（由CManager回调上来的）
	virtual UINT OnGetMsgType(const string &msg) = 0;

	//获取心跳请求内容（由CManager回调上来的）
	virtual string OnGetKeepAliveRequest() = 0;
};

#endif
