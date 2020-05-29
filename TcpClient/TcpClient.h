#ifndef _TCPCLIENT_H_
#define _TCPCLIENT_H_

#include "../Network.h"

#define MAX_RECV_BUFF_SIZE (8*1024)
#define MAX_SEND_BUFF_SIZE (64*1024)

class ITcpClient;

class NETWORK_API CTcpClient
{
private:
	//�ͻ����׽���
	SOCKET m_sClient;

	//����˵�ַ
	sockaddr_in m_server_addr;

	//�����շ��߳�ID
	DWORD m_nThreadId;

	//�����շ��߳̾��
	HANDLE m_hThread;

	//�����߳��˳�
	bool m_bRunning;

    //�����Ƿ���
    bool m_bConnected;

	//���ͻ�������
	CRITICAL_SECTION m_send_buff_mutex;

	//���ջ�����
	char *m_pRecvBuffer;
    int m_RecvBytes;

	//���ͻ�����
	char *m_pSendBuffer;
    int m_SendBytes;

	//��������ַ
	string m_serverip;

	//�������˿�
	short m_serverport;

	//ָ���û������
    ITcpClient *impl;

public:
	CTcpClient();
	~CTcpClient();

	//�����û������
    void SetHandle(ITcpClient *t);

	//��������ַ�Ͷ˿ڲ�������
	string GetServerIp(){ return m_serverip;}
	short GetServerPort(){ return m_serverport;}
	void SetServerIp(const string &ip){ m_serverip = ip;}
	void SetServerPort(short port){ m_serverport = port;}

	//�����ͻ����շ��߳�
	int Start(const string &strip, unsigned short port);
	int StartEx();

	//ֹͣ�ͻ����շ��߳�
	int Stop();

	//�����׽��֣��رպ���Ҫ���´���
	int CreateSocket();

	//���ӵ������
	int Connect();

	//�رյ�������������
	int Close();

	//����
	int ReConnect();

	//��ȡ����״̬
	int GetConnectStatus(){return m_bConnected;}

	//�������ݵ������
	//buffer:��Ҫ���͵�����
	//length:�������ݵ��ֽ���
	int Send(char *buffer, int length);

	//�������ݵ������
	//buffer:���յ�������
	//length:�������ݵ��ֽ���
	virtual int Recv(BYTE* buffer,int& length){return 0;}

private:
	//�����շ�����
	static DWORD WINAPI HandleDataProc(LPVOID lpParam);
	static DWORD WINAPI HandleDataProcEx(LPVOID lpParam);
};

#endif
