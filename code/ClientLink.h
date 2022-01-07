#pragma once
#include <string>
#include <atomic>
#include "win_linux_HEAD.h"
class CSocketObj;
class CSecondBuffer;

class CClientLink
{
private:
	std::string				MdLinkName;			// 服务名称，内容
	CSocketObj*				MdClientSock;		// 客户连接对象
	std::atomic<int>		MdIsConnect = 0;	// 表示是否连接成功	
private:
public:
	CClientLink(std::string s = "未命名");
	~CClientLink();
	int MfConnect(const char* ip, unsigned short port);	// 发起一个连接
	int MfClose();										// 关闭一个连接
	SOCKET MfGetSocket();								// 返回描述符
	std::string MfGetSerivceNmae();						// 返回当前服务的名称
	int MfGetIsConnect();								// 当前服务是否连接
	int MfRecv();										// 供服务管理者调用接收数据						只是简单调用第二缓冲区的成员函数
	int MfSend();										// 供服务管理者调用发送数据						只是简单调用第二缓冲区的成员函数
	bool MfDataToBuffer(const char* data, int len);		// 供调用者插入数据，插入数据到发送缓冲区
	const char* MfGetRecvBufferP();						// 供使用者处理数据，取得接收缓冲区原始指针		只是简单调用第二缓冲区的成员函数
	bool MfHasMsg();									// 供使用者处理数据，缓冲区是否有消息			只是简单调用第二缓冲区的成员函数
	void MfPopFrontMsg();								// 供使用者处理数据，第一条信息移出缓冲区		只是简单调用第二缓冲区的成员函数
};