#include "Service.h"
#include "Time.h"
#include "ObjectPoolg.h"
#include "ThreadManage.h"
#include "Log.h"
#include "Socket.h"
#include "NetMsgStruct.h"
CServiceNoBlock::CServiceNoBlock(int HeartBeatTime, int ServiceMaxPeoples, int DisposeThreadNums) :
	MdServiceMaxPeoples(ServiceMaxPeoples),
	MdDisposeThreadNums(DisposeThreadNums),
	MdHeartBeatTime(HeartBeatTime),
	MdHeartBeatTestInterval(nullptr),
	Md_CSocketObj_POOL(nullptr),
	MdPClientJoinList(nullptr),
	MdPClientJoinListMtx(nullptr),
	MdPClientFormalList(nullptr),
	MdPClientFormalListMtx(nullptr),
	MdClientLeaveList(nullptr),
	MdClientLeaveListMtx(nullptr),
	MdBarrier1(nullptr),
	MdBarrier2(nullptr),
	MdBarrier3(nullptr),
	MdBarrier4(nullptr),
	MdThreadPool(nullptr)
{
	MdHeartBeatTestInterval = new CTimer;
	Md_CSocketObj_POOL		= new CObjectPool<CSocketObj>(MdServiceMaxPeoples);
	MdPClientFormalList		= new std::map<SOCKET, CSocketObj*>[MdDisposeThreadNums];
	MdPClientFormalListMtx	= new std::shared_mutex[MdDisposeThreadNums];
	MdPClientJoinList		= new std::map<SOCKET, CSocketObj*>[MdDisposeThreadNums];
	MdPClientJoinListMtx	= new std::mutex[MdDisposeThreadNums];
	MdClientLeaveList		= new std::map<SOCKET, CSocketObj*>[MdDisposeThreadNums];
	MdClientLeaveListMtx	= new std::mutex[MdDisposeThreadNums];
	MdBarrier1 = new Barrier;
	MdBarrier2 = new Barrier;
	MdBarrier3 = new Barrier;
	MdBarrier4 = new Barrier;
	MdThreadPool = new CThreadPool;
}

CServiceNoBlock::~CServiceNoBlock()
{
	if (MdThreadPool)
		delete MdThreadPool;
	MdThreadPool = nullptr;
	
	if (MdHeartBeatTestInterval)
		delete MdHeartBeatTestInterval;
	MdHeartBeatTestInterval = nullptr;

	if (Md_CSocketObj_POOL)
		delete Md_CSocketObj_POOL;
	Md_CSocketObj_POOL = nullptr;

	if (MdPClientFormalList)
		delete[] MdPClientFormalList;
	MdPClientFormalList = nullptr;

	if (MdPClientFormalListMtx)
		delete[] MdPClientFormalListMtx;
	MdPClientFormalListMtx = nullptr;

	if (MdPClientJoinList)
		delete[] MdPClientJoinList;
	MdPClientJoinList = nullptr;

	if (MdPClientJoinListMtx)
		delete[] MdPClientJoinListMtx;
	MdPClientJoinListMtx = nullptr;

	if (MdClientLeaveList)
		delete[] MdClientLeaveList;
	MdClientLeaveList = nullptr;

	if (MdClientLeaveListMtx)
		delete[] MdClientLeaveListMtx;
	MdClientLeaveListMtx = nullptr;

	if (MdBarrier1)
		delete MdBarrier1;
	MdBarrier1 = nullptr;

	if (MdBarrier2)
		delete MdBarrier2;
	MdBarrier2 = nullptr;

	if (MdBarrier3)
		delete MdBarrier3;
	MdBarrier3 = nullptr;

	if (MdBarrier4)
		delete MdBarrier4;
	MdBarrier4 = nullptr;
}

void CServiceNoBlock::Mf_NoBlock_Start(const char * ip, unsigned short port)
{
	Mf_Init_ListenSock(ip, port);

	// 处理线程同步
	MdBarrier1->MfInit(MdDisposeThreadNums + 2 + 1);	// 处理线程数量 + send和recv线程 + recv线程本身
	MdBarrier2->MfInit(2);								// accept + recv两条线程
	MdBarrier3->MfInit(MdDisposeThreadNums + 1);		// 处理线程数量 + recv线程
	MdBarrier4->MfInit(MdDisposeThreadNums + 1);		// 处理线程数量 + send线程

	// 启动各个线程
	MdThreadPool->MfStart(1 + MdDisposeThreadNums + 1 + 1);
	MdThreadPool->MfEnqueue(std::bind(&CServiceNoBlock::Mf_NoBlock_SendThread, this));			// 启动发送线程
	for (int i = 0; i < MdDisposeThreadNums; ++i)
		MdThreadPool->MfEnqueue(std::bind(&CServiceNoBlock::Mf_NoBlock_DisposeThread, this, i));	// 启动多条处理线程线程
	MdThreadPool->MfEnqueue(std::bind(&CServiceNoBlock::Mf_NoBlock_RecvThread, this));			// 启动接收线程
	MdThreadPool->MfEnqueue(std::bind(&CServiceNoBlock::Mf_NoBlock_AcceptThread, this));			// 启动等待连接线程
}

