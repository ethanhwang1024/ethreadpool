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

#define TASK_MAX 4096
#define INVALID_THREAD_SIZE 0
const unsigned int INIT_THREAD_SIZE = std::thread::hardware_concurrency();

class Semaphore{
public:
    Semaphore(int limit=0)
        :_resLimit(limit)
    {}
    ~Semaphore() = default;
    //acquire a semaphore resource
    void wait(){
        std::unique_lock<std::mutex> lock(_mtx);
        if(--_resLimit<0){
            _cond.wait(lock,[&]()->bool {return _resLimit>0;});
        }
    }
    //add a semaphore resource
    void post(){
        std::unique_lock<std::mutex> lock(_mtx);
        if (++_resLimit < 1) {
            _cond.notify_one();
        }
    }
private:
    int _resLimit;
    std::mutex _mtx;
    std::condition_variable _cond;
};


class Any{
public:
    //just show you the default status
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    template<typename T>
    Any(T data):_base(std::make_unique<Derive<T>>(data)){};

    template<typename T>
    T getData(){
        auto *pd = dynamic_cast<Derive<T>*>(_base.get());
        return pd->getData();
    }
private:
    class Base{
    public:
        virtual ~Base() = default;
    };
    template<typename T>
    class Derive:public Base{
    public:
        Derive(T data):_data(data){}
        T getData(){
            return _data;
        }
    private:
        T _data;
    };
private:
    std::unique_ptr<Base> _base;
};

class Result;
class Task{
public:
    Task() = default;
    ~Task() = default;
    virtual Any run() = 0;
    void set(Result *result){
        _result = result;
    }
    Result* get(){
        return _result;
    }
private:
    Result* _result= nullptr;
};

class Result{
public:
    explicit Result(std::shared_ptr<Task> task,bool isValid=true);
    ~Result() = default;
    void setVal(Any any);
    Any get();
private:
    Any _any;
    Semaphore _sem;
    std::shared_ptr<Task> _task;
    std::atomic_bool _isValid;
};


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
    Result submitTask(const std::shared_ptr<Task>& sp);
    void start();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    bool checkRunning() const;
    unsigned int getCurrentThreadSize()const{
        return _currentThreadSize;
    };
private:
    void threadFunc(int threadId);
private:
    std::unordered_map<int,std::shared_ptr<Thread>> _threads;
    unsigned int _initThreadSize;
    std::queue<std::shared_ptr<Task>> _taskQ;
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