#include "ClientLinkManage.h"
#include "NetMsgStruct.h"
#include "Time.h"
#include "ClientLink.h"
#include "Log.h"

CClientLinkManage::CClientLinkManage(int HeartSendInterval):
	MdBarrier(nullptr),
	MdHeartSendInterval(HeartSendInterval),
	MdDefautHeartPacket(nullptr),
	MdHeartTime(nullptr)
{
	MdBarrier = new Barrier;
	MdDefautHeartPacket = new CNetMsgHead;
	MdHeartTime = new CTimer;
}

CClientLinkManage::~CClientLinkManage()
{
	{
		std::unique_lock<std::shared_mutex> lk(MdClientLinkListMtx);
		for (auto it = MdClientLinkList.begin(); it != MdClientLinkList.end();)
		{
			it->second->MfClose();
			MdClientLinkList.erase(it++);
		}
	}
			
	if (nullptr != MdBarrier)
		delete MdBarrier;
	MdBarrier = nullptr;

	if (nullptr != MdDefautHeartPacket)
		delete MdDefautHeartPacket;
	MdDefautHeartPacket = nullptr;

	if (nullptr != MdHeartTime)
		delete MdHeartTime;
	MdHeartTime = nullptr;
}

void CClientLinkManage::MfStart()
{
	MdBarrier->MfInit(3);
	MdThreadPool.MfStart(2);
	// 启动收发线程,需要做的是在启动收发线程前不应该建立连接
	MdThreadPool.MfEnqueue(std::bind(&CClientLinkManage::MfSendThread, this));
	MdThreadPool.MfEnqueue(std::bind(&CClientLinkManage::MfRecvThread, this));
	MdBarrier->MfWait();
	MdIsStart = 1;
}

int CClientLinkManage::MfCreateAddLink(std::string Linkname, const char* ip, unsigned short port)
{
	if (!MdIsStart.load())
	{
		printf("启动收发线程后再创建连接。\n");
		return SOCKET_ERROR;
	}

	{
		std::shared_lock<std::shared_mutex> lk(MdClientLinkListMtx);
		if (MdClientLinkList.find(Linkname) != MdClientLinkList.end())
		{
			printf("service <%s> already existed!\n", Linkname.c_str());
			LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "service <%s> already existed ", Linkname.c_str());
			return SOCKET_ERROR;
		}
	}

	CClientLink* temp = new	CClientLink(Linkname);
	int ret = temp->MfConnect(ip, port);
	if (SOCKET_ERROR != ret)	// 成功连接后就加入正式队列
	{
		std::unique_lock<std::shared_mutex> lk(MdClientLinkListMtx);
		MdClientLinkList.insert(std::pair<std::string, CClientLink*>(Linkname, temp));
		return ret;
	}
	delete temp;
	return SOCKET_ERROR;
}

void CClientLinkManage::MfCloseLink(std::string Linkname)
{
	std::unique_lock<std::shared_mutex> lk(MdClientLinkListMtx);
	auto it = MdClientLinkList.find(Linkname);
	if (it != MdClientLinkList.end())
	{
		it->second->MfClose();
		MdClientLinkList.erase(it);
	}
}

bool CClientLinkManage::MfSendData(std::string name, const char* data, int len)
{
	return MdClientLinkList[name]->MfDataToBuffer(data, len);
}

const char* CClientLinkManage::MfGetRecvBufferP(std::string name)
{
	return MdClientLinkList[name]->MfGetRecvBufferP();
}

bool CClientLinkManage::MfHasMsg(std::string name)
{
	return MdClientLinkList[name]->MfHasMsg();
}

void CClientLinkManage::MfPopFrontMsg(std::string name)
{
	return MdClientLinkList[name]->MfPopFrontMsg();
}

bool CClientLinkManage::MfLinkIsSurvive(std::string name)
{
	std::shared_lock<std::shared_mutex> lk(MdClientLinkListMtx);
	auto it = MdClientLinkList.find(name);
	return it != MdClientLinkList.end() && it->second->MfGetIsConnect();
}

void CClientLinkManage::MfSendThread()
{
	std::thread::id threadid = std::this_thread::get_id();
	printf("client send thread already start.\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "client send thread already start.");

	MdBarrier->MfWait();

	// 主循环
	while (!MdThreadPool.MfIsStop())
	{
		// 为每个连接发送心跳包,这里是把心跳包加入第二缓冲区，之后一起由循环整个一起发送
		if (MdHeartTime->getElapsedSecond() > MdHeartSendInterval)
		{
			std::shared_lock<std::shared_mutex> lk(MdClientLinkListMtx);
			for (auto it = MdClientLinkList.begin(); it != MdClientLinkList.end(); ++it)
				it->second->MfDataToBuffer((const char*)MdDefautHeartPacket, sizeof(CNetMsgHead));
			MdHeartTime->update();
		}

		// 加括号使锁提前释放
		{
			std::shared_lock<std::shared_mutex> lk(MdClientLinkListMtx);
			for (auto it = MdClientLinkList.begin(); it != MdClientLinkList.end(); ++it)
			{
				it->second->MfSend();		// 出错移除操作放在recv进程操作
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	printf("client send thread already over.\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "client send thread already over.");
}

void CClientLinkManage::MfRecvThread()
{
	std::thread::id threadid = std::this_thread::get_id();
	printf("client recv thread already start.\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "client recv thread already start.");
	std::map<std::string, CClientLink*>::iterator TmpIt;

	MdBarrier->MfWait();

	// 主循环
	while (!MdThreadPool.MfIsStop())
	{
		{
			std::shared_lock<std::shared_mutex> lk(MdClientLinkListMtx);
			for (auto it = MdClientLinkList.begin(); it != MdClientLinkList.end();)
			{
				TmpIt = it++;
				if (0 >= TmpIt->second->MfRecv())
				{
					std::unique_lock<std::shared_mutex> uk(MdClientLinkListMtx, std::adopt_lock);
					lk.release();		// 解除lk和互斥元的关联，这样lk析构时就不会第二次对互斥元解锁了
					TmpIt->second->MfClose();
					delete TmpIt->second;
					MdClientLinkList.erase(TmpIt->first);
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	printf("client recv thread already over.\n");
	LogFormatMsgAndSubmit(threadid, INFO_FairySun, "client recv thread already over.");
}