void CServiceNoBlock::Mf_Init_ListenSock(const char* ip, unsigned short port)
{
	std::thread::id threadid = std::this_thread::get_id();

	// 初始化监听套接字
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

	MdListenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (SOCKET_ERROR == MdListenSock)
	{
		LogFormatMsgAndSubmit(threadid, ERROR_FairySun, "server create listenSock failed!");
		ErrorWaitExit();
	}
	if (SOCKET_ERROR == bind(MdListenSock, (sockaddr*)&addr, sizeof(addr)))
	{
		LogFormatMsgAndSubmit(threadid, ERROR_FairySun, "listenSock bind failed!");
		ErrorWaitExit();
	}
	if (SOCKET_ERROR == listen(MdListenSock, 100))
	{
		LogFormatMsgAndSubmit(threadid, ERROR_FairySun, "listen socket failed!");
		ErrorWaitExit();
	}

	// 将接收套接字设为非阻塞，这里为了解决两个问题
	// 1是阻塞套接字，在整个服务程序退出时，如果没有客户端连接到来，会导致accept线程阻塞迟迟无法退出
	// 2是在unp第十六章非阻塞accept里，不过这个问题只在单线程情况下出现，就不写了
#ifndef WIN32														// 对套接字设置非阻塞
	int flags = fcntl(MdListenSock, F_GETFL, 0);
	fcntl(MdListenSock, F_SETFL, flags | O_NONBLOCK);
#else
	unsigned long ul = 1;
	ioctlsocket(MdListenSock, FIONBIO, &ul);
#endif
}

