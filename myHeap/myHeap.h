#ifndef _myHeap_h
#define _myHeap_h


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
#include "../lock/locker.h"


struct TimerInfo{
    struct timeval tv_deadline; //定时截止时间
    int timoout ;   //超时时间以ms为单位
    int timer_indexInHeap;
    bool isRuning; //True 表示正在运行， false表示被删除了。
    void *(* timeoutCallBack)(void *arg);
    void *arg; //timeoutCallBack 函数的传入参数
};


#define  HEAP_DATA_TYPE  TimerInfo*


// #define HEAP_ARRAY_MAXSIZE  100
// #define COMPARE_MOTHOD  compare_less 
// // #define COMPARE_MOTHOD  compare_greater
// bool compare_greater(HEAP_DATA_TYPE a, HEAP_DATA_TYPE b);
// bool compare_less(HEAP_DATA_TYPE a, HEAP_DATA_TYPE b);

class MyHeap
{ 
public:
    // MyTimerApp();
    // 参数1： 用作比较器，必须传入，通过比较确定是大根堆还是小根堆，
    MyHeap(bool (*compare)(HEAP_DATA_TYPE a, HEAP_DATA_TYPE b), int heapArraySize = 10);
    ~MyHeap();
public:
    void push(HEAP_DATA_TYPE data);  
    HEAP_DATA_TYPE top();
    HEAP_DATA_TYPE pop();
    bool empty();
    int size();         //当前堆的成员个数
    void travel_heap(); //遍历堆里的所有成员

    void test_heap_struction();

public:  //比较底层的函数一般不会用到
    void modifyAtIndex(HEAP_DATA_TYPE data, unsigned int index);
    HEAP_DATA_TYPE removeAtIndex(unsigned int index);
private:
    void heapArrayReallocDeal(); //如果堆空间不够了自动重新分配内存
    void heapUp( HEAP_DATA_TYPE* array, int index, bool(*compare)(HEAP_DATA_TYPE, HEAP_DATA_TYPE));
    void heapDowm(HEAP_DATA_TYPE* array, int index, bool (*compare)(HEAP_DATA_TYPE,HEAP_DATA_TYPE));
private:
    HEAP_DATA_TYPE *m_heapArray; //维持堆成员的数组
    int m_heapArraySize;

	int m_heapSize;         //堆里现有的成员数量 
    bool (*compare)(HEAP_DATA_TYPE , HEAP_DATA_TYPE );// 数据大小比较的函数。

    locker m_locker;       //互斥锁
};

#endif