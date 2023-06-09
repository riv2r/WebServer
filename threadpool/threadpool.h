#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "../lock/locker.h"
#include "../mysql/mysql_conn_pool.h"

template <typename T>
class threadpool
{
public:
    // thread_number 线程池线程数量 max_requests 请求队列最多允许等待处理的请求数量
    threadpool(mysql_conn_pool* connPool,int thread_number=8,int max_requests=10000);
    ~threadpool();
    bool append(T* request);
private:
    static void* worker(void* arg);
    void run();
private:
    int m_thread_number;            // 线程池中的线程数
    int m_max_requests;             // 请求队列中允许的最大请求数
    pthread_t* m_threads;           // 描述线程池的数组
    std::list<T*> m_workqueue;      // 请求队列
    locker m_queuelocker;           // 互斥锁
    sem m_queuestat;                // 是否有任务待处理
    bool m_stop;                    // 是否结束线程
    mysql_conn_pool* m_connPool;    // 数据库
};

template <typename T>
threadpool<T>::threadpool(mysql_conn_pool* connPool,int thread_number,int max_requests):m_thread_number(thread_number),m_max_requests(max_requests),m_threads(NULL),m_stop(false),m_connPool(connPool)
{
    if(thread_number<=0 || max_requests<=0) throw std::exception();

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) throw std::exception();

    for(int i=0;i<thread_number;++i)
    {
        if(pthread_create(m_threads+i,NULL,worker,this)!=0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        // 线程分离
        if(pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop=true;
}

template <typename T>
bool threadpool<T>::append(T* request)
{
    m_queuelocker.lock();
    if(m_workqueue.size()>m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void* threadpool<T>::worker(void* arg)
{
    threadpool* pool=(threadpool*) arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while(!m_stop)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request=m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request) continue;
        connRAII mysqlConn(&request->conn,m_connPool);
        request->process();
    }
}

#endif
