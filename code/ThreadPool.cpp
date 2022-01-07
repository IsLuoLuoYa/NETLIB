#include "ThreadPool.h"


CThreadPool::CThreadPool() :MdIsStop(false) {}

CThreadPool::~CThreadPool()
{
    MdIsStop = true;
    MdQueueCondition.notify_all();
    for (std::thread& worker : MdPool)
        worker.join();
}

void CThreadPool::MfStart(size_t threads)       // 线程不应该在构造函数中启动，因为这些线程使用了数据成员
{
    for (size_t i = 0; i < threads; ++i)
        MdPool.push_back(std::thread(&CThreadPool::MfTheadFun, this));
}

void CThreadPool::MfTheadFun()
{
    while (1)
    {
        std::function<void()> task;     // 要执行的任务
        {
            std::unique_lock<std::mutex> lock(MdQueueMutex);
            MdQueueCondition.wait(lock, [this] { return this->MdIsStop || !this->MdTasks.empty(); });
            if (this->MdIsStop && this->MdTasks.empty())
                return;
            task = std::move(this->MdTasks.front());
            this->MdTasks.pop();
        }
        task();
    }
}