#include "stdafx.h"
#include "TcpClient.h"
#include "ITcpClient.h"

CTcpClient::CTcpClient()
{
	m_sClient = INVALID_SOCKET;

	m_nThreadId = 0;

	m_hThread = NULL;

	//初始化临界区
	::InitializeCriticalSectionAndSpinCount(&m_send_buff_mutex,4000);

	m_pRecvBuffer = new char[MAX_RECV_BUFF_SIZE];
	if(!m_pRecvBuffer)
	{
		throw exception("CTcpClient m_pRecvBuffer no enough memory!");	
	}

	m_pSendBuffer = new char[MAX_SEND_BUFF_SIZE];
	if(!m_pSendBuffer)
	{
		throw exception("CTcpClient m_pSendBuffer no enough memory!");		
	}

    m_RecvBytes = 0;
    m_SendBytes = 0;

	m_bRunning = false;
    m_bConnected = false;

    impl = NULL;
}

CTcpClient::~CTcpClient()
{
    Stop();

	::DeleteCriticalSection(&m_send_buff_mutex);

	if(m_pRecvBuffer)
	{
		delete []m_pRecvBuffer;
		m_pRecvBuffer = NULL;
	}
	if(m_pSendBuffer)
	{
		delete[]m_pSendBuffer;
		m_pSendBuffer = NULL;
	}
}

//直接提供一个函数来设置句柄，这样就不用初始化时候构造调用，解耦
void CTcpClient::SetHandle(ITcpClient *t)
{
    impl = t;
}

//创建socket,关闭后需要重新创建
int CTcpClient::CreateSocket()
{
	// 判断是否需要关闭套接字
    Close();    

	// 创建套节字
	m_sClient = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(m_sClient == INVALID_SOCKET)
	{
		return -1;
	}

	// 填写远程地址信息
	m_server_addr.sin_family = AF_INET;

	m_server_addr.sin_port = htons(m_serverport);
	// 注意，这里要填写服务器程序（TCPServer程序）所在机器的IP地址
	// 如果你的计算机没有联网，直接使用127.0.0.1即可
	//支持域名方式
	hostent *hoste = gethostbyname(m_serverip.c_str());
	if (NULL == hoste)
	{
		::closesocket(m_sClient);
		m_sClient = INVALID_SOCKET;
		return -1;
	}
	
	m_server_addr.sin_addr.s_addr = *((ULONG *)hoste->h_addr);

	//设置为非阻塞模式
	int mode = 1;
	::ioctlsocket(m_sClient,FIONBIO,(u_long FAR*)&mode);

	return 0;
}

//连接到服务端
int CTcpClient::Connect()
{
    if (m_sClient != INVALID_SOCKET)
    {
	    //非阻塞连接,阻塞连接返回0表示成功，-1表示失败
	    int nRet = ::connect(m_sClient, (sockaddr*)&m_server_addr, sizeof(m_server_addr));
	    if(nRet == 0)
	    {
		    //表示成功
            m_bConnected = true;
		    return 0;
	    }
	    else 
	    {
			if(WSAGetLastError() == WSAEWOULDBLOCK)
			{
				timeval timeout;
				timeout.tv_sec = 5;
				timeout.tv_usec = 0;

				fd_set fdwrite;
				FD_ZERO(&fdwrite);
				FD_SET(m_sClient,&fdwrite);

				::select(0,NULL,&fdwrite,NULL,&timeout);
				if(FD_ISSET(m_sClient,&fdwrite))
				{
					m_bConnected = true;
					return 0;
				}
			}

			::closesocket(m_sClient);
			m_sClient = INVALID_SOCKET;
			m_bConnected = false;
	    }
    }

    return -1;
}

//关闭到服务器的连接
int CTcpClient::Close()
{
	if(m_sClient != INVALID_SOCKET)
	{
		::closesocket(m_sClient);
		m_sClient = INVALID_SOCKET;

        m_RecvBytes = 0;    //关闭连接后，已接收数据清空
        m_bConnected = false;

        EnterCriticalSection(&m_send_buff_mutex);
        m_SendBytes = 0;
        LeaveCriticalSection(&m_send_buff_mutex);
	}

	return 0;
}

//重连
int CTcpClient::ReConnect()
{
	CreateSocket();
	return Connect();	
}

//启动客户端收发线程
int CTcpClient::Start(const string &strip, unsigned short port)
{
	m_serverip = strip;
    m_serverport = port;

	m_hThread = (HANDLE)CreateThread(NULL, 0, HandleDataProc, (LPVOID)this, 0, &m_nThreadId);
	return 0;
}