void CServiceNoBlock::Mf_NoBlock_AcceptThread()
{
	std::thread::id threadid = std::this_thread::get_id();

	// 接受连接时用到的数据
	SOCKET			sock ;			
	sockaddr_in		addr;			// 地址
	int				sizeofsockaddr = sizeof(sockaddr);
#ifndef WIN32
	socklen_t		len;			// 地址长度
#else
	int				len;			// 地址长度
#endif
	int				minPeoples = INT32_MAX;		// 存储一个最小人数
	int				minId = 0;					// 最小人数线程的Id
	int				currPeoples = 0;			// 当前已连接人数

	// 等待其他所有线程
	MdBarrier1->MfWait();
	printf("NoBlock Accept Thread already Start\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "NoBlock AcceptThread already Start");
	
	// 主循环，每次循环接收一个连接
	while (!MdThreadPool->MfIsStop())
	{
		currPeoples = 0;
		for (int i = 0; i < MdDisposeThreadNums; ++i)	// 计算当前已连接人数
			currPeoples += (int)MdPClientFormalList[i].size();
		if (currPeoples > MdServiceMaxPeoples)	// 大于上限服务人数，就不accpet，等待然后开始下一轮循环
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		sock = INVALID_SOCKET;
		addr = {};
		len = sizeofsockaddr;
		sock = accept(MdListenSock, (sockaddr*)&addr, &len);
		if (SOCKET_ERROR == sock)
		{
			if (errno == 0 || errno == EWOULDBLOCK)			// windows非阻塞sock没有到达时errno是0，linux则是EWOULDBLOCK
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			else if (INVALID_SOCKET == sock)
				LogFormatMsgAndSubmit(threadid, WARN_FairySun, "accept return INVALID_SOCKET!");
			else
				LogFormatMsgAndSubmit(threadid, WARN_FairySun, "accept return ERROR!");
			continue;
		}
		else
		{
#ifndef WIN32												// 对收到套接字设置非阻塞
			int flags = fcntl(sock, F_GETFL, 0);
			fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#else
			unsigned long ul = 1;
			ioctlsocket(sock, FIONBIO, &ul);
#endif
			minPeoples = INT32_MAX;
			for (int i = 0; i < MdDisposeThreadNums; ++i)	// 找出人数最少的
			{
				if (minPeoples > (int)MdPClientFormalList[i].size())
				{
					minPeoples = (int)MdPClientFormalList[i].size();
					minId = i;
				}
			}
			CSocketObj* TempSocketObj = Md_CSocketObj_POOL->MfApplyObject(sock);
			TempSocketObj->MfSetPeerAddr(&addr);
			TempSocketObj->MfSetThreadIndex(minId);
			std::lock_guard<std::mutex> lk(MdPClientJoinListMtx[minId]);								// 对应的线程map上锁
			MdPClientJoinList[minId].insert(std::pair<SOCKET, CSocketObj*>(sock, TempSocketObj));		// 添加该连接到加入缓冲区
			LogFormatMsgAndSubmit(threadid, INFO_FairySun, "SOCKET <%5d> <%s:%5d>\tTo ClientJoinList[%d]", sock, TempSocketObj->MfGetPeerIP(), TempSocketObj->MfGetPeerPort(), minId);
		}
	}

	// 主循环结束后清理
#ifdef WIN32
	closesocket(MdListenSock);
#else
	close(MdListenSock);
#endif // WIN32

	printf("NoBlock Accept Thread already over\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "NoBlock Accept Thread already over");
	MdBarrier2->MfWait();
}

void CServiceNoBlock::Mf_NoBlock_RecvThread()
{
	std::thread::id threadid = std::this_thread::get_id();

	printf("Recv Thread already Start\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Recv Thread already Start");

	MdBarrier1->MfWait();	

	int ret = 0;

	// 主循环
	while (!MdThreadPool->MfIsStop())
	{
		for (int i = 0; i < MdDisposeThreadNums; ++i)
		{			
			std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[i]);				// 对应的线程map上锁，放在这里上锁的原因是为了防止插入行为导致迭代器失效
			for(auto it = MdPClientFormalList[i].begin(); it != MdPClientFormalList[i].end(); ++it)
			{
				ret = it->second->MfRecv();
				if (0 >= ret)				// 返回值小于等于0时表示socket出错或者对端关闭
				{
					std::unique_lock<std::mutex> lk(MdClientLeaveListMtx[i], std::defer_lock);		// 这里尝试锁定，而不是阻塞锁定，因为这里允许被跳过
					if (lk.try_lock())																// 每一轮recv或者send发现返回值<=0,都会尝试锁定，该连接加入待移除缓冲区
						MdClientLeaveList[i].insert(std::pair<SOCKET, CSocketObj*>(it->first, it->second));		
				}
				else if(INT32_MAX != ret)	// 没有出错时返回的值都是大于等于0的，但是返回值是INT32_MAX时，没有出错，但是也没有成功压入数据，只有成功压入数据时才更新计时器
					it->second->MfHeartBeatUpDate();
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// 主循环结束后，首先等待accept线程的结束以及发来的通知
	MdBarrier2->MfWait();

	// 然后对所有套接字最后一次接收
	for (int i = 0; i < MdDisposeThreadNums; ++i)
	{
		std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[i]);				
		for (auto it = MdPClientFormalList[i].begin(); it != MdPClientFormalList[i].end(); ++it)
			it->second->MfRecv();
	}

	printf("Recv Thread already over\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Recv Thread already over");


	// 接下来通知所有dispose线程
	MdBarrier3->MfWait();
}

void CServiceNoBlock::Mf_NoBlock_DisposeThread(int SeqNumber)
{
	std::thread::id threadid = std::this_thread::get_id();
	CTimer time;

	printf("Dispose Thread <%d> already Start\n", SeqNumber);
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Dispose Thread <%d> already Start", SeqNumber);

	MdBarrier1->MfWait();

	// 主循环
	while (!MdThreadPool->MfIsStop())
	{
		if (time.getElapsedSecond() > 1)
		{
			Mf_NoBlock_ClientJoin(threadid, SeqNumber);		// 将待加入队列的客户放入正式
			Mf_NoBlock_ClientLeave(threadid, SeqNumber);		// 清理离开队列的客户端
			time.update();
		}

		// 遍历正式客户端列表处理所有消息
		{
			std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[SeqNumber]);
			for (auto it : MdPClientFormalList[SeqNumber])
			{
				while (it.second->MfHasMsg())
				{
					if (-1 == ((CNetMsgHead*)it.second->MfGetRecvBufP())->MdCmd)	// 当该包的该字段为-1时，代表心跳
						it.second->MfHeartBeatUpDate();		// 更新心跳计时
					else
						MfVNetMsgDisposeFun(it.first, it.second, (CNetMsgHead*)it.second->MfGetRecvBufP(), threadid);
					it.second->MfPopFrontMsg();
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// 主循环结束后，所有处理线程都等待recv线程发来的通知
	MdBarrier3->MfWait();

	// 对所有套接字进行最后一次处理
	for (auto it : MdPClientFormalList[SeqNumber])
	{
		while (it.second->MfHasMsg())
		{
			MfVNetMsgDisposeFun(it.first, it.second, (CNetMsgHead*)it.second->MfGetRecvBufP(), threadid);
			it.second->MfPopFrontMsg();
		}
	}

	printf("Dispose Thread <%d> already over\n", SeqNumber);
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Dispose Thread <%d> already over", SeqNumber);

	MdBarrier4->MfWait();
}

void CServiceNoBlock::Mf_NoBlock_SendThread()
{
	std::thread::id threadid = std::this_thread::get_id();

	printf("Send Thread already Start\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Send Thread already Start");

	MdBarrier1->MfWait();

	int ret = 0;

	// 主循环
	while (!MdThreadPool->MfIsStop())
	{
		for (int i = 0; i < MdDisposeThreadNums; ++i)
		{
			std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[i]);			// 对应的线程map上锁，放在这里上锁的原因是为了防止插入行为导致迭代器失效
			for (auto it = MdPClientFormalList[i].begin(); it != MdPClientFormalList[i].end(); ++it)
			{
				ret = it->second->MfSend();
				if (0 >= ret)
				{
					std::unique_lock<std::mutex> lk(MdClientLeaveListMtx[i], std::defer_lock);	// 这里尝试锁定，而不是阻塞锁定，因为这里允许被跳过
					if (lk.try_lock())															// 每一轮recv或者send发现返回值<=0,都会尝试锁定,总有一次能锁定他，将该连接加入待移除缓冲区
						MdClientLeaveList[i].insert(std::pair<SOCKET, CSocketObj*>(it->first, it->second));		
				}
				//else if (INT32_MAX != ret)	// 没有出错时返回的值都是大于等于0的，但是返回值是INT32_MAX时，没有出错
				//	;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// 主循环结束后，等待所有处理线程到达屏障,发送完所有数据，然后关闭所有套接字
	MdBarrier4->MfWait();
	
	// 发送所有数据
	for (int i = 0; i < MdDisposeThreadNums; ++i)
	{
		std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[i]);	
		for (auto it = MdPClientFormalList[i].begin(); it != MdPClientFormalList[i].end(); ++it)
			it->second->MfSend();
	}

	// 关闭所有套接字
	for (int i = 0; i < MdDisposeThreadNums; ++i)
	{
		for (auto it = MdPClientFormalList[i].begin(); it != MdPClientFormalList[i].end(); ++it)
		{
			it->second->MfClose();
			Md_CSocketObj_POOL->MfReturnObject(it->second);
		}
	}
	printf("Send Thread already over\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Send Thread already over");
}

void CServiceNoBlock::Mf_NoBlock_ClientJoin(std::thread::id threadid, int SeqNumber)
{
	// 这个函数是客户端加入的，需要操作的，阻塞也是必然的
	std::lock_guard<std::mutex> lk(MdPClientJoinListMtx[SeqNumber]);
	if (!MdPClientJoinList[SeqNumber].empty())
	{
		std::lock_guard<std::shared_mutex> WriteLock(MdPClientFormalListMtx[SeqNumber]);
		for (auto it = MdPClientJoinList[SeqNumber].begin(); it != MdPClientJoinList[SeqNumber].end(); ++it)
		{
			MdPClientFormalList[SeqNumber].insert(std::pair<SOCKET, CSocketObj*>(it->first, it->second));
			LogFormatMsgAndSubmit(threadid, INFO_FairySun, "SOCKET <%5d> <%s:%5d>\tTo ClientFormalList[%d]", it->first, it->second->MfGetPeerIP(), it->second->MfGetPeerPort(), SeqNumber);
		}
		MdPClientJoinList[SeqNumber].clear();
	}
}

void CServiceNoBlock::Mf_NoBlock_ClientLeave(std::thread::id threadid, int SeqNumber)
{
	static const int HeartIntervalTime = MdHeartBeatTime / 3;		// 心跳检测间隔时限

	// 本函数中的两个也都使用尝试锁，因为这里不是重要的地方，也不是需要高效执行的地方
	std::unique_lock<std::mutex> lk(MdClientLeaveListMtx[SeqNumber], std::defer_lock);
	std::unique_lock<std::shared_mutex> WriteLock(MdPClientFormalListMtx[SeqNumber], std::defer_lock);
	if (lk.try_lock() && WriteLock.try_lock())
	{		
		// 第一个循环每隔 心跳超时时间的三分之一 检测一次心跳是否超时
		if (HeartIntervalTime < MdHeartBeatTestInterval->getElapsedSecond())
		{
			// 遍历当前列表的客户端心跳计时器，超时就放入待离开列表
			for (auto it : MdPClientFormalList[SeqNumber])
			{
				if (it.second->MfHeartIsTimeOut(MdHeartBeatTime))
				{
					MdClientLeaveList[SeqNumber].insert(std::pair<SOCKET, CSocketObj*>(it.first, it.second));
					LogFormatMsgAndSubmit(threadid, INFO_FairySun, "SOCKET <%5d> <%s:%5d> heart time out", it.first, it.second->MfGetPeerIP(), it.second->MfGetPeerPort());
				}
					
			}
		}

		// 第二个循环清除客户端
		if (!MdClientLeaveList[SeqNumber].empty())
		{
			for (auto it = MdClientLeaveList[SeqNumber].begin(); it != MdClientLeaveList[SeqNumber].end(); ++it)
			{
				MdPClientFormalList[SeqNumber].erase(it->first);
				it->second->MfClose();
				LogFormatMsgAndSubmit(threadid, INFO_FairySun, "SOCKET <%5d> <%s:%5d>\tClose And Leave ClientFormalList[%d]", it->first, it->second->MfGetPeerIP(), it->second->MfGetPeerPort(), it->second->MfGetThreadIndex());
				Md_CSocketObj_POOL->MfReturnObject(it->second);
			}
			MdClientLeaveList[SeqNumber].clear();
		}
	}
}

// --------------------------------------------------------分割线--------------------------------------------------------
// --------------------------------------------------------分割线--------------------------------------------------------
// --------------------------------------------------------分割线--------------------------------------------------------
// --------------------------------------------------------分割线--------------------------------------------------------

#ifndef WIN32
CServiceEpoll::CServiceEpoll(int HeartBeatTime, int ServiceMaxPeoples, int DisposeThreadNums) :
	CServiceNoBlock(HeartBeatTime, ServiceMaxPeoples, DisposeThreadNums)
{
	MdThreadAvgPeoples = (MdServiceMaxPeoples / MdDisposeThreadNums) + 100;
	for (int i = 0; i < MdDisposeThreadNums; ++i)
	{
		MdEpoll_In_Event.push_back(new epoll_event[MdThreadAvgPeoples]);
	}
}

CServiceEpoll::~CServiceEpoll()
{
	for (auto it : MdEpoll_In_Event)
		delete[] it;
}

void CServiceEpoll::Mf_Epoll_Start(const char* ip, unsigned short port)
{
	for (int i = 0; i < MdDisposeThreadNums; ++i)		// 为各个处理线程建立epoll的描述符
	{
		MdEpoll_In_Fd.push_back(epoll_create(MdThreadAvgPeoples));
	}

	for (int i = 0; i < MdDisposeThreadNums; ++i)		// 检查各个描述符是否出错
	{
		if (-1 == MdEpoll_In_Fd[i])
		{
			printf("create epoll fd error!\n");
			LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "create epoll fd error!");
			ErrorWaitExit();
		}
	}

	Mf_Init_ListenSock(ip, port);

	// 处理线程同步
	MdBarrier1->MfInit(MdDisposeThreadNums + 1 + 1);	// 处理线程数量 + send和recv线程 + recv线程本身
	MdBarrier2->MfInit(2);								// 在epoll中未被使用
	MdBarrier3->MfInit(MdDisposeThreadNums + 1);		// accept + 处理线程数量
	MdBarrier4->MfInit(MdDisposeThreadNums + 1);		// 处理线程数量 + send线程

	MdThreadPool->MfStart(1 + MdDisposeThreadNums + 1);
	MdThreadPool->MfEnqueue(std::bind(&CServiceEpoll::Mf_Epoll_SendThread, this));					// 启动发送线程
	for (int i = 0; i < MdDisposeThreadNums; ++i)
		MdThreadPool->MfEnqueue(std::bind(&CServiceEpoll::Mf_Epoll_RecvAndDisposeThread, this, i));	// 启动多条处理线程,！！！！收线程和处理线程合并了！！！！
	MdThreadPool->MfEnqueue(std::bind(&CServiceEpoll::Mf_Epoll_AcceptThread, this));		// 启动等待连接线程			
}

void CServiceEpoll::Mf_Epoll_AcceptThread()
{
	std::thread::id threadid = std::this_thread::get_id();

	// 接受连接时用到的数据
	SOCKET			sock;
	sockaddr_in		addr;			// 地址
	int				sizeofsockaddr = sizeof(sockaddr);
	socklen_t		len;			// 地址长度
	int				minPeoples = INT32_MAX;	// 存储一个最小人数
	int				minId = 0;					// 最小人数线程的Id
	int				currPeoples = 0;			// 当前已连接人数

	// 等待其他所有线程
	MdBarrier1->MfWait();
	printf("Epoll Accept Thread already Start\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Epoll AcceptThread already Start");

	// 主循环，每次循环接收一个连接
	while (!MdThreadPool->MfIsStop())
	{
		currPeoples = 0;
		for (int i = 0; i < MdDisposeThreadNums; ++i)	// 计算当前已连接人数
			currPeoples += (int)MdPClientFormalList[i].size();
		if (currPeoples > MdServiceMaxPeoples)	// 大于上限服务人数，就不accpet，等待然后开始下一轮循环
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		sock = INVALID_SOCKET;
		addr = {};
		len = sizeofsockaddr;
		sock = accept(MdListenSock, (sockaddr*)&addr, &len);
		if (SOCKET_ERROR == sock)
		{
			if (errno == 0 || errno == EWOULDBLOCK)		// windows非阻塞sock没有到达时errno是0，linux则是EWOULDBLOCK
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			else if (INVALID_SOCKET == sock)
				LogFormatMsgAndSubmit(threadid, WARN_FairySun, "accept return INVALID_SOCKET!");
			else
				LogFormatMsgAndSubmit(threadid, WARN_FairySun, "accept return ERROR!");
			continue;
		}
		else
		{													
			int flags = fcntl(sock, F_GETFL, 0);					// 对收到套接字设置非阻塞
			fcntl(sock, F_SETFL, flags | O_NONBLOCK);
			minPeoples = INT32_MAX;
			for (int i = 0; i < MdDisposeThreadNums; ++i)			// 找出人数最少的
			{
				if (minPeoples > (int)MdPClientFormalList[i].size())
				{
					minPeoples = (int)MdPClientFormalList[i].size();
					minId = i;
				}
			}
			CSocketObj* TempSocketObj = Md_CSocketObj_POOL->MfApplyObject(sock);
			TempSocketObj->MfSetPeerAddr(&addr);
			TempSocketObj->MfSetThreadIndex(minId);
			std::lock_guard<std::mutex> lk(MdPClientJoinListMtx[minId]);								// 对应的线程map上锁
			MdPClientJoinList[minId].insert(std::pair<SOCKET, CSocketObj*>(sock, TempSocketObj));		// 添加该连接到加入缓冲区
			LogFormatMsgAndSubmit(threadid, INFO_FairySun, "SOCKET <%5d> <%s:%5d>\tTo ClientJoinList[%d]", sock, TempSocketObj->MfGetPeerIP(), TempSocketObj->MfGetPeerPort(), minId);
		}
	}

	// 主循环结束后清理
#ifdef WIN32
	closesocket(MdListenSock);
#else
	close(MdListenSock);
#endif // WIN32

	printf("Epoll Accept Thread already over\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Epoll Accept Thread already over");
	MdBarrier3->MfWait();
}

void CServiceEpoll::Mf_Epoll_RecvAndDisposeThread(int SeqNumber)
{
	std::thread::id threadid = std::this_thread::get_id();
	CTimer time;

	printf("Recv and Dispose Thread <%d> already Start\n", SeqNumber);
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Recv and Dispose Thread <%d> already Start", SeqNumber);

	MdBarrier1->MfWait();

	int Epoll_N_Fds;
	int ret = 0;
	SOCKET tmp_fd;
	CSocketObj* tmp_obj;

	// 主循环
	while (!MdThreadPool->MfIsStop())
	{
		if (time.getElapsedSecond() > 1)
		{
			Mf_Epoll_ClientJoin(threadid, SeqNumber);		// 将待加入队列的客户放入正式
			Mf_Epoll_ClientLeave(threadid, SeqNumber);		// 清理离开队列的客户端
			time.update();
		}

		// 遍历正式客户端列表处理所有消息
		{
			std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[SeqNumber]);
			Epoll_N_Fds = epoll_wait(MdEpoll_In_Fd[SeqNumber], MdEpoll_In_Event[SeqNumber], MdThreadAvgPeoples, 0);

			// 第一个循环对epoll返回的集合接收数据
			for (int j = 0; j < Epoll_N_Fds; ++j)
			{
				tmp_fd = MdEpoll_In_Event[SeqNumber][j].data.fd;
				tmp_obj = MdPClientFormalList[SeqNumber][tmp_fd];
				ret = tmp_obj->MfRecv();
				if (0 >= ret)				// 返回值小于等于0时表示socket出错或者对端关闭
				{
					std::unique_lock<std::mutex> lk(MdClientLeaveListMtx[SeqNumber], std::defer_lock);	// 这里尝试锁定，而不是阻塞锁定，因为这里允许被跳过
					if (lk.try_lock())																	// 每一轮recv或者send发现返回值<=0,都会尝试锁定
						MdClientLeaveList[SeqNumber].insert(std::pair<SOCKET, CSocketObj*>(tmp_fd, tmp_obj));			
				}
				else if (INT32_MAX != ret)	// 没有出错时返回的值都是大于等于0的，但是返回值是INT32_MAX时，没有出错，但是也没有成功压入数据，只有成功压入数据时才更新计时器
					MdPClientFormalList[SeqNumber][tmp_fd]->MfHeartBeatUpDate();
			}

			// 第二个循环处理那些收到的数据
			for (int j = 0; j < Epoll_N_Fds; ++j)
			{
				tmp_fd = MdEpoll_In_Event[SeqNumber][j].data.fd;
				tmp_obj = MdPClientFormalList[SeqNumber][tmp_fd];
				while (tmp_obj->MfHasMsg())
				{
					if (-1 == ((CNetMsgHead*)tmp_obj->MfGetRecvBufP())->MdCmd)	// 当该包的该字段为-1时，代表心跳包
						tmp_obj->MfHeartBeatUpDate();		// 更新心跳计时
					else
						MfVNetMsgDisposeFun(tmp_fd, tmp_obj, (CNetMsgHead*)tmp_obj->MfGetRecvBufP(), threadid);
					tmp_obj->MfPopFrontMsg();
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// 主循环结束后，所有处理线程都等待recv线程发来的通知
	MdBarrier3->MfWait();

	// 对所有套接字进行最后一次处理
	std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[SeqNumber]);
	Epoll_N_Fds = epoll_wait(MdEpoll_In_Fd[SeqNumber], MdEpoll_In_Event[SeqNumber], MdThreadAvgPeoples, 0);
	for (int j = 0; j < Epoll_N_Fds; ++j)
	{
		tmp_fd = MdEpoll_In_Event[SeqNumber][j].data.fd;
		tmp_obj = MdPClientFormalList[SeqNumber][tmp_fd];
		ret = tmp_obj->MfRecv();
		while (tmp_obj->MfHasMsg())
		{
			MfVNetMsgDisposeFun(tmp_fd, tmp_obj, (CNetMsgHead*)tmp_obj->MfGetRecvBufP(), threadid);
			tmp_obj->MfPopFrontMsg();
		}
	}

	printf("Recv and Dispose Thread <%d> already over\n", SeqNumber);
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Recv and Dispose Thread <%d> already over", SeqNumber);

	MdBarrier4->MfWait();
}

void CServiceEpoll::Mf_Epoll_SendThread()
{
	std::thread::id threadid = std::this_thread::get_id();

	printf("Send Thread already Start\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Send Thread already Start");

	MdBarrier1->MfWait();

	int ret = 0;

	// 主循环
	while (!MdThreadPool->MfIsStop())
	{
		for (int i = 0; i < MdDisposeThreadNums; ++i)
		{
			std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[i]);				// 对应的线程map上锁，放在这里上锁的原因是为了防止插入行为导致迭代器失效
			
			for (auto it: MdPClientFormalList[i])
			{
				ret = it.second->MfSend();
				if (0 >= ret)				// 返回值小于等于0时表示socket出错或者对端关闭
				{
					std::unique_lock<std::mutex> lk(MdClientLeaveListMtx[i], std::defer_lock);		// 这里尝试锁定，而不是阻塞锁定，因为这里允许被跳过
					if (lk.try_lock())																// 每一轮recv或者send发现返回值<=0,都会尝试锁定
						MdClientLeaveList[i].insert(std::pair<SOCKET, CSocketObj*>(it.first, it.second));
				}
				//else if (INT32_MAX != ret)	// 没有出错时返回的值都是大于等于0的，但是返回值是INT32_MAX时，没有出错，但是也没有成功压入数据，只有成功压入数据时才更新计时器
				//	;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// 主循环结束后，等待所有处理线程到达屏障,发送完所有数据，然后关闭所有套接字
	MdBarrier4->MfWait();

	// 发送所有数据
	for (int i = 0; i < MdDisposeThreadNums; ++i)
	{
		std::shared_lock<std::shared_mutex> ReadLock(MdPClientFormalListMtx[i]);		
		for (auto it: MdPClientFormalList[i])
		{
			it.second->MfSend();
		}
	}

	for (int i = 0; i < MdDisposeThreadNums; ++i)
	{
		for (auto it = MdPClientFormalList[i].begin(); it != MdPClientFormalList[i].end(); ++it)
		{
			it->second->MfClose();
			Md_CSocketObj_POOL->MfReturnObject(it->second);
		}
	}
	printf("Send Thread already over\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "Send Thread already over");
}

void CServiceEpoll::Mf_Epoll_ClientJoin(std::thread::id threadid, int SeqNumber)
{
	epoll_event		EvIn{};
	EvIn.events = EPOLLIN;	
	std::lock_guard<std::mutex> lk(MdPClientJoinListMtx[SeqNumber]);
	if (!MdPClientJoinList[SeqNumber].empty())
	{
		std::lock_guard<std::shared_mutex> WriteLock(MdPClientFormalListMtx[SeqNumber]);
		for (auto it = MdPClientJoinList[SeqNumber].begin(); it != MdPClientJoinList[SeqNumber].end(); ++it)
		{
			MdPClientFormalList[SeqNumber].insert(std::pair<SOCKET, CSocketObj*>(it->first, it->second));
			EvIn.data.fd = it->first;
			epoll_ctl(MdEpoll_In_Fd[SeqNumber], EPOLL_CTL_ADD, it->first, &EvIn);		// 设置套接字到收线程的Epoll
			LogFormatMsgAndSubmit(threadid, INFO_FairySun, "SOCKET <%5d> <%s:%5d>\tTo ClientFormalList[%d]", it->first, it->second->MfGetPeerIP(), it->second->MfGetPeerPort(), SeqNumber);
		}
		MdPClientJoinList[SeqNumber].clear();
	}
}

void CServiceEpoll::Mf_Epoll_ClientLeave(std::thread::id threadid, int SeqNumber)
{
	static const int HeartIntervalTime = MdHeartBeatTime / 3;		// 心跳检测间隔时限

	// 本函数中的两个也都使用尝试锁，因为这里不是重要的地方，也不是需要高效执行的地方
	std::unique_lock<std::mutex> lk(MdClientLeaveListMtx[SeqNumber], std::defer_lock);
	std::unique_lock<std::shared_mutex> WriteLock(MdPClientFormalListMtx[SeqNumber], std::defer_lock);
	if (lk.try_lock() && WriteLock.try_lock())
	{
		// 第一个循环每隔 心跳超时时间的三分之一 检测一次心跳是否超时
		if (HeartIntervalTime < MdHeartBeatTestInterval->getElapsedSecond())
		{
			// 遍历当前列表的客户端心跳计时器，超时就放入待离开列表
			for (auto it : MdPClientFormalList[SeqNumber])
			{
				if (it.second->MfHeartIsTimeOut(MdHeartBeatTime))
				{
					MdClientLeaveList[SeqNumber].insert(std::pair<SOCKET, CSocketObj*>(it.first, it.second));
					LogFormatMsgAndSubmit(threadid, INFO_FairySun, "SOCKET <%5d> <%s:%5d> heart time out", it.first, it.second->MfGetPeerIP(), it.second->MfGetPeerPort());
				}
			}
		}

		// 第二个循环清除客户端
		if (!MdClientLeaveList[SeqNumber].empty())
		{
			for (auto it = MdClientLeaveList[SeqNumber].begin(); it != MdClientLeaveList[SeqNumber].end(); ++it)
			{
				it->second->MfClose();
				epoll_ctl(MdEpoll_In_Fd[SeqNumber], EPOLL_CTL_DEL, it->first, nullptr);
				MdPClientFormalList[SeqNumber].erase(it->first);
				LogFormatMsgAndSubmit(threadid, INFO_FairySun, "SOCKET <%5d> <%s:%5d>\tClose And Leave ClientFormalList[%d]", it->first, it->second->MfGetPeerIP(), it->second->MfGetPeerPort(), it->second->MfGetThreadIndex());
				Md_CSocketObj_POOL->MfReturnObject(it->second);
			}
			MdClientLeaveList[SeqNumber].clear();
		}
	}
}
#endif