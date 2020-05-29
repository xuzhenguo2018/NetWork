#include "stdafx.h"
#include "Dispatch.h"
#include "Manager.h"

CDispatch::CDispatch()
{
	InitializeCriticalSection(&map_section);
}

CDispatch::~CDispatch()
{
	DeleteCriticalSection(&map_section);
}

int CDispatch::dispatch_back(UINT msgType,const string &str)
{
	EnterCriticalSection(&map_section);
    CMD_MAP::iterator iter = back_map.find(msgType);
    if (iter != back_map.end())
    {
        CDataProcessBase *p = (CDataProcessBase *)iter->second.first;
        pfunc func = iter->second.second;
        (p->*func)(str, NULL);
    }
	LeaveCriticalSection(&map_section);

    return 0;
}

int CDispatch::net_state_back(UINT type)
{
	EnterCriticalSection(&map_section);
	CMD_MAP::iterator iter = back_map.find(MsgType_NetStateCallBack);
	if (iter != back_map.end())
	{
		CDataProcessBase *p = (CDataProcessBase *)iter->second.first;
		pfunc func = iter->second.second;
		(p->*func)("", &type);
	}
	LeaveCriticalSection(&map_section);

	return 0;
}

int CDispatch::register_back_map(unsigned int id, void *p, pfunc func)
{
    back_map[id] = make_pair(p, func);
    return 0;
}

int CDispatch::unregister()
{
	EnterCriticalSection(&map_section);
	back_map.clear();
	LeaveCriticalSection(&map_section);
	return 0;
}
