#ifndef _myTimerManeger_h
#define _myTimerManeger_h


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include "../myHeap/myHeap.h"



class MyTimerManeger
{ 
public:
    //C++11以后,使用局部变量懒汉不用加锁
    static MyTimerManeger* getInstance(){
        static MyTimerManeger myTimerManeger;
        return &myTimerManeger;
    };

    static bool compare_less(HEAP_DATA_TYPE a, HEAP_DATA_TYPE b); //m_myHeap ，里的比较方法，这里是小根堆。
private:
    // MyTimerApp();
    MyTimerManeger();
    ~MyTimerManeger();
private:
     /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();
public:
    /* 添加定时器： 超时时间，回调函数 */
    TimerInfo * addTimer(int timeout, void *(* timeoutCallBack)(void *arg), void *arg);//超时时间以ms为单位

    /* 删除定时器： */
    void delTimer(TimerInfo *timerInfo);
private:
    /* 堆里的数据结构类型需要： 定时器的截止时间，超时时间，回调函数，定时器在堆里的索引（用来删除 O（1）） */
    MyHeap *m_myHeap;
    int epollfd;
    struct epoll_event  events[1];
};

#endif