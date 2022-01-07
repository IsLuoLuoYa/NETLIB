#pragma once

// 主要处理库的构造和析构问题

class CThreadPool;

// 该线程池只执行日志线程
extern CThreadPool* LogThread;

// 启动函数中，应该先调用下线程管理类的函数让其构造
// 接着调用调用下日志类的函数
void FairySunOfNetBaseStart();

// 结束时，线程管理类对象应该只是最后销毁
// 倒数第二的应该是日志类
// 还有一个问题，如何确定除日志线程外的其他线程确定写完了日志。
void FairySunOfNetBaseOver();

#ifndef WIN32
void SIGPIPE_Dispose(int no);
void SIGHUP_Dispose(int no);
#endif

// 当发生错误时，等待日志同步时间+1秒的时间在退出。
void ErrorWaitExit(int val = 0);
