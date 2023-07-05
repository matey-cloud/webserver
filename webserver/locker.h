#ifndef LOCKER_H
#define LOCKER_H
/**
 * @Author       : Yang Li
 * @Date         : 2023:07:05
 * @Description  : 线程同步机制封装类
 *
 **/

#include <pthread.h>
#include <semaphore>
// 互斥锁类
class Locker{
public:
    Locker();
    ~Locker();

    bool lock();
    bool unlock();
    pthread_mutex_t *get();

private:
    pthread_mutex_t m_mutex;

};

// 条件变量类
class Cond
{
public:
    Cond();
    ~Cond();

    bool wait(pthread_mutex_t *mutex);
    bool timedwait(pthread_mutex_t *mutex, const timespec time);
    bool signal() noexcept;          //随机唤醒一个线程
    bool broadcast() noexcept;       // 唤醒所有线程
private:
    pthread_cond_t m_cond;
};


// 信号量类
class Sem{
public:
    Sem();
    Sem(int num);
    ~Sem();

    bool wait();
    bool post();
private:
    sem_t m_sem;
};

#endif // LOCKER_H
