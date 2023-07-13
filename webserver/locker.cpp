/**
 * @Author       : Yang Li
 * @Date         : 2023:07:05
 * @Description  : 线程同步机制封装类的实现
 *
 **/
#include "locker.h"
#include <exception>
// class Locker
Locker::Locker()
{
    if(pthread_mutex_init(&m_mutex,NULL)){
        throw std::exception();
    }
}

Locker::~Locker()
{
    pthread_mutex_destroy(&m_mutex);
}

bool Locker::lock()
{
    return pthread_mutex_lock(&m_mutex) == 0;
}

bool Locker::unlock()
{
    return pthread_mutex_unlock(&m_mutex) == 0;
}

pthread_mutex_t *Locker::get()
{
    return &m_mutex;
}

// class Cond
Cond::Cond()
{
    if(pthread_cond_init(&m_cond, NULL)){
        throw std::exception();
    }
}

Cond::~Cond()
{
    pthread_cond_destroy(&m_cond);
}

bool Cond::wait(pthread_mutex_t *mutex)
{
    return pthread_cond_wait(&m_cond, mutex);
}

bool Cond::timedwait(pthread_mutex_t *mutex, const struct timespec time)
{
    return pthread_cond_timedwait(&m_cond, mutex, &time);
}

bool Cond::signal()
{
    return pthread_cond_signal(&m_cond) == 0;
}

bool Cond::broadcast()
{
    return pthread_cond_broadcast(&m_cond) == 0;
}

// class Sem
Sem::Sem()
{
    if(sem_init(&m_sem, 0, 0)){
        throw std::exception();
    }
}

Sem::Sem(int num)
{
    if(sem_init(&m_sem, 0, num)){
        throw std::exception();
    }
}

Sem::~Sem()
{
    sem_destroy(&m_sem);
}

bool Sem::wait()
{
    return sem_wait(&m_sem) == 0;
}

bool Sem::post()
{
    return sem_post((&m_sem)) == 0;
}