int CTcpClient::StartEx()
{
	m_hThread = (HANDLE)CreateThread(NULL, 0, HandleDataProcEx, (LPVOID)this, 0, &m_nThreadId);
	return 0;
}

//停止客户端收发线程
int CTcpClient::Stop()
{
    if (m_bRunning)
    {
	    m_bRunning = false;
    	
	    WaitForSingleObject(m_hThread,INFINITE);    //等待通信线程退出

	    CloseHandle(m_hThread); //关闭线程句柄

	    m_hThread = NULL;

        Close();    //关闭套接字
    }

	return 0;
}

//发送数据到服务端
//data:需要发送的数据
//length:发送数据的字节数
//成功返回length，失败返回0
int CTcpClient::Send(char *buffer, int length)
{
	//发送数据时候增加判断，如果没有建立连接，发送无意义
	if (m_bConnected)
	{
		EnterCriticalSection(&m_send_buff_mutex);
		int availLen = MAX_SEND_BUFF_SIZE - m_SendBytes;
		if (length <= availLen)
		{
			memcpy(m_pSendBuffer+m_SendBytes, buffer, length);
			m_SendBytes += length;
		}
		else
		{
			LeaveCriticalSection(&m_send_buff_mutex);
			return 0;
		}
		LeaveCriticalSection(&m_send_buff_mutex);

		return length;
	}

	return 0;
}

//数据收发函数
DWORD CTcpClient::HandleDataProc(LPVOID lpParam)
{
	CTcpClient* pThis = (CTcpClient*)lpParam;
    pThis->m_bRunning = true;

	int nRecv = 0;
	int nRet = 0;
	int nError = 0;

    string strMsg;  //日志消息，输出到日志文件
    string strRsp;  //收到服务器应答数据

    int process_len;
    int proto_len;

    bool bTO = false;
    int toCount = 0;    //超时计数

	while(pThis->m_bRunning)
	{
        //是否需要建立连接，都会触发回调
        if (INVALID_SOCKET == pThis->m_sClient)
        {
            if (bTO)    //超时状态，5秒重试一次
            {
                if (toCount >= 5)
                {
                    nRet = pThis->ReConnect();
                    if (0 == nRet)
                    {
                        if (pThis->impl)
                        {
                            pThis->impl->OnOpen();
                        }

                        bTO = false;
                        toCount = 0;
                    }
                    else
                    {
                        if (pThis->impl)
                        {
                            pThis->impl->OnError();
                        }

                        //这里修改下实现方式，不用sleep这么长时间来做超时
                        Sleep(1000);    //1秒
                        toCount = 1;
                        continue;
                    }
                }
                else
                {
                    Sleep(1000);    //1秒
                    toCount++;
                    continue;
                }
            }
            else    //不是超时重试状态
            {
                nRet = pThis->ReConnect();
                if (0 == nRet)
                {
                    if (pThis->impl)
                    {
                        pThis->impl->OnOpen();
                    }

                    bTO = false;
                    toCount = 0;
                }
                else
                {
                    if (pThis->impl)
                    {
                        pThis->impl->OnError();
                    }

                    //这里修改下实现方式，不用sleep这么长时间来做超时
                    Sleep(1000);    //1秒
                    bTO = true;
                    toCount++;
                    continue;
                }
            }
        }

	    //接收数据
        int availLen = MAX_RECV_BUFF_SIZE - pThis->m_RecvBytes;
		nRecv = ::recv(pThis->m_sClient,pThis->m_pRecvBuffer+pThis->m_RecvBytes, availLen, 0);
	    if(nRecv > 0)//接收到数据大小
	    {
            pThis->m_RecvBytes += nRecv;
            process_len = 0;
			
            //调用回调函数由用户层去处理分包
            while (pThis->m_RecvBytes > 0)
            {
                if (pThis->impl)
                {
                    strRsp.assign(pThis->m_pRecvBuffer+process_len, pThis->m_RecvBytes);
                    proto_len = pThis->impl->OnMessage(strRsp);
                    if (proto_len > 0 && proto_len <= strRsp.length())
                    {
                        process_len += proto_len;
                        pThis->m_RecvBytes -= proto_len;
                    }
                    else if (0 == proto_len)
                    {
                        break;
                    }
                    else    //出错，关闭连接
                    {
						if (pThis->impl)
						{
							pThis->impl->OnClose();
						}
                        pThis->Close();
						bTO = true;
                        break;
                    }
                }
                else    //没有设置操作函数，直接退出循环
                {
                    pThis->m_RecvBytes = 0;
                }
            }

            if (pThis->m_sClient != INVALID_SOCKET)
            {
                if (process_len > 0)
                {
                    if (pThis->m_RecvBytes > 0)
                    {
                        memmove(pThis->m_pRecvBuffer, pThis->m_pRecvBuffer+process_len, pThis->m_RecvBytes);
                    }
                }
            }
	    }
	    else if(nRecv == 0)//连接关闭
	    {
            if (pThis->impl)
            {
                pThis->impl->OnClose();
            }
		    pThis->Close();
			bTO = true;
	    }
	    else//出错
	    {
		    nError = WSAGetLastError();
		    if(nError != WSAEWOULDBLOCK && nError != WSAEINTR)
		    {
                if (pThis->impl)
                {
                    pThis->impl->OnClose();
                }
                pThis->Close();
				bTO = true;
		    }
	    }

		//---发送数据---------------------------
        if (pThis->m_sClient != INVALID_SOCKET)
        {
	        //检查发送队列是否有数据需要发送
	        if(pThis->m_SendBytes > 0)
	        {
		        EnterCriticalSection(&pThis->m_send_buff_mutex);

				nRet = ::send(pThis->m_sClient,pThis->m_pSendBuffer,pThis->m_SendBytes,0);
                if (nRet > 0)
                {
                    pThis->m_SendBytes -= nRet;

                    if (pThis->m_SendBytes > 0) //没有发送完全
                    {
                        memmove(pThis->m_pSendBuffer, pThis->m_pSendBuffer+nRet, pThis->m_SendBytes);
                    }
					LeaveCriticalSection(&pThis->m_send_buff_mutex);
                }
                else
                {
					LeaveCriticalSection(&pThis->m_send_buff_mutex);
                    nError = WSAGetLastError();
                    if(nError != WSAEWOULDBLOCK && nError != WSAEINTR)
		            {
                        if (pThis->impl)
                        {
                            pThis->impl->OnClose();
                        }
                        pThis->Close();
						bTO = true;
		            }
                }
	        }
        }

	    Sleep(10);
	}

	return 0;
}

