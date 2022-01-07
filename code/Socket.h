#pragma once
#include "win_linux_HEAD.h"
#include "Time.h"

const int DEFAULT_BUFF_LEN = 16384;

class CSecondBuffer;
class CTimer;

// 第二缓冲区版本
class CSocketObj
{
private:
	SOCKET				MdSock;					// socket
	int					MdThreadIndex;			// 当前socket对象位于线程的线程索引，哪个dispose线程，service用
	CSecondBuffer*		MdPSendBuffer;			// 发送缓冲区
	CSecondBuffer*		MdPRecvBuffer;			// 接收缓冲区
	CTimer*				MdHeartBeatTimer;		// 心跳计时器
	char				MdIP[20];				// sock对端的地址
	int					MdPort;					// sock对端的地址
public:
	CSocketObj(SOCKET sock, int SendBuffLen = DEFAULT_BUFF_LEN, int RecvBuffLen = DEFAULT_BUFF_LEN);
	~CSocketObj();
public:
	SOCKET	MfGetSock()			{	return MdSock;	}

	char*	MfGetRecvBufP();					// 返回接收缓冲区原始指针

	int		MfRecv();							// 为该对象接收数据
	int		MfSend();							// 为该对象发送数据

	void	MfSetPeerAddr(sockaddr_in* addr);	// 设置对端IP和端口
	char*	MfGetPeerIP();						// 获取对端IP
	int		MfGetPeerPort();					// 获取对端端口

	void	MfSetThreadIndex(int index);		// 设置线程索引
	int		MfGetThreadIndex();					// 获取线程索引，哪个dispose线程，service用

	bool	MfDataToBuffer(const char* data, int len);		// 压数据到发送缓冲区

	bool	MfHasMsg();							// 接收缓冲区是否有消息
	void	MfPopFrontMsg();					// 第一条信息移出接收缓冲区

	void	MfHeartBeatUpDate();				// 更新心跳计时
	bool	MfHeartIsTimeOut(int seconds);		// 传入一个秒数，返回是否超过该值设定的时间

	int		MfClose();							// 关闭套接字
};