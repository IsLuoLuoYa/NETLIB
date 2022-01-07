#include <fstream>
#include "Log.h"
#include "ThreadManage.h"
#include "Time.h"
#include "win_linux_HEAD.h"
#include "BaseControl.h"
#include "ThreadPool.h"


// CDoubleBufLogQueue_FairySun类
CDoubleBufLogQueue::CDoubleBufLogQueue():MdMtx(nullptr), MdBuf1(nullptr), MdBuf2(nullptr)
{
	MdMtx = new std::mutex;
	MdBuf1 = new std::queue<CLogMsg*>;
	MdBuf2 = new std::queue<CLogMsg*>;
}

CDoubleBufLogQueue::~CDoubleBufLogQueue()
{
	if(nullptr != MdMtx)
		delete MdMtx;
	MdMtx = nullptr;

	if (nullptr != MdBuf1)
		delete MdBuf1;
	MdBuf1 = nullptr;

	if (nullptr != MdBuf2)
		delete MdBuf2;
	MdBuf2 = nullptr;
}

inline std::mutex* CDoubleBufLogQueue::MfGetMtx()
{
	return MdMtx;
}

inline std::queue<CLogMsg*>* CDoubleBufLogQueue::MfGetBuf1()
{
	return MdBuf1;
}

inline std::queue<CLogMsg*>* CDoubleBufLogQueue::MfGetBuf2()
{
	return MdBuf2;
}

void CDoubleBufLogQueue::MfSwapBuf()
{
	std::lock_guard<std::mutex> lk(*MdMtx);
	auto temp = MdBuf1;
	MdBuf1 = MdBuf2;
	MdBuf2 = temp;
	return;
}

// CMyLog_FairySun类
CLog*											CLog::MdPInstance = nullptr;
CMemoryPool<CLogMsg>							CLog::MdLogMsgObjectPool(1000);
std::thread::id									CLog::MdLogThreadId{};
std::map<std::thread::id, CDoubleBufLogQueue>	CLog::MdThreadIdAndLogQueue{};
std::atomic<long long int>						CLog::MdMsgId{};

CLog::~CLog()
{
	if (!MdPInstance)
		delete MdPInstance;
	MdPInstance = nullptr;
}

CLog* CLog::MfGetInstance()
{
	if (!MdPInstance)
	{
		MdPInstance = new CLog;
		LogThread->MfEnqueue(MfThreadRun);
	}
	return MdPInstance;
}

