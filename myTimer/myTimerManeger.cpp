#include "myTimerManeger.h"

#include "sys/time.h"
#include <iostream>
using namespace std;
#include "../myHeap/myHeap.h"


static const int FD = 2;

bool MyTimerManeger::compare_less(HEAP_DATA_TYPE a, HEAP_DATA_TYPE b)
{
	return a->tv_deadline.tv_sec < b->tv_deadline.tv_sec ||\
		( (a->tv_deadline.tv_sec == b->tv_deadline.tv_sec) && (a->tv_deadline.tv_usec < b->tv_deadline.tv_usec));
}

MyTimerManeger:: MyTimerManeger()
{
    m_myHeap = new MyHeap(compare_less);
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }
    pthread_t t_id;
    if (pthread_create(&t_id, NULL, worker, this) != 0)
    {
        /* deng: 线程创建失败，一个失败都不行 */

        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if (pthread_detach(t_id))
    {
        perror("pthread_detach");
        exit(EXIT_FAILURE);
    }

};

MyTimerManeger:: ~MyTimerManeger()
{
    if(m_myHeap)
        delete m_myHeap;
}


void* MyTimerManeger::worker(void * arg)
{
    MyTimerManeger* myTimerManeger = (MyTimerManeger *)arg;
    myTimerManeger->run();
    return myTimerManeger;
}
void MyTimerManeger::run()
{
    int  nfds;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    // cout<< "tv.tv_sec = "<< tv.tv_sec << "。 tv.tv_usec = " << tv.tv_usec <<endl;
    for (;;) 
    {
        if(m_myHeap->top() == 0)
        {
             nfds = epoll_wait(epollfd, events, 1, -1);
            //  cout<<"无超时退出"<<endl;
        }
         else
         {
            int epoll_wait_timeout = (m_myHeap->top()->tv_deadline.tv_sec - tv.tv_sec)*1000 +  (m_myHeap->top()->tv_deadline.tv_usec/1000 - tv.tv_usec/1000);
            // cout<< ".tv_sec = "<< tv.tv_sec << "。 .tv_usec = " << tv.tv_usec <<endl;
            // cout<< "tv_deadline.tv_sec = "<< m_myHeap->top()->tv_deadline.tv_sec << "。 tv_deadline.tv_usec = " << m_myHeap->top()->tv_deadline.tv_usec <<endl;
            // cout << "epoll_wait_timeout = "<< epoll_wait_timeout << endl;
            // cout << " " << endl;

            nfds = epoll_wait(epollfd, events, 1, epoll_wait_timeout);
            // cout<<"有超时退出"<<endl;
         }
            
        if (nfds == -1) 
        {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }
        // cout<< "epoll_wait 退出"<<endl;
        
        gettimeofday(&tv, NULL);
        if(!((m_myHeap->top()->tv_deadline.tv_sec < tv.tv_sec) || \
        ((m_myHeap->top()->tv_deadline.tv_sec == tv.tv_sec) && (m_myHeap->top()->tv_deadline.tv_usec/1000 <= tv.tv_usec/1000 ))))
        {
            cout<< "epoll_wait : 定时时间未到： tv.tv_sec = "<< tv.tv_sec << "。 tv.tv_usec = " << tv.tv_usec <<endl;
            cout<< "tv_deadline.tv_sec = "<< m_myHeap->top()->tv_deadline.tv_sec << "。 tv_deadline.tv_usec = " << m_myHeap->top()->tv_deadline.tv_usec <<endl;
            //  cout<< "m_myHeap->size() = " << m_myHeap->size() <<endl;
             //直接删除当前监听的文件描述符，不然epoll_wait 会一直返回。
            if (epoll_ctl(epollfd, EPOLL_CTL_DEL, FD, 0)== -1) 
            {
                // perror("epoll_ctl: EPOLL_CTL_DEL error");
                // exit(EXIT_FAILURE);
            }
        }   

        //有定时器的定时时间到了, m_myHeap->top() 可能为空，所有定时器都不删完了
        while(  m_myHeap->top() && \
                ((m_myHeap->top()->tv_deadline.tv_sec < tv.tv_sec) || \
                ((m_myHeap->top()->tv_deadline.tv_sec == tv.tv_sec) && (m_myHeap->top()->tv_deadline.tv_usec/1000 <= tv.tv_usec/1000 ))))
        {
            // cout<< " >tv_deadline.tv_usec/1000 == " << m_myHeap->top()->tv_deadline.tv_usec/1000 << " tv.tv_usec/1000 == "<< tv.tv_usec/1000 << endl;
            // cout<< "m_myHeap->size() = " << m_myHeap->size() <<endl;
             /* 可以直接修改数值的，先不管 */
            TimerInfo * timerInfo = m_myHeap->pop();
            // cout<< "timerInfo->isRuning = "<< timerInfo->isRuning <<endl;
            if(timerInfo->isRuning)
            {
                timerInfo->timeoutCallBack(timerInfo->arg);
                timerInfo->tv_deadline.tv_sec = tv.tv_sec + timerInfo->timoout/1000 + (tv.tv_usec + (timerInfo->timoout%1000)*1000 )/1000000;
                timerInfo->tv_deadline.tv_usec = (tv.tv_usec + (timerInfo->timoout%1000)*1000)%1000000;
                m_myHeap->push(timerInfo);
            }
            else
            {
                 delete timerInfo; //定时器已经被删除，放在这里执行这个感觉有点延时处理的味道，不过时间复杂度Q(1)，不算堆pop操作;
            }
            gettimeofday(&tv, NULL);
            // cout<< "11111"<<endl;
        }
        // cout<< "222222"<<endl;
         
    }
}

  /* 添加定时器： 超时时间，回调函数 */
TimerInfo*  MyTimerManeger::addTimer(int timeout, void *(* timeoutCallBack)(void *arg), void* arg)//超时时间以ms为单位
{

    TimerInfo* timerInfo = new TimerInfo;
    timerInfo->isRuning = true;  
    timerInfo->timoout = timeout;
    timerInfo->timeoutCallBack = timeoutCallBack;
    timerInfo->arg = arg;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    timerInfo->tv_deadline.tv_sec = tv.tv_sec + timeout/1000 + (tv.tv_usec + (timeout%1000)*1000 )/1000000; //可以用浮点数来处理，但是感觉这样也行
    timerInfo->tv_deadline.tv_usec = (tv.tv_usec + (timeout%1000)*1000 )%1000000;
    m_myHeap->push(timerInfo);

    struct epoll_event ev_write;
    ev_write.events =  EPOLLOUT | EPOLLET;
    ev_write.data.fd = FD;
    int ret1 = epoll_ctl(epollfd, EPOLL_CTL_ADD, FD, &ev_write);/* 每次添加写事件会使 epoll_weit 函数退出一次 */

    // cout<< "m_myHeap->size() = " << m_myHeap->size() <<endl;
    // cout<< ".tv_sec = "<< tv.tv_sec << "。 .tv_usec = " << tv.tv_usec <<endl;
    // cout<< "tv_deadline.tv_sec = "<< timerInfo->tv_deadline.tv_sec << "。 tv_deadline.tv_usec = " << timerInfo->tv_deadline.tv_usec <<endl;
    cout<<"ret1 = "<< ret1 << " ,addTimer: tv.tv_sec = "<< tv.tv_sec << "。 tv.tv_usec = " << tv.tv_usec <<endl;

    return timerInfo;

}

/* 删除定时器： */
void MyTimerManeger::delTimer(TimerInfo* timerInfo)
{
    timerInfo->isRuning = false;
}
