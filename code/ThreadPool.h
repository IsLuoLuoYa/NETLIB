#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <condition_variable>
#include <functional>
#include <future>
#include <list>


class CThreadPool
{
private:
    std::vector<std::thread>            MdPool;             // �̳߳�
    std::queue<std::function<void()>>   MdTasks;            // �ύ���������
    std::mutex                          MdQueueMutex;       // ���е���
    std::condition_variable             MdQueueCondition;   // ���е���������
    std::atomic<bool>                   MdIsStop;           // ����ֹͣʱʹ��
    int                                 MdCount = 0;
    std::mutex                          MdCountMutex;
public:
    CThreadPool();
    ~CThreadPool();
    void MfStart(size_t threads = 5);
    bool MfIsStop() { return MdIsStop; };
private:
    void MfTheadFun();
public:
    template<class F, class... Args>
    auto MfEnqueue(F&& f, Args&&... args)->std::future<typename std::result_of<F(Args...)>::type>;
};


// ���÷������ͣ���ȡ������F�ķ���ֵ����
// ģ���Ա��Ҫд��,h��
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
        MdTasks.emplace([task]() { (*task)(); });
    }
    MdQueueCondition.notify_one();
    return res;
}