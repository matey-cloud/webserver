/**
 * @Author       : Yang Li
 * @Date         : 2023:07:05
 * @Description  : 线程池类的实现
 *
 **/
#include "threadpool.h"
#include <exception>
#include <iostream>

template<typename T>
ThreadPool<T>::ThreadPool(int thread_num, int max_requests)
    : m_thread_num(thread_num), m_max_requests(max_requests),
    m_threads(NULL), m_stop(false)
{
    if(thread_num <= 0 || max_requests <= 0){
        throw std::exception();
    }

    m_threads = new pthread_t(m_thread_num);
    if(!m_threads){
        delete []m_threads;
        throw std::exception();
    }
    // 创建thread_number个线程，并将他们设置为脱离线程
    for(int i = 0 ; i < m_thread_num; ++i){
        std::cout << "create the " << i << "th thread" << std::endl;
        if(pthread_create(m_threads+i, NULL, working, this) != 0){
            delete[] m_threads;
            throw std::exception();
        }

        if(pthread_detach(m_threads[i]) != 0){
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
    delete []m_threads;
    m_stop = true;
}

template<typename T>
bool ThreadPool<T>::append(T *request)
{
    //操作工作队列时一定要加锁，因为它被所有线程共享。
    m_queueLocker.lock();
    if((int)m_workQueue.size() > m_max_requests){
        m_queueLocker.unlock();
        return false;
    }
    m_workQueue.push_back(request);
    m_queueLocker.unlock();
    m_works.post();
    return true;
}

template<typename T>
void *ThreadPool<T>::working(void *arg)
{
    ThreadPool *pool = (ThreadPool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void ThreadPool<T>::run()
{
    while(!m_stop){
        m_works.wait();
        m_queueLocker.lock();
        if(m_workQueue.empty()){
            m_queueLocker.unlock();
            continue;
        }
        T *request = m_workQueue.front();
        m_workQueue.pop_front();
        m_queueLocker.unlock();
        if(!request){
            continue;
        }
        request->process();
    }
}
