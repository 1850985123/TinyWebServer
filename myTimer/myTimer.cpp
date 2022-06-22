#include "myTimer.h"

#include "sys/time.h"
#include <iostream>
using namespace std;



MyTimer::MyTimer(int timeout, void *(* timeoutCallBack)(void *arg), void *arg)
{
    myTimerManeger = MyTimerManeger::getInstance();
    this->timeout = timeout;
    this->timeoutCallBack = timeoutCallBack;
    this->arg = arg;
    m_timerInfo = NULL;
}
MyTimer::~MyTimer()
{
    cout<< "进入~MyTimer, this == "<<  this<<endl;
    stop();
}

/* 删除定时器： */
void MyTimer::delTimer(TimerInfo *timerInfo)
{
     myTimerManeger->delTimer(m_timerInfo);
}

/* 添加定时器： 超时时间，回调函数 */
TimerInfo * MyTimer::addTimer(int timeout, void *(* timeoutCallBack)(void *arg), void *arg)//超时时间以ms为单位
{
    m_timerInfo = myTimerManeger->addTimer(timeout,timeoutCallBack, arg);
}

MyTimer& MyTimer::start()
{
    if(!isRuning()) //不能重复启动，必须关闭后才能启动
        addTimer(timeout,timeoutCallBack, arg);
    return *this;
}
MyTimer& MyTimer::stop()
{
    if(isRuning())
    {
        delTimer(m_timerInfo);
        m_timerInfo = NULL;
    }    
    return *this;
}
bool MyTimer::isRuning()
{
    return m_timerInfo ;
    
}

/* 设置超时时间后，（如果定时器已经启动）重新启动定时器，并用新的超时时间 */
MyTimer& MyTimer::setTimeout(int timeout)
{
    this->timeout = timeout;
    if(isRuning())
    {
        this->stop();
        this->start();
    }
}