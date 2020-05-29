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

//���Թ�����(ͬʱҲ֧��ͨ�Ų��¼��ص�)��ÿ�������߹���һ���ͻ��˺ͷ�����������
//�������д洢������ţ����͵�������У�Ӧ����У��������հ��������ɹ�Ӧ������ʧ����
class NETWORK_API CManager : public ITcpClient
{
public:
    CManager();
    ~CManager();

	//�̳нӿڣ�ͨ�Ų�ص�ʱ���õ�
	virtual void OnOpen();
	virtual void OnClose();
	virtual void OnError();
	virtual int OnMessage(const string &msg);//�ְ�

	//����Э�鴦����
	void SetProtocalHandler(CProtocolBase *pProtocalHandler);

	//��ʼͨ��
    int Start(const string &ip, unsigned short port,UINT KeepAliveSeconds = 20);

	//ֹͣͨ��
    int Stop();

	//�������ݣ����ϲ����Э�����ã�
	void RecvData(const string &strMsg);

	//��������
    void SendData(const string &strMsg);

	//��ȡ����״̬��δ������
	bool GetNetState();

	//��ȡ����ʵ��
	static CManager *GetManager();

	//��ȡ��Ϣ�����߶���
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

	CTcpClient *m_tcpClient;//ͨѶ��
	CDispatch *m_pDispatch;//���к���ע�ᵽ�������ַ�
	CProtocolBase *m_pProtocolHandler;//Э�鴦���ߣ����ϲ㴦��

	//��д���к���ʱ�������
	list<string> m_reqQueue;
    list<string> m_rspQueue;
    list<string> m_tmpReqQueue;

	//��д��
	CRITICAL_SECTION m_sectionRsp;
	CRITICAL_SECTION m_sectionReq;
};

NETWORK_API CManager *AfxGetMng();

#endif
