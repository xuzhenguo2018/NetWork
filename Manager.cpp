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
    Stop(); //如果已经stop，在析构时候调用也无影响

    if (m_pInstance==this)
    {
        m_pInstance=NULL;
    }

    if (m_tcpClient)    //析构通信对象实例
    {
        delete m_tcpClient;
    }

    DeleteCriticalSection(&m_sectionRsp);
    DeleteCriticalSection(&m_sectionReq);
}

void CManager::OnOpen()
{
	//连接成功
	m_bConnected = true;
	GetDispatch()->net_state_back(EnumCallBackOnOpen);
}

void CManager::OnClose()
{
	//连接关闭
	m_bConnected = false;
	GetDispatch()->net_state_back(EnumCallBackOnClose);
}

void CManager::OnError()
{
	//连接错误
	m_bConnected = false;
	GetDispatch()->net_state_back(EnumCallBackOnError);
}

int CManager::OnMessage(const string &msg)
{
	//消息处理，协议处理后放入应答队列中，这里实现协议分包
	int len=msg.length();

	//协议由上层解析，解析后的包调用CManager的
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

        WaitForSingleObject(m_thread,INFINITE); //等待自身内部线程退出

	    CloseHandle(m_thread);  //关闭线程句柄

        m_tcpClient->Stop();    //停止通信线程

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
		list<string> tmpQueue; //临时队列

		//先检查Manager的临时队列
		if (!pManager->m_tmpReqQueue.empty())
		{
			for (tmpiter = pManager->m_tmpReqQueue.begin(); tmpiter != pManager->m_tmpReqQueue.end(); ++tmpiter)
			{
				tmpQueue.push_back((*tmpiter));
			}

			pManager->m_tmpReqQueue.clear();
		}

		//将发送内容拷贝到一个临时队列
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

		bIsSendFailed = false;//发送失败后，后续的包等下次再发送
		for (tmpiter = tmpQueue.begin(); tmpiter != tmpQueue.end(); ++tmpiter)
		{
			if (!bIsSendFailed)
			{
				ReqMsg.assign((char *)(*tmpiter).c_str(), (*tmpiter).length());

				sLen = pManager->m_tcpClient->Send((char *)ReqMsg.c_str(), ReqMsg.length());
				if (sLen > 0)   //发送成功处理下一条，未发送成功继续处理该条记录
				{
					continue;
				}
				else    //发送失败(缓冲区已满)
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

		//处理应答，将应答数据拷贝到临时队列中处理
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

		//处理临时队列，解析数据
		for (tmpiter = tmpQueue.begin(); tmpiter != tmpQueue.end(); ++tmpiter)
		{
			if(pManager->m_pProtocolHandler)
			{
				UINT msgType = pManager->m_pProtocolHandler->OnGetMsgType(*tmpiter);

				//通过dispatch分配到相关业务处理函数中，占用的是manager的线程
				pManager->GetDispatch()->dispatch_back(msgType,*tmpiter);
			}
		}

		//发心跳请求，一定时间内没有数据则发心跳包
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

		Sleep(10);//如果没有这句话，cpu会占用率高
	}

	return 0;
}
