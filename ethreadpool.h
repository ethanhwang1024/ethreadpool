#ifndef ETHREADPOOL_H
#define ETHREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <unordered_map>
#include <future>


#define TASK_MAX 4096
#define INVALID_THREAD_SIZE 0
const unsigned int INIT_THREAD_SIZE = std::thread::hardware_concurrency();

enum class PoolMode{
    MODE_FIXED,
    MODE_CACHED,
};

class Thread{
public:
    //线程函数对象类型
    using ThreadFunc = std::function<void(int)>;
    explicit Thread(ThreadFunc func);
    ~Thread() = default;
    void start();
    int getId()const;
private:
    ThreadFunc _func;
    static int _generateId;
    int _threadId;
};


class ThreadPool{
public:
    ThreadPool(PoolMode pm=PoolMode::MODE_FIXED,unsigned int initThreadSize = INIT_THREAD_SIZE,
            unsigned int maxThreadSize = INVALID_THREAD_SIZE ,unsigned int taskMax = TASK_MAX);
    ~ThreadPool();
    void setTaskQMaxThreshold(int threshold);

    template<typename Func, typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>{
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::future<RType> result = task->get_future();

        std::unique_lock<std::mutex> lock(_taskQMtx);
        if(!_notFull.wait_for(lock,std::chrono::seconds(1),
                              [&]()->bool {return _taskQ.size()<_taskQMaxThreshold;})){
            printf("task queue is full,submit task fail.");
            auto tmpTask = std::make_shared<std::packaged_task<RType()>>
                    ([]()-> RType {return RType();});
            (*tmpTask)();
            return tmpTask->get_future();
        }
        std::function<void()> fuc = [=](){(*task)();};
        _taskQ.emplace(fuc);
        _taskSize++;
        _cv.notify_all();
        if(_poolMode == PoolMode::MODE_CACHED && _taskSize > _idleThreadSize){
            //add thread
            for(int i=0;i<_taskSize-_idleThreadSize;i++){
                if(_currentThreadSize>=_maxThreadSize){
                    break;
                }
                auto ptr = std::make_shared<Thread>([this](int threadId){ threadFunc(threadId); });
                int threadId = ptr->getId();
                _threads.emplace(ptr->getId(),std::move(ptr));
                _threads[threadId]->start();
                _idleThreadSize ++;
                _currentThreadSize ++;
            }
        }
        return result;
    }

    void start();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    unsigned int getCurrentThreadSize()const{
        return _currentThreadSize;
    };
private:
    void threadFunc(int threadId);
private:
    std::unordered_map<int,std::shared_ptr<Thread>> _threads;
    unsigned int _initThreadSize;
    using Task = std::function<void()>;
    std::queue<Task> _taskQ;
    std::atomic_uint _taskSize;
    unsigned int _taskQMaxThreshold;
    std::mutex _taskQMtx;
    std::mutex _threadFuxMtx;
    std::mutex _destroyMtx;
    std::condition_variable _notFull;
    std::condition_variable _cv;
    std::condition_variable _exit;
    PoolMode _poolMode;
    bool _isRunning;
    std::atomic_int _idleThreadSize;
    unsigned int _maxThreadSize;
    std::atomic_int  _currentThreadSize;
};

#endif