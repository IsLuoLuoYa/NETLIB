#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>
#include <future>


class CThreadPool 
{
private:
    std::vector<std::thread>            MdPool;             // 线程池
    std::queue<std::function<void()>>   MdTasks;            // 提交的任务队列
    std::mutex                          MdQueueMutex;       // 队列的锁
    std::condition_variable             MdQueueCondition;   // 队列的条件变量
    std::atomic<bool>                   MdIsStop;           // 队列停止时使用
public:
    CThreadPool();
    ~CThreadPool();
    void MfStart(size_t threads = 5);
    bool MfIsStop() { return MdIsStop; };
private:
    void MfTheadFun();
public:
    template<class F, class... Args>
    auto MfEnqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
};


// 后置返回类型，提取出参数F的返回值类型
// 模板成员需要写在,h中
template<class F, class... Args>
auto CThreadPool::MfEnqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(MdQueueMutex);
        if (MdIsStop)
            printf("MfEnqueue on MdIsStopped ThreadPool");
        MdTasks.emplace([task]() { (*task)(); });
    }
    MdQueueCondition.notify_one();
    return res;
}