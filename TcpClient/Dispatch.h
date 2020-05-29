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

	//�ְ�������ע��Ļص���������
    int dispatch_back(UINT msgType,const string &str);

	//����״̬�ص�
	int net_state_back(UINT type);

	//ע�ᴦ����
    int register_back_map(unsigned int id, void *p, pfunc func);

	//��ע�ᣬ������лص�����ָ��
	int unregister();

private:
    CMD_MAP back_map;

	CRITICAL_SECTION map_section;
};

#endif
