#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);
private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    int m_actor_model;          //模型切换
};
template <typename T>/* deng: 以下主要是创建线程，并且线程分离 */
threadpool<T>::threadpool( int actor_model,  int thread_number, int max_requests) : m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            /* deng: 线程创建失败，一个失败都不行 */
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
             /* deng: 线程分离失败，一个失败都不行*/
            delete[] m_threads;
            throw std::exception();
        }
    }
}
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}
template <typename T>
bool threadpool<T>::append(T *request, int state)// deng: state : 表示读， 1表示写。
{
    /* deng: 往队列里添加request */
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    /* dneg: 增加一个信号量 */
    m_queuestat.post();

    return true;
}

template <typename T> /* deng: 这里可不可以直接用pool里的成员函数，值的思考？： 不用思考了不可以。 */
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        /* dneg: 等待信号量，若有则信号量减1 */
        m_queuestat.wait();

        /* 从队列里拿出request */
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;


        if (1 == m_actor_model)
        {
            /* degn: m_state;  //读为0, 写为1 */
            if (0 == request->m_state)
            {
                // LOG_DEBUG("reactor: 从客户端读取到数据");
                if (request->read_once())
                {
                    // printf("读取到数据\r\n");
                    request->improv = 1;
                    request->process();
                }
                else //deng： 没有读取到数据， 断开连接
                {
                    // printf("没有读取到数据\r\n");
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                // LOG_DEBUG("reactor: 写数据到客户端");
                if (request->write() == false) //deng: 写完数据后还保持连接。
                {
                    request->timer_flag = 1;
                }
                request->improv = 1;
            }
        }
        else
        {
            request->process();
        }
    }
}
#endif
