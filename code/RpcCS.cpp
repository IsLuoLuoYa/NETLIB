#include "RpcCS.h"
#include "Log.h"
#include "Socket.h"
#include  <memory>
#include <future>

CRpcServer::CRpcServer(int HeartBeatTime, int ServiceMaxPeoples, int DisposeThreadNums) :
	CServiceNoBlock(HeartBeatTime, ServiceMaxPeoples, DisposeThreadNums)
{

}

CRpcServer::~CRpcServer()
{

}

void CRpcServer::MfStart(const char* ip, unsigned short port, int threadNums)
{
	MdThreadPool.MfStart(threadNums+1);						// 线程池启动
	MdThreadPool.MfEnqueue(&CRpcServer::MfCallRetrun, this);// 给对应客户端写回结果的线程丢尽线程池执行
	return CServiceNoBlock::Mf_NoBlock_Start(ip, port);		// 网络收发处理启动
}

int CRpcServer::MfRegCall(int CallNo, RemoteProc Call, int ArgLen, int RetLen)
{
	{
		std::lock_guard<std::shared_mutex> write_lock(MdCallListMutex);
		if (MdCallList.find(CallNo) != MdCallList.end())
		{
			printf("RempteProcReg CallNo <%d> already existed!\n", CallNo);
			LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "RempteProcReg CallNo <%d> already existed!\n", CallNo);
			return -1;
		}
		MdCallList[CallNo] = std::make_tuple(Call, ArgLen, RetLen);
	}
	return 0;
}

int CRpcServer::MfRemoveCall(int CallNo)
{
	{
		std::lock_guard<std::shared_mutex> write_lock(MdCallListMutex);
		auto it = MdCallList.find(CallNo);
		if (it != MdCallList.end())
			MdCallList.erase(it);
	}
	return 0;
}

void CRpcServer::MfCallRetrun()
{
	while (!MdProcRet.empty() || !MdThreadPool.MfIsStop())
	{
		RpcMsg ret;
		// for循环中 执行完成的远程调用 的CSocketObj*会被加入该队列，结束后统一从MdProcRet中移除
		std::vector<CSocketObj*> removelist;

		// 遍历MdProcRet，如果有一个过程可以取得结果，就把结果发回对应的socket
		for (auto it = MdProcRet.begin(); it != MdProcRet.end(); ++it)
		{
			if (std::future_status::ready == std::get<1>(it->second).wait_for(std::chrono::milliseconds(1)))		// 如果结果可用
			{
				int procNo = std::get<0>(it->second);
				int procRetSize = std::get<2>(MdCallList[procNo]);
				ret.MdLen = (int)(sizeof(CNetMsgHead) + procRetSize);
				ret.MdCmd = procNo;
				std::shared_ptr<char[]> data = std::get<1>(it->second).get();
				it->first->MfDataToBuffer((char*)&ret, sizeof(CNetMsgHead));	// 先写包头
				if(nullptr != data.get())
					it->first->MfDataToBuffer(data.get(), procRetSize);				// 再写数据
				removelist.push_back(it->first);
			}
		}

		// 将执行完成的远程调用统一移除
		{
			std::unique_lock<std::shared_mutex> write_lock(MdProcRetMutex);
			for (auto it = removelist.begin(); it != removelist.end(); ++it)
			{
				if (MdProcRet.find(*it) != MdProcRet.end())
					MdProcRet.erase(*it);
			}
		}
				
		std::this_thread::sleep_for(std::chrono::seconds(1));	// 暂停一秒防止执行过快
	}
}

void CRpcServer::MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid)
{
	// 注册列表中没有找到对应的过程号，发回一个为0的消息，表示找不到对应过程
	int flag = false;
	RpcMsg ret;
	{
		std::shared_lock<std::shared_mutex> read_lock(MdCallListMutex);
		if (MdCallList.find(msg->MdCmd) != MdCallList.end())
			flag = true;
	}
	if (flag == false)
	{
		ret.MdLen = sizeof(RpcMsg);
		ret.MdCmd = RpcProc_NOON;
		cli->MfDataToBuffer((char*)&ret, ret.MdLen);
	}
	else		// 否则就是找到了对应的过程,保存客户端对象和对应的future，放进结果表
	{
		int arglen = std::get<1>(MdCallList[msg->MdCmd]);
		std::shared_ptr<char[]> buf(new char[arglen]);
		memcpy(buf.get(), ((char*)msg) + sizeof(CNetMsgHead), arglen);
		{
			std::unique_lock<std::shared_mutex> write_lock(MdProcRetMutex);
			MdProcRet[cli] = std::make_tuple
			(
				msg->MdCmd,
				MdThreadPool.MfEnqueue(std::get<0>(MdCallList[msg->MdCmd]), buf)
			);
		}
	}
}

CRpcClient::CRpcClient():
	CClientLinkManage()
{

}

CRpcClient::~CRpcClient()
{

}

void CRpcClient::MfStart()
{
	CClientLinkManage::MfStart();
}

int CRpcClient::MfConnectRpcServer(std::string Linkname, const char* ip, unsigned short port)
{
	return CClientLinkManage::MfCreateAddLink(Linkname, ip, port);
}

void CRpcClient::MfCloseRpclink(std::string Linkname)
{
	CClientLinkManage::MfCloseLink(Linkname);
}

std::shared_ptr<char[]> CRpcClient::MfRemote(std::string Linkname, int CallNo, void* data, int DataSize)
{
	RpcMsg msg;
	msg.MdLen = (int)(sizeof(CNetMsgHead) + DataSize);
	msg.MdCmd = CallNo;
	CClientLinkManage::MfSendData(Linkname, (char*)&msg, sizeof(CNetMsgHead));	// 先写包头
	CClientLinkManage::MfSendData(Linkname, (char*)data, DataSize);				// 再写数据	
	while (1)
	{
		if (!CClientLinkManage::MfHasMsg(Linkname))		// 如果没有数据
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		else
		{
			const char * buff = CClientLinkManage::MfGetRecvBufferP(Linkname);
			if (  ((RpcMsg*)buff)->MdCmd == RpcProc_NOON  )						// 收到了为0的callno
				return nullptr;
			else if (  ((RpcMsg*)buff)->MdCmd != CallNo  )						// 收到的callno和发出去的callno不一样
				return nullptr;
			else
			{
				int retsize = (int)(((RpcMsg*)buff)->MdLen - sizeof(CNetMsgHead));		// 计算返回的数据长度
				char* ret = new char[retsize];									// 申请空间
				memcpy(ret, ((char*)buff) + sizeof(CNetMsgHead) , retsize);			// 复制到新申请的空间
				CClientLinkManage::MfPopFrontMsg(Linkname);						// 缓冲区的消息弹出
				return std::shared_ptr<char[]>(ret);							// 转换成智能指针返回
			}
		}
	}
}

void CRpcClient::MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid)
{

}