inline void CLog::MfThreadRun()
{
	MdLogThreadId = std::this_thread::get_id();
	CLog::MdThreadIdAndLogQueue[MdLogThreadId];

	char LogDirName[128] = "LogDir";				// 日志文件夹名字
#ifndef WIN32
	int IsError = mkdir(LogDirName, S_IRWXU);		// 创建日志文件夹
	if (IsError)
		printf("Create Dir failed! error code %d: %s \n", IsError, LogDirName);
#else
	if (::_mkdir(LogDirName))						// 创建日志文件夹
		printf("Create Dir failed! %s \n", LogDirName);
#endif // !WIN32

	// 创建日志文件
	// 文件名：器名称、进程名字、进程ID、文件创建时间、后缀.log 
	// 变化的只有文件创建时间，其他可以都存起来
	char LogFileNameFront[128]{};			// 存储机器名称、进程名字、进程ID
	char LogFileNameRear[] = ".log";		// 存储后缀.log 
	char FinalLogFileName[256]{};			// 文件名最终拼接的地方
	char Temp[128];
	
	// 拼接部分文件名，最终文件名在循环中拼接。
#ifndef WIN32
	strcat(LogFileNameFront, "./LogDir/");
	gethostname(Temp, 100);
	strcat(LogFileNameFront, Temp);
	strcat(LogFileNameFront, "--");
#else
	strcat_s(LogFileNameFront, ".\\LogDir\\");
#endif
	strcat(LogFileNameFront, "FairySunOfNetBase--");
	snprintf(Temp, 128, "Pid[%d]--", getpid());
	strcat(LogFileNameFront, Temp);

	strcat(FinalLogFileName, LogFileNameFront);
	strcat(FinalLogFileName, YearMonthDayHourMinuteSecondStr().c_str());
	strcat(FinalLogFileName, LogFileNameRear);


	CTimer		Time;				// 文件重写计时器
	CTimer		TimeSyn;			// 文件同步计时器
	FILE*		LogFileStream;		// 文件流对象
	LogFileStream = fopen(FinalLogFileName, "wb");		// 打开文件流
	if (nullptr == LogFileStream)
	{
		printf("!!!!!Fatal error : create LogFile fail111\n");
		LogFormatMsgAndSubmit(std::this_thread::get_id(), FATAL_FairySun, "!!!!!Fatal error : create LogFile fail111");
		ErrorWaitExit();
	}

	// 每隔一段时间日志重新写一个文件
	printf("LogThread will start.\n");
	while (!LogThread->MfIsStop())
	{
		// 这块代码用来每隔一个小时重新建立一个日志文件
		if (3600 < Time.getElapsedSecond())		
		{
			fclose(LogFileStream);				// 关闭原来的文件流
			memset(FinalLogFileName, 0, 256);	// 清空文件名重新拼接
			strcat(FinalLogFileName, LogFileNameFront);
			strcat(FinalLogFileName, YearMonthDayHourMinuteSecondStr().c_str());
			strcat(FinalLogFileName, LogFileNameRear);
			//printf("LogNewFlieName:%s\n", FinalLogFileName);		// 调试时打印文件名
			LogFileStream = fopen(FinalLogFileName, "wb");			// 打开文件流
			if (nullptr == LogFileStream)
			{
				//printf("!!!!!Fatal error : create NewLogFile fail222\n");
				LogFormatMsgAndSubmit(std::this_thread::get_id(), FATAL_FairySun, "!!!!!Fatal error : create NewLogFile fail222");
				ErrorWaitExit();
			}
			Time.update();
		}

		// 接下来就是每次循环把所有缓冲区先交换，紧随其后通过流把日志消息写入文件
		for (auto it1 = MdThreadIdAndLogQueue.begin(); it1 != MdThreadIdAndLogQueue.end(); ++it1)
		{
			CLogMsg* Temp;
			if (it1->second.MfGetBuf1()->empty() && it1->second.MfGetBuf2()->empty())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
			it1->second.MfSwapBuf();
			while (!it1->second.MfGetBuf1()->empty())
			{
				Temp = it1->second.MfGetBuf1()->front();
				fputs(Temp->MdMsg, LogFileStream);
				//printf("%p ：%s", it1->second.MfGetBuf1(), Temp->MdMsg);
				it1->second.MfGetBuf1()->pop();
				MdLogMsgObjectPool.MfReturnMemory(Temp);
				//delete Temp;
			}
			if (LOG_SYN_TIME_SECOND < TimeSyn.getElapsedSecond())
			{
				fflush(LogFileStream);
				TimeSyn.update();
			}
		}
	}
	
	// 日志线程主循环退出后，应该清空所有缓冲区
	for (auto it1 = MdThreadIdAndLogQueue.begin(); it1 != MdThreadIdAndLogQueue.end(); ++it1)
	{
		CLogMsg* Temp;
		while (!it1->second.MfGetBuf1()->empty())
		{
			Temp = it1->second.MfGetBuf1()->front();
			fputs(Temp->MdMsg, LogFileStream);
			MdLogMsgObjectPool.MfReturnMemory(Temp);
			it1->second.MfGetBuf1()->pop();
		}
		it1->second.MfSwapBuf();
		while (!it1->second.MfGetBuf1()->empty())
		{
			Temp = it1->second.MfGetBuf1()->front();
			fputs(Temp->MdMsg, LogFileStream);
			MdLogMsgObjectPool.MfReturnMemory(Temp);
			it1->second.MfGetBuf1()->pop();
		}
	}

	fclose(LogFileStream);

	// 最后文件关闭后，调用日志排序
	MfLogSort();
}

void CLog::MfSubmitLog(std::thread::id ThreadId, CLogMsg* Log)
{
	std::lock_guard<std::mutex> lk(*(CLog::MdThreadIdAndLogQueue[ThreadId].MfGetMtx()));
	CLog::MdThreadIdAndLogQueue[ThreadId];								// 这一行用来保证map中的第二参数被初始化
	CLog::MdThreadIdAndLogQueue[ThreadId].MfGetBuf2()->push(Log);
}

