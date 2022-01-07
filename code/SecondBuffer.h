#pragma once
#include <mutex>
#include "win_linux_HEAD.h"
const int DEFAULTBUFFERLEN = 16384;

class CSecondBuffer
{
private:
	char*		MdPBuffer = nullptr;		// 缓冲区指针
	int			MdBufferLen = -1;			// 缓冲区长度
	int			Mdtail = 0;
	std::mutex	MdMtx;						// 操作数据指针时需要的互斥元
	CSecondBuffer();
public:
	CSecondBuffer(int bufferlen = DEFAULTBUFFERLEN);
	~CSecondBuffer();

	char* MfGetBufferP();	// 返回缓冲区原始指针以供操作

	bool MfDataToBuffer(const char* data, int len);		// 写数据到缓冲区
	int MfBufferToSocket(SOCKET sock);					// 数据从 缓冲区 写到 套接字，返回值主要用于判断socket是否出错，出错条件是send或recv返回值<=0

	int MfSocketToBuffer(SOCKET sock);		// 数据从 套接字 写到 缓冲区，返回值主要用于判断socket是否出错，出错条件是send或recv返回值<=0
	bool MfHasMsg();						// 缓冲区中是否够一条消息的长度
	void MfPopFrontMsg();					// 弹出缓冲区中的第一条消息
	void MfBufferPopData(int len);			// 缓冲区中数据弹出

private:
	bool MfSend(SOCKET sock, const char* buf, int len, int* ret);	// 封装SEND和RECV调用其非阻塞模式，并处理两个问题
	bool MfRecv(SOCKET sock, void* buf, int len, int* ret);			// 返回值用来区分ret值结果参数是否可用，如果为1表示ret的值应该被用于更新tail，反之则不应该更新
};