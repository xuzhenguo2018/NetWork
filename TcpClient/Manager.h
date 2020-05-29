#ifndef _MANAGER_H_
#define _MANAGER_H_

#include "ITcpClient.h"

#define MsgType_NetStateCallBack 1

enum NetStateType
{
	EnumCallBackOnOpen = 1,
	EnumCallBackOnClose = 2,
	EnumCallBackOnError = 3
};

class CTcpClient;
class CDispatch;
class CProtocolBase;

//测试管理类(同时也支持通信层事件回调)，每个管理者管理一个客户端和服务器交互，
//管理类中存储车场编号，发送的请求队列，应答队列，发包和收包个数，成功应答数和失败数
class NETWORK_API CManager : public ITcpClient
{
public:
    CManager();
    ~CManager();

	//继承接口，通信层回调时候用到
	virtual void OnOpen();
	virtual void OnClose();
	virtual void OnError();
	virtual int OnMessage(const string &msg);//分包

	//设置协议处理者
	void SetProtocalHandler(CProtocolBase *pProtocalHandler);

	//开始通信
    int Start(const string &ip, unsigned short port,UINT KeepAliveSeconds = 20);

	//停止通信
    int Stop();

	//接收数据（从上层解析协议后调用）
	void RecvData(const string &strMsg);

	//发送数据
    void SendData(const string &strMsg);

	//获取网络状态（未加锁）
	bool GetNetState();

	//获取本类实例
	static CManager *GetManager();

	//获取消息分派者对象
	CDispatch *GetDispatch();

private:
	static DWORD WINAPI Run(LPVOID lpParam);

private:
    static CManager *m_pInstance;

	bool m_bStart;
	bool m_bRunning;
    HANDLE m_thread;
    DWORD m_threadid;
	bool m_bConnected;
	UINT m_KeepAliveSeconds;

	CTcpClient *m_tcpClient;//通讯层
	CDispatch *m_pDispatch;//所有函数注册到该类来分发
	CProtocolBase *m_pProtocolHandler;//协议处理者，由上层处理

	//读写队列和临时请求队列
	list<string> m_reqQueue;
    list<string> m_rspQueue;
    list<string> m_tmpReqQueue;

	//读写锁
	CRITICAL_SECTION m_sectionRsp;
	CRITICAL_SECTION m_sectionReq;
};

NETWORK_API CManager *AfxGetMng();

#endif
