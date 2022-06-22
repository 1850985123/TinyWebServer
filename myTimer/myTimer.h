#ifndef _myTimer_h
#define _myTimer_h


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
#include "myTimerManeger.h"

class MyTimer
{ 
public:
    MyTimer(int timeout, void *(* timeoutCallBack)(void *arg), void *arg);
    ~MyTimer();
  
private:
    /* 添加定时器： 超时时间，回调函数 */
    TimerInfo * addTimer(int timeout, void *(* timeoutCallBack)(void *arg), void *arg);//超时时间以ms为单位

    /* 删除定时器： */
    void delTimer(TimerInfo *timerInfo);
public:
    MyTimer& start();
    MyTimer& stop();
    bool isRuning();
    MyTimer& setTimeout(int timeout);
private:
    /* 堆里的数据结构类型需要： 定时器的截止时间，超时时间，回调函数，定时器在堆里的索引（用来删除 O（1）） */
    TimerInfo* m_timerInfo;
    MyTimerManeger * myTimerManeger;

    int timeout;
    void *(* timeoutCallBack)(void *arg);
    void *arg;
};



#endif