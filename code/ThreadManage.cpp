#include "ThreadManage.h"
#include "Log.h"

// 静态成员初始化
//CThreadManage*						CThreadManage::MdPInstance = nullptr;
//std::map<std::thread::id, bool>		CThreadManage::MdAllIdAndStopFlag{};
//std::thread::id						CThreadManage::MdLogThreadId;
//std::atomic<bool>					CThreadManage::MdLogThreadStopFlag{};
//Barrier								CThreadManage::MdBarrier1;
//Barrier								CThreadManage::MdBarrier2;
//
//CThreadManage* CThreadManage::MfGetInstance()
//{
//	if (!MdPInstance)
//		MdPInstance = new CThreadManage;
//	return MdPInstance;
//}
//
//void CThreadManage::MfCreateLogThreadAndDetach(void(*p)())
//{
//	MfGetInstance();				// 调用一次保证对象被构造
//	std::thread t(p);
//	MdLogThreadId = t.get_id();
//	MdLogThreadStopFlag = true;
//	t.detach();
//}
//
//bool CThreadManage::MfGetLogThreadFlag()
//{
//	return MdLogThreadStopFlag;
//}
//
//void CThreadManage::MfCreateThreadAndDetach(std::function<void(void)> p)
//{
//	MfGetInstance();				// 调用一次保证对象被构造
//	std::thread t(CThreadManage::MfPackerFun, p);
//	MdAllIdAndStopFlag[t.get_id()] = true;
//	t.detach();
//}
//
//bool CThreadManage::MfGetThreadFlag(std::thread::id threadid)					// 取得某个线程的flag
//{
//	if (MdPInstance != nullptr)
//		return MdAllIdAndStopFlag[threadid];
//	return false;
//}
//
//void CThreadManage::MfPackerFun(std::function<void(void)> p)
//{
//	p();
//	MdBarrier1.MfWait();
//}
//
//void CThreadManage::MfThreadManageOver()
//{
//	printf("ThreadManage OverIng Program!\n");
//	LogFormatMsgAndSubmit(std::this_thread::get_id(), INFO_FairySun, "ThreadManage OverIng Program!");
//
//	// 这一块代码用来在结束日志线程前等待并结束其他线程!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//	MdBarrier1.MfInit((int)MdAllIdAndStopFlag.size() + 1);
//	for (auto it = MdAllIdAndStopFlag.begin(); it != MdAllIdAndStopFlag.end(); ++it)		// 停止日志线程之外的其他线程
//		it->second = false;
//	MdBarrier1.MfWait();
//	printf("all thread arrived barried.\n");
//
//
//	// 这一块代码用来等待和结束日志线程!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//	MdBarrier2.MfInit(2);
//	MdLogThreadStopFlag = false;			// 停止日志线程
//	MdBarrier2.MfWait();
//	if (!MdPInstance)
//	{
//		delete MdPInstance;
//		MdPInstance = nullptr;
//	}
//}

// BarrierV2类
Barrier::~Barrier()
{
#ifndef WIN32
	pthread_barrier_destroy(&MdBarrier);
#endif // WIN32
}

void Barrier::MfInit(int target)
{
	MdTarget = target;
#ifdef WIN32
	MdBarrier = 0;
#else
	pthread_barrier_init(&MdBarrier, nullptr, MdTarget);
#endif // WIN

}

void Barrier::MfWait()
{
#ifdef WIN32
	++MdBarrier;
	while (MdBarrier.load() < MdTarget)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
#else
	pthread_barrier_wait(&MdBarrier);
#endif // WIN32
}