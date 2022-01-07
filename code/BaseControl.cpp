#include <thread>
#include "ThreadManage.h"
#include "Log.h"
#include "win_linux_HEAD.h"
#include "RpcCS.h"
#include "ThreadPool.h"
#include "BaseControl.h"

CThreadPool* LogThread = nullptr;

void FairySunOfNetBaseStart()
{
	if (nullptr == LogThread)
		LogThread = new CThreadPool;
	LogThread->MfStart(1);
	CLog::MfGetInstance();						// 实例化日志对象，并启动日志线程
	LogFormatMsgAndSubmit(std::this_thread::get_id() ,INFO_FairySun, "test");		// 提交一条日志保证启动

#ifdef WIN32
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA wsdata;
	WSAStartup(sockVersion, &wsdata);
#else
	sigset_t set;
	if (-1 == sigemptyset(&set))
	{
		LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "sigemptyset error\n");
		ErrorWaitExit(0);
	}
	if (-1 == sigaddset(&set, SIGPIPE))
	{
		LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "sigaddset error\n");
		ErrorWaitExit(0);
	}
	if (-1 == sigprocmask(SIG_BLOCK, &set, nullptr))
	{
		LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "sigprocmask error\n");
		ErrorWaitExit(0);
	}
	//if (SIG_ERR == signal(SIGPIPE, SIGPIPE_Dispose))
	//{
	//	printf("signal_fun set signal_dispose_fun fail\n");
	//	LogFormatMsgAndSubmit(std::this_thread::get_id(), INFO_FairySun, "signal_fun set signal_dispose_fun fail\n");
	//}
	signal(SIGHUP, SIGHUP_Dispose);
	signal(SIGINT, SIGHUP_Dispose);
	signal(SIGKILL, SIGHUP_Dispose);
#endif // !WIN32

}

void FairySunOfNetBaseOver()
{
	if (nullptr != LogThread)
	{
		delete LogThread;
		LogThread = nullptr;
	}

#ifdef WIN32
	WSACleanup();
#endif // !WIN32
}

#ifndef WIN32
void SIGPIPE_Dispose(int no)
{
	//printf("system produce SIGPIPE signal.\n");
	LogFormatMsgAndSubmit(std::this_thread::get_id(), WARN_FairySun, "system produce SIGPIPE signal.");
}

void SIGHUP_Dispose(int no)
{
	FairySunOfNetBaseOver();
}
#endif

extern const int LOG_SYN_TIME_SECOND;

void ErrorWaitExit(int val)
{
	std::this_thread::sleep_for(std::chrono::seconds(LOG_SYN_TIME_SECOND + 1));
	exit(val);
}