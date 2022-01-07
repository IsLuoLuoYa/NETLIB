#include "test.h"
#include "Socket.h"
#include "Log.h"
#ifndef WIN32
CTestSevice::CTestSevice(int HeartBeatTime, int ServiceMaxPeoples, int DisposeThreadNums) : CServiceEpoll(HeartBeatTime, ServiceMaxPeoples, DisposeThreadNums)
{

}

void CTestSevice::MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid)
{
	CMsg_Echo2 echo;
	switch (msg->MdCmd)
	{
	case CMD_Echo1:
		for (int i = 0; i < 10; ++i)
			strncmp(echo.MdData, ((CMsg_Echo1*)msg)->MdData, sizeof(CMsg_Echo1));		// 复制十次来模拟正常的耗时操作
		echo.Mdint = ((CMsg_Echo1*)msg)->Mdint + 1;
		cli->MfDataToBuffer((const char*)&echo, sizeof(CMsg_Echo2));
		break;

	case CMD_Echo2:
		printf("recv server return data,len : <%d>", sizeof(((CMsg_Echo2*)msg)->MdData));
		break;

	default:
		printf("cmd:<%d>, len:<%d>, int:<%d>\n", ((CMsg_Echo1*)msg)->MdCmd, ((CMsg_Echo1*)msg)->MdLen, ((CMsg_Echo1*)msg)->Mdint);
		LogFormatMsgAndSubmit(threadid, ERROR_FairySun, "SOCKET <%5d> <%s:%5d>\t recv undefined operation", sock, cli->MfGetPeerIP(), cli->MfGetPeerPort());
		break;
	}
}
#endif // !WIN32

