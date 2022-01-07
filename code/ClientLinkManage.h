#pragma once
#include <map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include "NetMsgStruct.h"
#include "ThreadManage.h"
#include "Time.h"
#include "ThreadPool.h"

class CClientLink;
struct CNetMsgHead;
class CTimer;

class CClientLinkManage
{
private:
	std::map<std::string, CClientLink*>		MdClientLinkList;
	std::shared_mutex						MdClientLinkListMtx;
	std::atomic<int>						MdIsStart = 0;			// 收发线程是否启动，不启动时，不能添加连接，因为如果放在构造中启动线程是危险的
	Barrier*								MdBarrier;				// 用于创建连接前的收发线程启动
	int										MdHeartSendInterval;	// 心跳发送时间间隔，单位秒
	CNetMsgHead*							MdDefautHeartPacket;	// 默认的心跳包对象
	CTimer*									MdHeartTime;			// 心跳计时器对象
	CThreadPool								MdThreadPool;
public:
	CClientLinkManage(int HeartSendInterval = 3);
	~CClientLinkManage();
	void MfStart();													// 启动收发线程
	int MfCreateAddLink(std::string Linkname, const char* ip, unsigned short port);		// 如果需要建立新的连接，就new一个ClientLink，同时记录连接的目标，然后加入列表是，设置client可用
	void MfCloseLink(std::string Linkname);							// 关闭某个连接
	bool MfSendData(std::string str, const char* data, int len);	// 发送数据，插入缓冲区
	const char* MfGetRecvBufferP(std::string name);					// 返回接收缓冲区的指针，可以直接读这里的数据
	bool MfHasMsg(std::string name);								// 判断缓冲区是否有数据
	void MfPopFrontMsg(std::string name);							// 缓冲区数据按包弹出
	bool MfLinkIsSurvive(std::string name);							// 某个服务是否活着
private:
	void MfSendThread();											// 发送线程，循环调用send，收发线程根据是否可用标志确定行为
	void MfRecvThread();											// 接收线程，循环调用recv，收发线程根据是否可用标志确定行为
};