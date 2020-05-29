#ifndef _TCPCLIENT_H_
#define _TCPCLIENT_H_

#include "../Network.h"

#define MAX_RECV_BUFF_SIZE (8*1024)
#define MAX_SEND_BUFF_SIZE (64*1024)

class ITcpClient;

class NETWORK_API CTcpClient
{
private:
	//客户端套接字
	SOCKET m_sClient;

	//服务端地址
	sockaddr_in m_server_addr;

	//数据收发线程ID
	DWORD m_nThreadId;

	//数据收发线程句柄
	HANDLE m_hThread;

	//控制线程退出
	bool m_bRunning;

    //连接是否建立
    bool m_bConnected;

	//发送缓冲区锁
	CRITICAL_SECTION m_send_buff_mutex;

	//接收缓冲区
	char *m_pRecvBuffer;
    int m_RecvBytes;

	//发送缓冲区
	char *m_pSendBuffer;
    int m_SendBytes;

	//服务器地址
	string m_serverip;

	//服务器端口
	short m_serverport;

	//指向用户层对象
    ITcpClient *impl;

public:
	CTcpClient();
	~CTcpClient();

	//设置用户层对象
    void SetHandle(ITcpClient *t);

	//服务器地址和端口操作函数
	string GetServerIp(){ return m_serverip;}
	short GetServerPort(){ return m_serverport;}
	void SetServerIp(const string &ip){ m_serverip = ip;}
	void SetServerPort(short port){ m_serverport = port;}

	//启动客户端收发线程
	int Start(const string &strip, unsigned short port);
	int StartEx();

	//停止客户端收发线程
	int Stop();

	//创建套接字，关闭后需要重新创建
	int CreateSocket();

	//连接到服务端
	int Connect();

	//关闭到服务器的连接
	int Close();

	//重连
	int ReConnect();

	//获取连接状态
	int GetConnectStatus(){return m_bConnected;}

	//发送数据到服务端
	//buffer:需要发送的数据
	//length:发送数据的字节数
	int Send(char *buffer, int length);

	//发送数据到服务端
	//buffer:接收到的数据
	//length:接收数据的字节数
	virtual int Recv(BYTE* buffer,int& length){return 0;}

private:
	//数据收发函数
	static DWORD WINAPI HandleDataProc(LPVOID lpParam);
	static DWORD WINAPI HandleDataProcEx(LPVOID lpParam);
};

#endif
