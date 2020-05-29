#include "stdafx.h"
#include "TcpClient.h"
#include "ITcpClient.h"

CTcpClient::CTcpClient()
{
	m_sClient = INVALID_SOCKET;

	m_nThreadId = 0;

	m_hThread = NULL;

	//��ʼ���ٽ���
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

//ֱ���ṩһ�����������þ���������Ͳ��ó�ʼ��ʱ������ã�����
void CTcpClient::SetHandle(ITcpClient *t)
{
    impl = t;
}

//����socket,�رպ���Ҫ���´���
int CTcpClient::CreateSocket()
{
	// �ж��Ƿ���Ҫ�ر��׽���
    Close();    

	// �����׽���
	m_sClient = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(m_sClient == INVALID_SOCKET)
	{
		return -1;
	}

	// ��дԶ�̵�ַ��Ϣ
	m_server_addr.sin_family = AF_INET;

	m_server_addr.sin_port = htons(m_serverport);
	// ע�⣬����Ҫ��д����������TCPServer�������ڻ�����IP��ַ
	// �����ļ����û��������ֱ��ʹ��127.0.0.1����
	//֧��������ʽ
	hostent *hoste = gethostbyname(m_serverip.c_str());
	if (NULL == hoste)
	{
		::closesocket(m_sClient);
		m_sClient = INVALID_SOCKET;
		return -1;
	}
	
	m_server_addr.sin_addr.s_addr = *((ULONG *)hoste->h_addr);

	//����Ϊ������ģʽ
	int mode = 1;
	::ioctlsocket(m_sClient,FIONBIO,(u_long FAR*)&mode);

	return 0;
}

//���ӵ������
int CTcpClient::Connect()
{
    if (m_sClient != INVALID_SOCKET)
    {
	    //����������,�������ӷ���0��ʾ�ɹ���-1��ʾʧ��
	    int nRet = ::connect(m_sClient, (sockaddr*)&m_server_addr, sizeof(m_server_addr));
	    if(nRet == 0)
	    {
		    //��ʾ�ɹ�
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

//�رյ�������������
int CTcpClient::Close()
{
	if(m_sClient != INVALID_SOCKET)
	{
		::closesocket(m_sClient);
		m_sClient = INVALID_SOCKET;

        m_RecvBytes = 0;    //�ر����Ӻ��ѽ����������
        m_bConnected = false;

        EnterCriticalSection(&m_send_buff_mutex);
        m_SendBytes = 0;
        LeaveCriticalSection(&m_send_buff_mutex);
	}

	return 0;
}

//����
int CTcpClient::ReConnect()
{
	CreateSocket();
	return Connect();	
}

//�����ͻ����շ��߳�
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

//ֹͣ�ͻ����շ��߳�
int CTcpClient::Stop()
{
    if (m_bRunning)
    {
	    m_bRunning = false;
    	
	    WaitForSingleObject(m_hThread,INFINITE);    //�ȴ�ͨ���߳��˳�

	    CloseHandle(m_hThread); //�ر��߳̾��

	    m_hThread = NULL;

        Close();    //�ر��׽���
    }

	return 0;
}

//�������ݵ������
//data:��Ҫ���͵�����
//length:�������ݵ��ֽ���
//�ɹ�����length��ʧ�ܷ���0
int CTcpClient::Send(char *buffer, int length)
{
	//��������ʱ�������жϣ����û�н������ӣ�����������
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

//�����շ�����
DWORD CTcpClient::HandleDataProc(LPVOID lpParam)
{
	CTcpClient* pThis = (CTcpClient*)lpParam;
    pThis->m_bRunning = true;

	int nRecv = 0;
	int nRet = 0;
	int nError = 0;

    string strMsg;  //��־��Ϣ���������־�ļ�
    string strRsp;  //�յ�������Ӧ������

    int process_len;
    int proto_len;

    bool bTO = false;
    int toCount = 0;    //��ʱ����

	while(pThis->m_bRunning)
	{
        //�Ƿ���Ҫ�������ӣ����ᴥ���ص�
        if (INVALID_SOCKET == pThis->m_sClient)
        {
            if (bTO)    //��ʱ״̬��5������һ��
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

                        //�����޸���ʵ�ַ�ʽ������sleep��ô��ʱ��������ʱ
                        Sleep(1000);    //1��
                        toCount = 1;
                        continue;
                    }
                }
                else
                {
                    Sleep(1000);    //1��
                    toCount++;
                    continue;
                }
            }
            else    //���ǳ�ʱ����״̬
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

                    //�����޸���ʵ�ַ�ʽ������sleep��ô��ʱ��������ʱ
                    Sleep(1000);    //1��
                    bTO = true;
                    toCount++;
                    continue;
                }
            }
        }

	    //��������
        int availLen = MAX_RECV_BUFF_SIZE - pThis->m_RecvBytes;
		nRecv = ::recv(pThis->m_sClient,pThis->m_pRecvBuffer+pThis->m_RecvBytes, availLen, 0);
	    if(nRecv > 0)//���յ����ݴ�С
	    {
            pThis->m_RecvBytes += nRecv;
            process_len = 0;
			
            //���ûص��������û���ȥ����ְ�
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
                    else    //�����ر�����
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
                else    //û�����ò���������ֱ���˳�ѭ��
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
	    else if(nRecv == 0)//���ӹر�
	    {
            if (pThis->impl)
            {
                pThis->impl->OnClose();
            }
		    pThis->Close();
			bTO = true;
	    }
	    else//����
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

		//---��������---------------------------
        if (pThis->m_sClient != INVALID_SOCKET)
        {
	        //��鷢�Ͷ����Ƿ���������Ҫ����
	        if(pThis->m_SendBytes > 0)
	        {
		        EnterCriticalSection(&pThis->m_send_buff_mutex);

				nRet = ::send(pThis->m_sClient,pThis->m_pSendBuffer,pThis->m_SendBytes,0);
                if (nRet > 0)
                {
                    pThis->m_SendBytes -= nRet;

                    if (pThis->m_SendBytes > 0) //û�з�����ȫ
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

	//���ʹ���
	int nSendCnt = 0;

	//����״̬
	int nConnState = 0;

	//�������ӷ�����,���δ���ϼ�������ֱ������
	while(pThis->m_bRunning)
	{
		pThis->Connect();
		if(pThis->m_bConnected) break;
		Sleep(100);
	}

	while(pThis->m_bRunning)
	{
		memset(pThis->m_pRecvBuffer,0,MAX_RECV_BUFF_SIZE);

		//��������
		nRecv = recv(pThis->m_sClient,(char*)pThis->m_pRecvBuffer,MAX_RECV_BUFF_SIZE,0);
		if(nRecv >0)//��ȷ���յ�����
		{		
			pThis->Recv((BYTE*)pThis->m_pRecvBuffer,nRecv);
		}
		else if(nRecv == 0)
		{
			//�Զ˹رգ���������
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

		//��������
		if (pThis->m_sClient != INVALID_SOCKET)
		{
			//��鷢�Ͷ����Ƿ���������Ҫ����
			if(pThis->m_SendBytes > 0)
			{
				EnterCriticalSection(&pThis->m_send_buff_mutex);

				nRet = send(pThis->m_sClient,pThis->m_pSendBuffer,pThis->m_SendBytes,0);
				if (nRet > 0)
				{
					pThis->m_SendBytes -= nRet;

					if (pThis->m_SendBytes > 0) //û�з�����ȫ
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

