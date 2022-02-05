#include "ethreadpool.h"
#include <functional>
#include <thread>


const int THREAD_MAX_IDLE_TIME = 30;

//--------------------> for ThreadPool <-----------------
ThreadPool::ThreadPool(PoolMode pm,unsigned int initThreadSize,unsigned int maxThreadSize,
                       unsigned int taskMax)
    :_initThreadSize(initThreadSize)
    ,_maxThreadSize(maxThreadSize)
    ,_taskSize(0)
    ,_taskQMaxThreshold(taskMax)
    ,_poolMode(pm)
    ,_isRunning(false)
    ,_idleThreadSize(0)
    ,_currentThreadSize((int)initThreadSize)
{
    if(pm==PoolMode::MODE_CACHED){
        if(_maxThreadSize == INVALID_THREAD_SIZE){
            _maxThreadSize = initThreadSize * 5;
        }
    }
}

void ThreadPool::setTaskQMaxThreshold(int threshold) {
    _taskQMaxThreshold = threshold;
}

void ThreadPool::start() {
    _isRunning = true;
    //create thread object
    for(int i=0;i<_initThreadSize;i++){
        auto ptr = std::make_shared<Thread>([this](int threadId){ threadFunc(threadId); });
        _threads.emplace(ptr->getId(),std::move(ptr));
    }
    //start all
    for(int i=0;i<_initThreadSize;i++){
        _threads[i] -> start();
        _idleThreadSize ++;
    }

}

void ThreadPool::threadFunc(int threadId)  {
    auto lastTime = std::chrono::high_resolution_clock::now();
   for(;;){
       std::function<void()> task;
       {
           std::unique_lock<std::mutex> lock(_threadFuxMtx);
           while (_taskQ.empty()){
               if(!_isRunning){
                   _threads.erase(threadId);
                   _exit.notify_all();
                   return;
               }
               if(_poolMode == PoolMode::MODE_CACHED && _currentThreadSize>_initThreadSize){
                   if(std::cv_status::timeout==
                           _cv.wait_for(lock,std::chrono::seconds(1))){ //wait for 1s then get the lock
                       auto now = std::chrono::high_resolution_clock::now();
                       auto dur = std::chrono::duration_cast<std::chrono::seconds>(now-lastTime);
                       // need to double-check
                       if(dur.count()>=THREAD_MAX_IDLE_TIME && _currentThreadSize>_initThreadSize){
                           _threads.erase(threadId);
                           _currentThreadSize--;
                           _idleThreadSize --;
                           return;
                       }
                   }
               } else{
                   _cv.wait(lock);
               }
           }

           _idleThreadSize--;
            task = _taskQ.front();
           _taskQ.pop();
           _taskSize--;
           if(!_taskQ.empty()){
               _cv.notify_all();
           }
           _notFull.notify_all();
       }
       if(task!= nullptr){
            task();
       }
       _idleThreadSize ++;
       lastTime = std::chrono::high_resolution_clock::now();
   }
}


ThreadPool::~ThreadPool() {
    _isRunning = false;
    std::unique_lock<std::mutex> lock(_destroyMtx);
    _cv.notify_all();
    _exit.wait(lock,[&]()->bool {return _threads.empty();});
}


//--------------------> for Thread <-----------------
Thread::Thread(Thread::ThreadFunc func): _func(std::move(func))
,_threadId(_generateId++)
{}

void Thread::start() {
    //创建一个线程
    std::thread t(_func,_threadId);
    t.detach(); // pthread_detach
}

int Thread::_generateId = 0;

int Thread::getId() const {
    return _threadId;
}


