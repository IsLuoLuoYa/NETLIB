#include <string.h>
#include "Socket.h"
#include "SecondBuffer.h"
#include "Time.h"

CSocketObj::CSocketObj(SOCKET sock, int SendBuffLen, int RecvBuffLen):MdSock(sock), MdPSendBuffer(nullptr), MdPRecvBuffer(nullptr), MdHeartBeatTimer(nullptr)
{
	MdPSendBuffer = new CSecondBuffer(SendBuffLen);
	MdPRecvBuffer = new CSecondBuffer(RecvBuffLen);
	MdHeartBeatTimer = new CTimer();
}

CSocketObj::~CSocketObj()
{
	if (MdPSendBuffer)
		delete MdPSendBuffer;
	MdPSendBuffer = nullptr;
	if (MdPRecvBuffer)
		delete MdPRecvBuffer;
	MdPRecvBuffer = nullptr;
	if (MdHeartBeatTimer)
		delete MdHeartBeatTimer;
	MdHeartBeatTimer = nullptr;
}

char* CSocketObj::MfGetRecvBufP()
{
	return MdPRecvBuffer->MfGetBufferP();
}

int CSocketObj::MfRecv()
{
	return MdPRecvBuffer->MfSocketToBuffer(MdSock);
}

int CSocketObj::MfSend()
{
	return MdPSendBuffer->MfBufferToSocket(MdSock);
}

void CSocketObj::MfSetPeerAddr(sockaddr_in* addr)
{
	MdPort = ntohs(addr->sin_port);
	strcpy(MdIP, inet_ntoa(addr->sin_addr));
	//printf("<%s:%d>\n", );
}

char* CSocketObj::MfGetPeerIP() 
{
	return MdIP; 
}

int CSocketObj::MfGetPeerPort() 
{
	return MdPort; 
}

void CSocketObj::MfSetThreadIndex(int index)
{
	MdThreadIndex = index;
}

int CSocketObj::MfGetThreadIndex()
{
	return MdThreadIndex;
}

bool CSocketObj::MfDataToBuffer(const char* data, int len)
{
	return MdPSendBuffer->MfDataToBuffer(data, len);
}

bool CSocketObj::MfHasMsg() 
{ 
	return MdPRecvBuffer->MfHasMsg(); 
}

void CSocketObj::MfPopFrontMsg()
{
	MdPRecvBuffer->MfPopFrontMsg();
}

void CSocketObj::MfHeartBeatUpDate()
{
	MdHeartBeatTimer->update();
}

bool CSocketObj::MfHeartIsTimeOut(int seconds)
{
	return seconds < MdHeartBeatTimer->getElapsedSecond();
}

int	CSocketObj::MfClose()
{
#ifdef WIN32
	return closesocket(MdSock);
#else
	return close(MdSock);
#endif // def WIN32
}