DWORD CTcpClient::HandleDataProcEx(LPVOID lpParam)
{
	CTcpClient* pThis = (CTcpClient*)lpParam;

	int nRecv = 0;

	int nSend = 0;

	int nRet = 0;

	int nError = 0;

	//发送次数
	int nSendCnt = 0;

	//连接状态
	int nConnState = 0;

	//首先连接服务器,如果未连上继续重连直到连上
	while(pThis->m_bRunning)
	{
		pThis->Connect();
		if(pThis->m_bConnected) break;
		Sleep(100);
	}

	while(pThis->m_bRunning)
	{
		memset(pThis->m_pRecvBuffer,0,MAX_RECV_BUFF_SIZE);

		//接收数据
		nRecv = recv(pThis->m_sClient,(char*)pThis->m_pRecvBuffer,MAX_RECV_BUFF_SIZE,0);
		if(nRecv >0)//正确接收到数据
		{		
			pThis->Recv((BYTE*)pThis->m_pRecvBuffer,nRecv);
		}
		else if(nRecv == 0)
		{
			//对端关闭，启动重连
			pThis->ReConnect();
		}
		else
		{
			nError = WSAGetLastError();
			if(nError != WSAEWOULDBLOCK && nError != WSAEINTR)
			{
				pThis->ReConnect();
			}			
		}	

		//发送数据
		if (pThis->m_sClient != INVALID_SOCKET)
		{
			//检查发送队列是否有数据需要发送
			if(pThis->m_SendBytes > 0)
			{
				EnterCriticalSection(&pThis->m_send_buff_mutex);

				nRet = send(pThis->m_sClient,pThis->m_pSendBuffer,pThis->m_SendBytes,0);
				if (nRet > 0)
				{
					pThis->m_SendBytes -= nRet;

					if (pThis->m_SendBytes > 0) //没有发送完全
					{
						memmove(pThis->m_pSendBuffer, pThis->m_pSendBuffer+nRet, pThis->m_SendBytes);
					}
					LeaveCriticalSection(&pThis->m_send_buff_mutex);
				}
				else
				{
					LeaveCriticalSection(&pThis->m_send_buff_mutex);
					nError = WSAGetLastError();
					if(nError != WSAEWOULDBLOCK && nError != WSAEINTR)
					{
						pThis->ReConnect();
					}
				}
			}
		}

		Sleep(10);
	}
	return 0;
}

