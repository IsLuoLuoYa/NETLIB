#include <thread>
#include <string.h>
#include "ClientLink.h"
#include "Log.h"
#include "Socket.h"


CClientLink::CClientLink(std::string s) :MdLinkName(s), MdClientSock(nullptr)
{ 
	// 构造封装的socket对象时必须传递socket，所以不在这里构造，connect成功后在new
}

CClientLink::~CClientLink() 
{ 
	MfClose();
}

int CClientLink::MfConnect(const char* ip, unsigned short port)
{
	auto Id = std::this_thread::get_id();
	int ret = -1;
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
#ifndef WIN32
	if (ip)		addr.sin_addr.s_addr = inet_addr(ip);
	else		addr.sin_addr.s_addr = INADDR_ANY;
#else
	if (ip)		addr.sin_addr.S_un.S_addr = inet_addr(ip);
	else		addr.sin_addr.S_un.S_addr = INADDR_ANY;
#endif


	SOCKET CliSock = socket(AF_INET, SOCK_STREAM, 0);
	if (SOCKET_ERROR == CliSock)
	{
		LogFormatMsgAndSubmit(Id, ERROR_FairySun, "client create listenSock failed!");
		std::this_thread::sleep_for(std::chrono::seconds(LOG_SYN_TIME_SECOND + 1));	// 比日志同步时间多一秒，保证提交的日志被写到硬盘
		return SOCKET_ERROR;
	}

	for (int i = 0; i < 3; ++i)
	{
		printf("service <%s> %d st linking...\n", MdLinkName.c_str(), i + 1);
		ret = connect(CliSock, (sockaddr*)&addr, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			printf("service <%s> %d st link fail，errno:<%d>. errnomsg:<%s>\n", MdLinkName.c_str(), i + 1, errno, strerror(errno));
			LogFormatMsgAndSubmit(Id, ERROR_FairySun, "service <%s> %d st link fail，errno:<%d>. errnomsg:<%s>", MdLinkName.c_str(), i + 1, errno, strerror(errno));
			continue;
		}
		else 
		{
			printf("service <%s> %d st link success\n", MdLinkName.c_str(), i + 1);
			LogFormatMsgAndSubmit(Id, ERROR_FairySun, "service <%s> %d st link success", MdLinkName.c_str());
			
#ifdef WIN32		
			unsigned long ul = 1;
			ioctlsocket(CliSock, FIONBIO, &ul);
#else
			int flags = fcntl(CliSock, F_GETFL, 0);
			fcntl(CliSock, F_SETFL, flags | O_NONBLOCK);
#endif
			MdClientSock = new CSocketObj(CliSock);
			MdIsConnect = 1;
			break;
		}
	}
	return ret;
}

int CClientLink::MfClose()
{
	if (MdIsConnect)
	{
		MdIsConnect = 0;
		MdClientSock->MfClose();
	}
		
	if (nullptr != MdClientSock)
		delete MdClientSock;
	MdClientSock = nullptr;
	return 0;
}

SOCKET CClientLink::MfGetSocket()
{
	return MdClientSock->MfGetSock();
}

std::string CClientLink::MfGetSerivceNmae()
{
	return MdLinkName.c_str();
}

int CClientLink::MfGetIsConnect()
{
	return MdIsConnect.load();
}

//void CClientLink::MfSetIsConnect(int a)
//{
//	MdIsConnect = a;
//}

int CClientLink::MfRecv()
{ 
	return MdClientSock->MfRecv(); 
}	

int CClientLink::MfSend()
{ 
	return MdClientSock->MfSend(); 
}

bool CClientLink::MfDataToBuffer(const char* data, int len)
{
	return MdClientSock->MfDataToBuffer(data, len);
}

const char* CClientLink::MfGetRecvBufferP()
{ 
	return MdClientSock->MfGetRecvBufP();
}

bool CClientLink::MfHasMsg()
{
	return MdClientSock->MfHasMsg();
}

void CClientLink::MfPopFrontMsg()
{
	MdClientSock->MfPopFrontMsg();
}