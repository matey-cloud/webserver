#ifndef THREADPOOL_H
#define THREADPOOL_H
/**
 * @Author       : Yang Li
 * @Date         : 2023:07:05
 * @Description  : 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类
 *
 **/
#include <pthread.h>
#include <list>
#include "locker.h"

template<typename T>
class ThreadPool
{
public:
    ThreadPool() = delete;
    ThreadPool(int thread_num = 8, int max_requests = 10000);
    ThreadPool(ThreadPool*) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ~threadPool();

    bool append(T *request);

private:
    static void* working(void *arg);
    void run();

private:
    int m_thread_num;           // 线程的数量
    pthread_t *m_threads;       // 描述线程池的数组，大小为m_thread_number
    int m_max_requests;         // 请求队列中最多允许的、等待处理的请求的数量
    std::list<T> m_workQueue;   // 请求队列
    Locker m_queueLocker;       // 保护请求队列的互斥锁
    Sem m_works;                // 是否有任务需要处理
    bool m_stop;                // 是否结束线程
};

#endif // THREADPOOL_H
