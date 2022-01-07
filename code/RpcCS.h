#pragma once
#include <map>
#include "NetMsgStruct.h"
#include "Service.h"
#include "ClientLinkManage.h"
#include "ThreadPool.h"
// 所有远程调用的函数应当遵循此接口
// 返回值是一个智能指针，指向一个char[]
// 参数是一个智能指针，指向一个char[]
// 使用智能指针而不是直接使用char*，是因为很多地方都是跨线程传递的，方便在调用中的内存管理
// 不论是返回值还是参数，都在void的基础上自定义结构解析
typedef std::function
<
	std::shared_ptr<char[]> (std::shared_ptr<char[]>)
> RemoteProc;

enum RpcNetMsgCmd
{
	RpcProc_NOON = 0	// 若CNetMsgHead::MdCmd为该值，
	// 其余自定义的过程号都应该大于0
};

struct RpcMsg :public CNetMsgHead
{
	// CNetMsgHead::MdLen 该成员依旧代表整个包的长度
	// CNetMsgHead::MdCmd 该成员不再代表某个操作，而是直接代表要调用的那个过程号CallNo
	void* MdData;	// 数据指针
					// 在server中,收到该结构MdData表示参数，发送该结构MdData表示返回值
					// 在client中,收到该结构MdData表示返回值，发送该结构MdData表示参数
					// 发送时不设置该成员，而是直接写数据到缓冲区
					// 从缓冲区取出时使用这个成员
	RpcMsg()
	{
		MdData = nullptr;
		MdCmd = 0;
	}
};

class CRpcServer :private CServiceNoBlock
{
private:
	CThreadPool											MdThreadPool;		// 执行过程时的线程池
	std::map<int, std::tuple<RemoteProc, int, int>>		MdCallList;			// 注册的过程表，分别描述过程号、过程函数，参数的长度、返回值的长度
	std::shared_mutex									MdCallListMutex;	// 该表的互斥元
	std::map<CSocketObj*, std::tuple<int, std::future<std::shared_ptr<char[]>>>>	MdProcRet;		// 过程放入线程池执行时会返回一个std::future<void*>，以便后续异步取得该过程的返回值
																									// 分别描述客户端连接、执行的过程号、可以取得返回值的future对象
	std::shared_mutex																MdProcRetMutex; // 过程结果集的锁
																			
public:
	CRpcServer(int HeartBeatTime = 300, int ServiceMaxPeoples = 100, int DisposeThreadNums = 1);
	virtual ~CRpcServer();
	void MfStart(const char* ip, unsigned short port, int threadNums = 3);	// 启动收发线程和线程池,threadNums代表线程池线程数量
	int MfRegCall(int CallNo, RemoteProc Call, int ArgLen, int RetLen);		// 注册一个过程
	int MfRemoveCall(int CallNo);											// 移除一个过程
private:
	void MfCallRetrun();
	virtual void MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid);
};

class CRpcClient :private CClientLinkManage
{
private:
public:
	CRpcClient();
	virtual ~CRpcClient();
	void MfStart();																					// 启动收发线程
	int MfConnectRpcServer(std::string Linkname, const char* ip, unsigned short port);				// 连接Rpc服务
	void MfCloseRpclink(std::string Linkname);														// 关闭和Rpc的连接
	std::shared_ptr<char[]> MfRemote(std::string Linkname, int CallNo, void* data, int DataSize);	// 远程过程调用，阻塞等待
private:
	virtual void MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid);
};