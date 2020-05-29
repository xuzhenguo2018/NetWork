#ifndef _DATA_PROCESS_BASE_H_
#define _DATA_PROCESS_BASE_H_

#include "Manager.h"

class NETWORK_API CDataProcessBase
{
public:
    CDataProcessBase();
    virtual ~CDataProcessBase();
};

#define REG_BACK_FUNC(id,pBase,pFunc) AfxGetMng()->GetDispatch()->register_back_map(id,(void *)pBase,(int (CDataProcessBase::*)(const string &, void *))pFunc);

#endif
