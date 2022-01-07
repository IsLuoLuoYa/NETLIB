#pragma once
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include "win_linux_HEAD.h"

// 前向声明
class Barrier;

//// 使用时应注意，在线程中，使用线程ID取得该类成员MdAllIdAndStopFlag中的int
//// 通过该int值来作为循环是否执行的条件
//class CThreadManage
//{
//private:
// 	static CThreadManage*					MdPInstance;			// 单例对象指针
//	static std::map<std::thread::id, bool>	MdAllIdAndStopFlag;		// 每个线程的ID对应一个是否运行的bool值
//	static std::thread::id					MdLogThreadId;			// 日志线程ID，特殊对待
//	static std::atomic<bool>				MdLogThreadStopFlag;	// 日志线程标志
//	static Barrier							MdBarrier1;				// 日志线程结束前用于等待其他线程的屏障
//public:	
//	static Barrier							MdBarrier2;				// 等待日志线程结束
//
//private:
//	CThreadManage() {};
//	CThreadManage(const CThreadManage&) = delete;
//	CThreadManage& operator=(const CThreadManage&) = delete;
//public:
//	~CThreadManage() {};
//	static CThreadManage* MfGetInstance();								// 返回对象指针
//	static void MfCreateLogThreadAndDetach(void(*p)());					// 因为日志线程的特殊性，对于日志线程特别启动
//	static bool MfGetLogThreadFlag();									// 取得日志线程的flag
//	static void MfCreateThreadAndDetach(std::function<void(void)> p);	// 使用线程管理对象启动一个普通线程
//	static void MfPackerFun(std::function<void(void)> p);				// 启动线程时用这个函数包裹一下，原因是需要这个函数来实现一个类似屏障的东西
//	static bool MfGetThreadFlag(std::thread::id threadid);				// 取得某个线程的flag
//	static void MfThreadManageOver();									// 最终在程序结束时，由库控制的那个函数调用，行为是停止所有线程，清空日志线程等
//};

class Barrier
{
private:
	int								MdTarget = 0;
#ifdef WIN32
	std::atomic<unsigned int>		MdBarrier;
#else
	pthread_barrier_t				MdBarrier;
#endif // WIN32
public:
	~Barrier();
	void MfInit(int target);
	void MfWait();
};