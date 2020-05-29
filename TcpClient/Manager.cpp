#include "stdafx.h"
#include "Manager.h"
#include "constdef.h"
#include "TcpClient.h"
#include "dispatch.h"
#include "ProtocolBase.h"
#include <math.h>
#include <sstream>
#pragma comment(lib,"ws2_32.lib")

NETWORK_API CManager *AfxGetMng()
{
    return CManager::GetManager();
}

CManager *CManager::m_pInstance = NULL;

CManager::CManager()
{
    m_bRunning = false;
    m_bStart = false;
    m_thread = NULL;
    m_threadid = 0;
	m_bConnected = false;

    m_tcpClient = new CTcpClient;
    m_pDispatch = new CDispatch;
	m_pProtocolHandler = NULL;

	InitializeCriticalSection(&m_sectionRsp);
	InitializeCriticalSection(&m_sectionReq);

    if (!m_pInstance)
    {
        m_pInstance = this;
    }
}

CManager::~CManager()
{
    Stop(); //����Ѿ�stop��������ʱ�����Ҳ��Ӱ��

    if (m_pInstance==this)
    {
        m_pInstance=NULL;
    }

    if (m_tcpClient)    //����ͨ�Ŷ���ʵ��
    {
        delete m_tcpClient;
    }

    DeleteCriticalSection(&m_sectionRsp);
    DeleteCriticalSection(&m_sectionReq);
}

void CManager::OnOpen()
{
	//���ӳɹ�
	m_bConnected = true;
	GetDispatch()->net_state_back(EnumCallBackOnOpen);
}

void CManager::OnClose()
{
	//���ӹر�
	m_bConnected = false;
	GetDispatch()->net_state_back(EnumCallBackOnClose);
}

void CManager::OnError()
{
	//���Ӵ���
	m_bConnected = false;
	GetDispatch()->net_state_back(EnumCallBackOnError);
}

int CManager::OnMessage(const string &msg)
{
	//��Ϣ����Э�鴦������Ӧ������У�����ʵ��Э��ְ�
	int len=msg.length();

	//Э�����ϲ������������İ�����CManager��
	if(m_pProtocolHandler)
	{
		len = m_pProtocolHandler->OnSeparatePacket(msg);
	}

	return len;
}

void CManager::SetProtocalHandler(CProtocolBase *pProtocalHandler)
{
	m_pProtocolHandler = pProtocalHandler;
}

int CManager::Start(const string &ip, unsigned short port,UINT KeepAliveSeconds)
{
    if (m_bStart) return 0;

	m_KeepAliveSeconds = KeepAliveSeconds;

    m_tcpClient->SetHandle(this);
    m_tcpClient->Start(ip, port);

    m_thread = (HANDLE)CreateThread(NULL, 0, Run, (LPVOID)this, 0, &m_threadid);

    m_bStart = true;

    return 0;
}

int CManager::Stop()
{
    if (m_bStart)
    {
        m_bRunning = false;

        WaitForSingleObject(m_thread,INFINITE); //�ȴ������ڲ��߳��˳�

	    CloseHandle(m_thread);  //�ر��߳̾��

        m_tcpClient->Stop();    //ֹͣͨ���߳�

		m_bStart = false;
    }

    return 0;
}

void CManager::RecvData(const string &strMsg)
{
	EnterCriticalSection(&m_sectionRsp);
	m_rspQueue.push_back(strMsg);
	LeaveCriticalSection(&m_sectionRsp);
}

void CManager::SendData(const string &strMsg)
{
    EnterCriticalSection(&m_sectionReq);
    m_reqQueue.push_back(strMsg);
    LeaveCriticalSection(&m_sectionReq);
}

bool CManager::GetNetState()
{
	return m_bConnected;
}

CManager *CManager::GetManager()
{
	if(!m_pInstance)
	{
		m_pInstance = new CManager;
	}

    return m_pInstance;
}

