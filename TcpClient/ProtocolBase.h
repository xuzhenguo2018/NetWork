#ifndef _PROTOCOL_BASE_H_
#define _PROTOCOL_BASE_H_

#include "../Network.h"

class NETWORK_API CProtocolBase
{
public:
	CProtocolBase(void);
	virtual ~CProtocolBase(void);

	//�ְ�����ͨ�Ų�ص������ģ���ÿ���Ӱ����������CManager::RecvData()
	virtual int OnSeparatePacket(const string &msg) = 0;

	//��ȡ��Ϣ���ͣ���CManager�ص������ģ�
	virtual UINT OnGetMsgType(const string &msg) = 0;

	//��ȡ�����������ݣ���CManager�ص������ģ�
	virtual string OnGetKeepAliveRequest() = 0;
};

#endif
