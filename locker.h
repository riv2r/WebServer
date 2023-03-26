#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <semaphore.h>

// 信号量
class sem
{
    public:
    sem()
    {
        if(sem_init(&m_sem,0,0)!=0) throw std::exception();
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    // P 加锁 将信号量减1
    // 若为0 阻塞该进程
    bool wait()
    {
        return sem_wait(&m_sem)==0;
    }
    bool post()
    {
        return sem_post(&m_sem)==0;
    }
    private:
    sem_t m_sem;
};

#endif