CDispatch *CManager::GetDispatch()
{
    return m_pDispatch;
}

DWORD CManager::Run(LPVOID lpParam)
{
	CManager *pManager = (CManager *)lpParam;

	pManager->m_bRunning = true;

	list<string>::iterator iter;
	list<string>::iterator tmpiter;
	int sLen;
	string ReqMsg;
	string strRsp;
	int second_count = 0;
	bool bIsSendFailed;

	time_t tmNow,tmEnd;
	time(&tmEnd);

	while (pManager->m_bRunning)
	{
		list<string> tmpQueue; //��ʱ����

		//�ȼ��Manager����ʱ����
		if (!pManager->m_tmpReqQueue.empty())
		{
			for (tmpiter = pManager->m_tmpReqQueue.begin(); tmpiter != pManager->m_tmpReqQueue.end(); ++tmpiter)
			{
				tmpQueue.push_back((*tmpiter));
			}

			pManager->m_tmpReqQueue.clear();
		}

		//���������ݿ�����һ����ʱ����
		EnterCriticalSection(&pManager->m_sectionReq);
		for (tmpiter = pManager->m_reqQueue.begin(); tmpiter != pManager->m_reqQueue.end(); ++tmpiter)
		{
			tmpQueue.push_back((*tmpiter));
		}
		pManager->m_reqQueue.clear();
		LeaveCriticalSection(&pManager->m_sectionReq);

		if(!tmpQueue.empty())
		{
			time(&tmEnd);
		}

		bIsSendFailed = false;//����ʧ�ܺ󣬺����İ����´��ٷ���
		for (tmpiter = tmpQueue.begin(); tmpiter != tmpQueue.end(); ++tmpiter)
		{
			if (!bIsSendFailed)
			{
				ReqMsg.assign((char *)(*tmpiter).c_str(), (*tmpiter).length());

				sLen = pManager->m_tcpClient->Send((char *)ReqMsg.c_str(), ReqMsg.length());
				if (sLen > 0)   //���ͳɹ�������һ����δ���ͳɹ��������������¼
				{
					continue;
				}
				else    //����ʧ��(����������)
				{
					bIsSendFailed = true;
					pManager->m_tmpReqQueue.push_back((*tmpiter));
				}
			}
			else
			{
				pManager->m_tmpReqQueue.push_back((*tmpiter));
			}
		}

		tmpQueue.clear();

		//����Ӧ�𣬽�Ӧ�����ݿ�������ʱ�����д���
		EnterCriticalSection(&pManager->m_sectionRsp);
		for (tmpiter = pManager->m_rspQueue.begin(); tmpiter != pManager->m_rspQueue.end(); ++tmpiter)
		{
			tmpQueue.push_back(*tmpiter);
		}
		pManager->m_rspQueue.clear();
		LeaveCriticalSection(&pManager->m_sectionRsp);

		if(!tmpQueue.empty())
		{
			time(&tmEnd);
		}

		//������ʱ���У���������
		for (tmpiter = tmpQueue.begin(); tmpiter != tmpQueue.end(); ++tmpiter)
		{
			if(pManager->m_pProtocolHandler)
			{
				UINT msgType = pManager->m_pProtocolHandler->OnGetMsgType(*tmpiter);

				//ͨ��dispatch���䵽���ҵ�������У�ռ�õ���manager���߳�
				pManager->GetDispatch()->dispatch_back(msgType,*tmpiter);
			}
		}

		//����������һ��ʱ����û��������������
		if(pManager->m_bConnected)
		{
			time(&tmNow);
			if(fabs(difftime(tmEnd,tmNow)) > pManager->m_KeepAliveSeconds)
			{
				if(pManager->m_pProtocolHandler)
				{
					string msg=pManager->m_pProtocolHandler->OnGetKeepAliveRequest();
					pManager->SendData(msg);
				}
			}
		}

		Sleep(10);//���û����仰��cpu��ռ���ʸ�
	}

	return 0;
}
