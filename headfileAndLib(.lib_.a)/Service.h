#pragma once
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>
#include <map>
#include "win_linux_HEAD.h"
#include "NetMsgStruct.h"
#include "ThreadPool.h"

class CSocketObj;
template <class T>
class CObjectPool;
class Barrier;
class CTimer;

class CServiceNoBlock
{
protected:
	SOCKET								MdListenSock;				// 监听套接字
	int									MdServiceMaxPeoples;		// 该服务的上限人数，暂未被使用
	int									MdDisposeThreadNums;		// 消息处理线程数
	int									MdHeartBeatTime;			// 心跳时限，该时限内未收到消息默认断开，单位秒
	CTimer*								MdHeartBeatTestInterval;	// 心跳间隔检测用到的计时器
	CObjectPool<CSocketObj>*			Md_CSocketObj_POOL;			// 客户端对象的对象池
	std::map<SOCKET, CSocketObj*>*		MdPClientJoinList;			// 非正式客户端缓冲列表，等待加入正式列表，同正式客户列表一样，一个线程对应一个加入列表
	std::mutex*							MdPClientJoinListMtx;		// 非正式客户端列表的锁
	std::map<SOCKET, CSocketObj*>*		MdPClientFormalList;		// 正式的客户端列表，一个线程对应一个map[],存储当前线程的服务对象,map[threadid]找到对应map，map[threadid][socket]找出socket对应的数据
	std::shared_mutex*					MdPClientFormalListMtx;		// 为一MdPEachDisoposeThreadOfServiceObj[]添加sock时，应MdEachThreadOfMtx[].lock
	std::map<SOCKET, CSocketObj*>*		MdClientLeaveList;			// 等待从正式列表中移除的缓冲列表
	std::mutex*							MdClientLeaveListMtx;		// 移除列表的锁
	CThreadPool*						MdThreadPool;
protected:			// 用来在服务启动时，等待其他所有线程启动后，再启动Accept线程
	Barrier*		MdBarrier1;
protected:			// 这一组变量，用来在服务结束时，按accept recv dispose send的顺序来结束线程以保证不出错
	Barrier*		MdBarrier2;		// accept线程		和	recv线程			的同步变量		！！！！！在epoll中未被使用！！！！！
	Barrier*		MdBarrier3;		// recv线程			和	所有dispos线程		的同步变量
	Barrier*		MdBarrier4;		// send线程			和	dispose线程			的同步和前面不同，用屏障的概念等待多个线程来继续执行
public:
	CServiceNoBlock(int HeartBeatTime = 300, int ServiceMaxPeoples = 1000, int DisposeThreadNums = 1);
	virtual ~CServiceNoBlock();
	void Mf_NoBlock_Start(const char* ip, unsigned short port);		// 启动收发处理线程的非阻塞版本
protected:
	void Mf_Init_ListenSock(const char* ip, unsigned short port);	// 初始化套接字
private:
	void Mf_NoBlock_AcceptThread();									// 等待客户端连接的线程
	void Mf_NoBlock_RecvThread();									// 收线程
	void Mf_NoBlock_DisposeThread(int SeqNumber);					// 处理线程
	void Mf_NoBlock_SendThread();									// 发线程
	void Mf_NoBlock_ClientJoin(std::thread::id threadid, int SeqNumber);	// 客户端加入正式列表
	void Mf_NoBlock_ClientLeave(std::thread::id threadid, int SeqNumber);	// 客户端移除正式列表
protected:
	virtual void MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id & threadid) = 0;	// 使用者复写该函数，来处理收到的消息
};

// --------------------------------------------------------分割线--------------------------------------------------------
// --------------------------------------------------------分割线--------------------------------------------------------
// --------------------------------------------------------分割线--------------------------------------------------------
// --------------------------------------------------------分割线--------------------------------------------------------

#ifndef WIN32

class CServiceEpoll :public CServiceNoBlock
{
private:
	std::vector<SOCKET>			MdEpoll_In_Fd;		// epoll句柄
	std::vector<epoll_event*>	MdEpoll_In_Event;	// 接收线程用到的结构集合
	int							MdThreadAvgPeoples;	// 每个线程的平均人数
public:
	CServiceEpoll(int HeartBeatTime = 300, int ServiceMaxPeoples = 1000, int DisposeThreadNums = 1);
	virtual ~CServiceEpoll();
	void Mf_Epoll_Start(const char* ip, unsigned short port);
private:
	void Mf_Epoll_AcceptThread();							// 等待客户端连接的线程
	void Mf_Epoll_RecvAndDisposeThread(int SeqNumber);		// 处理线程
	void Mf_Epoll_SendThread();								// 发线程
	void Mf_Epoll_ClientJoin(std::thread::id threadid, int SeqNumber);	// 客户端加入正式列表
	void Mf_Epoll_ClientLeave(std::thread::id threadid, int SeqNumber);	// 客户端移除正式列表
};

#endif // !WIN32