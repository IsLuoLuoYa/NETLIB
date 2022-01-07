#pragma once
#include "NetMsgStruct.h"
#include "Service.h"

enum ENetMsgCmd
{
	CMD_Echo1,
	CMD_Echo2
};

struct CMsg_Echo1 :public CNetMsgHead
{
	char MdData[4]{};
	int Mdint = 0;
	CMsg_Echo1()
	{
		MdLen = sizeof(CMsg_Echo1);
		MdCmd = CMD_Echo1;
	}
};

struct CMsg_Echo2 :public CNetMsgHead
{
	char MdData[4]{};
	int Mdint = 0;
	CMsg_Echo2()
	{
		MdLen = sizeof(CMsg_Echo2);
		MdCmd = CMD_Echo2;
	}
};

#ifndef WIN32
class CTestSevice : public CServiceEpoll
{
public:
	CTestSevice(int HeartBeatTime, int ServiceMaxPeoples, int DisposeThreadNums);
private:
	virtual void MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid);
};
#endif