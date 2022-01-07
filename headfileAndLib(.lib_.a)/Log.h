#pragma once
#include <mutex>
#include <queue>
#include <map>
#include <string.h>
#include <thread>
#include <atomic>
#include "ObjectPoolg.h"
#include "win_linux_HEAD.h"
const int LOG_SYN_TIME_SECOND = 3;		// 日志同步时间
const int LOG_MSG_LEN = 256;			// 日志消息长度

struct CLogMsg
{
	char MdMsg[LOG_MSG_LEN];
};

enum EMsgLevel
{
	DEBUG_FairySun,
	INFO_FairySun,
	WARN_FairySun,
	ERROR_FairySun,
	FATAL_FairySun
};

class CDoubleBufLogQueue
{
private:
	std::mutex*				MdMtx;		// 交换两个队列时使用的互斥元
	std::queue<CLogMsg*>*	MdBuf1;		// 日志读线程使用buf1
	std::queue<CLogMsg*>*	MdBuf2;		// 另外的写进程使用buf2

public:
	CDoubleBufLogQueue();
	~CDoubleBufLogQueue();
	inline std::mutex* MfGetMtx();
	inline std::queue<CLogMsg*>* MfGetBuf1();
	inline std::queue<CLogMsg*>* MfGetBuf2();
	void MfSwapBuf();
};

template <class T>
class CMemoryPool;

class CLog
{
private:
	static CLog*											MdPInstance;			// 单例对象指针
public:
	static std::thread::id									MdLogThreadId;			// 当前日志线程ID
	static CMemoryPool<CLogMsg>								MdLogMsgObjectPool;		// 日志消息对象池
	static std::map<std::thread::id, CDoubleBufLogQueue>	MdThreadIdAndLogQueue;	// 每个线程ID对应双缓冲的消息队列
	static std::atomic<long long int>						MdMsgId;				// 消息Id

private:
	CLog() {};								
public:
	~CLog();
	CLog(const CLog&) = delete;
	CLog& operator=(const CLog&) = delete;
	
public:
	static CLog* MfGetInstance();			// 返回对象指针
	static void MfThreadRun();				// 遍历所有双缓冲的消息队列，并写入文件，同时负责生成日志目录和日志文件
	static void MfSubmitLog(std::thread::id ThreadId, CLogMsg* Log);		// 真正提交日志的函数
	static CLogMsg* MfApplyLogObject();		// 该函数存在的意义，因为LogFormatMsgAndSubmit需要模板可变参，但是MdLogMsgObjectPool是静态成员，定义在头文件会报重定义错，模板函数写在cpp文件又会无法解析，所以写个函数包一层
	static void MfLogSort();				// 日志排序
};

template <typename ...Arg>
void LogFormatMsgAndSubmit(std::thread::id Id, EMsgLevel Level,  const char* format, Arg... args)	// 暴露给外部按格式生成日志并提交的函数
{
	const time_t t = time(NULL);
	struct tm Time;
#ifndef WIN32
	localtime_r(&t, &Time);
#else
	localtime_s(&Time, &t);
#endif
	CLogMsg* Temp = CLog::MfApplyLogObject();
	//CLogMsg_FairySun* Temp = new CLogMsg_FairySun;
	switch (Level)
	{
	case DEBUG_FairySun:
		snprintf(Temp->MdMsg, LOG_MSG_LEN, "MsgId:%20lld\t%6s\t%4d-%02d-%02d_%02d-%02d-%02d\tTid:%11x\t", ++CLog::MdMsgId, "DEBUG", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday, Time.tm_hour, Time.tm_min, Time.tm_sec, std::hash<std::thread::id>{}(std::this_thread::get_id()));
		break;
	case INFO_FairySun:
		snprintf(Temp->MdMsg, LOG_MSG_LEN, "MsgId:%20lld\t%6s\t%4d-%02d-%02d_%02d-%02d-%02d\tTid:%11x\t", ++CLog::MdMsgId, "INFO", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday, Time.tm_hour, Time.tm_min, Time.tm_sec, std::hash<std::thread::id>{}(std::this_thread::get_id()));
		break;
	case WARN_FairySun:
		snprintf(Temp->MdMsg, LOG_MSG_LEN, "MsgId:%20lld\t%6s\t%4d-%02d-%02d_%02d-%02d-%02d\tTid:%11x\t", ++CLog::MdMsgId, "WARN", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday, Time.tm_hour, Time.tm_min, Time.tm_sec, std::hash<std::thread::id>{}(std::this_thread::get_id()));
		break;
	case ERROR_FairySun:
		snprintf(Temp->MdMsg, LOG_MSG_LEN, "MsgId:%20lld\t%6s\t%4d-%02d-%02d_%02d-%02d-%02d\tTid:%11x\t", ++CLog::MdMsgId, "ERROR", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday, Time.tm_hour, Time.tm_min, Time.tm_sec, std::hash<std::thread::id>{}(std::this_thread::get_id()));
		break;
	case FATAL_FairySun:
		snprintf(Temp->MdMsg, LOG_MSG_LEN, "MsgId:%20lld\t%6s\t%4d-%02d-%02d_%02d-%02d-%02d\tTid:%11x\t", ++CLog::MdMsgId, "FATAL", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday, Time.tm_hour, Time.tm_min, Time.tm_sec, std::hash<std::thread::id>{}(std::this_thread::get_id()));
		break;
	}
	auto TempInt = strlen(Temp->MdMsg);
#ifdef WIN32
	_snprintf_s(Temp->MdMsg + TempInt, LOG_MSG_LEN - TempInt - 1, 255, format, args...);
#else
	snprintf(Temp->MdMsg + TempInt, LOG_MSG_LEN - TempInt - 1, format, args...);
#endif // !WIN32
	strcat(Temp->MdMsg, "\n");
	CLog::MfSubmitLog(Id, Temp);
}