CLogMsg* CLog::MfApplyLogObject()
{
	return MdLogMsgObjectPool.MfApplyMemory();
}

void CLog::MfLogSort()
{
	std::map<int, CLogMsg*>		LogBuffer;				// 日志暂时全都读到这里
	CMemoryPool<CLogMsg>		LogPool(10000);			// 读日志时的内存池不用频繁申请
	char						temp_str[256]{};		// 临时存字串
	char						temp_FileName[260]{};	// 临时存文件名
	int							temp_id;				// 临时存数字

	// 排序前变量的定义
#ifdef WIN32
	if (0 != ::chdir("LogDir"))						// 切换到日志目录，失败直接返回，WIN是先切目录在获取
	{
		printf("切换日志目录失败\n");
		return;
	}
	intptr_t			Handle;						// Win目录句柄
	_finddata_t			File;						// 大概类似一个文件指针，存储文件名，文件长度，是文件还是类目之类的信息
	Handle = _findfirst("*.*", &File);				// 查找目录中的第一个文件
	if (Handle == -1)								// 如果一个文件也找不到直接返回
		return;
#else
	DIR*				Handle;						// Linux目录句柄
	struct dirent*		File;						// 指向文件的信息
	if (nullptr == (Handle = opendir("LogDir")))	// 目录打开失败直接返回
	{
		printf("日志目录打开失败]\n");
		return;
	}
	if (nullptr == (File = readdir(Handle)))		// 一个文件读不到也直接返回
		return;
#endif // WIN32

	// 排序主循环
	do
	{
#ifdef WIN32
		if (File.attrib & _A_SUBDIR)	// 如果是LogDir下的子目录，直接跳过		// strcmp(findData.name, "..") == 0   是不是那两个特殊目录
			continue;
		else
		{
			strncpy(temp_FileName, File.name, strlen(File.name));
#else
		if (File->d_type & DT_DIR)		// 如果是LogDir下的子目录，直接跳过
			continue;
		else
		{
			if (0 != ::chdir("LogDir"))						// 切换到日志目录，失败直接返回，Linux是先打开目录，然后再切目录打开文件
			{
				printf("切换日志目录失败\n");
				return;
			}
			strncpy(temp_FileName, File->d_name, strlen(File->d_name));

#endif // WIN32
			printf("正在对: < %s >\t排序!\n", temp_FileName);		// findData.size    是文件的长度
			std::fstream		OldFile(temp_FileName);				// 打开要被排序的文件
			CLogMsg*	Temp_Log = nullptr;							// 临时日志的指针
			while (OldFile)															// 一直读到旧日志文件尾
			{
				Temp_Log = LogPool.MfApplyMemory();									// 对象池出一条日志的空间
				OldFile.getline(Temp_Log->MdMsg, LOG_MSG_LEN);						// 读一行出来
				sscanf(Temp_Log->MdMsg + 6, " %d ", &temp_id);						// 读出该条日志的ID
				LogBuffer.insert(std::pair<int, CLogMsg*>(temp_id, Temp_Log));		// 插入数据
			}

			// 新建一个文件，
			temp_str[0] = '\0';		
			strcat(temp_str, "SORTED_");				// 拼接新文件名
			strcat(temp_str, temp_FileName);			// 拼接新文件名
			FILE* filepNew = fopen(temp_str, "wb");		// 创建打开新文件
			if (nullptr == filepNew)
			{
				printf("WARRING : 日志排序文件打开失败\n");
				return;
			}
			for (auto it : LogBuffer)					// 直接范围循环写入新文件，就是按顺序排序。
			{
				fputs(it.second->MdMsg, filepNew);		// 写入一行
				fputc('\n', filepNew);					// 写入换行
				LogPool.MfReturnMemory(it.second);		// 归还对象池空间
				//it.second = nullptr;
			}

			fclose(filepNew);
			LogBuffer.clear();							// 清理map下一轮循环继续用
			printf("对文件: < %s >\t排序完成!\n", temp_FileName);
		}
#ifdef WIN32
	}while (_findnext(Handle, &File) == 0);			// 查找目录中的下一个文件
	_findclose(Handle);								// 清理文件句柄
#else
	}while ((File = readdir(Handle)) != NULL);		// 查找目录中的下一个文件
	closedir(Handle);								// 清理文件句柄
#endif
}