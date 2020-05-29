#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#include "DataProcessBase.h"

typedef int (CDataProcessBase::*pfunc)(const string &, void *);
typedef pair<void *, pfunc> FUNC;
typedef map<unsigned int, FUNC> CMD_MAP;

class NETWORK_API CDispatch
{
public:
    CDispatch();
    ~CDispatch();

	//分包，调用注册的回调函数处理
    int dispatch_back(UINT msgType,const string &str);

	//网络状态回调
	int net_state_back(UINT type);

	//注册处理函数
    int register_back_map(unsigned int id, void *p, pfunc func);

	//反注册，清除所有回调函数指针
	int unregister();

private:
    CMD_MAP back_map;

	CRITICAL_SECTION map_section;
};

#endif
