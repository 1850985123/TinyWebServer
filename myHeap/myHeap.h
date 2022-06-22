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

#include "myHeap.h"

#include "sys/time.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
using namespace std;


template<class T>
class MyHeap
{ 
public:
    // MyTimerApp();
    // 参数1： 用作比较器，必须传入，通过比较确定是大根堆还是小根堆，
    MyHeap(bool (*compare)(T a, T b), int heapArraySize = 10);
    ~MyHeap();
public:
    void push(T data);  
    T top();
    T pop();
    bool empty();
    int size();         //当前堆的成员个数
    void travel_heap(); //遍历堆里的所有成员

    void test_heap_struction();

public:  //比较底层的函数一般不会用到
    void modifyAtIndex(T data, unsigned int index);
    T removeAtIndex(unsigned int index);
private:
    void heapArrayReallocDeal(); //如果堆空间不够了自动重新分配内存
    void heapUp( T* array, int index, bool(*compare)(T, T));
    void heapDowm(T* array, int index, bool (*compare)(T,T));
private:
    T *m_heapArray; //维持堆成员的数组
    int m_heapArraySize;

	int m_heapSize;         //堆里现有的成员数量 
    bool (*compare)(T , T );// 数据大小比较的函数。

    locker m_locker;       //互斥锁
};




template<class T>
MyHeap<T>:: MyHeap(bool (*compare)(T a, T b), int heapArraySize )
{
	if(heapArraySize == 0)
	{
		perror("MyHeap::MyHeap error");
		exit(0);
		return;
	}

	m_heapArraySize = heapArraySize;
	m_heapArray = new T[m_heapArraySize];
	if(!m_heapArray)
	{
		perror("MyHeap::MyHeap error");
		exit(0);
		return;
	}
	memset(m_heapArray, 0, sizeof(T)*m_heapArraySize);
	m_heapSize = 0;;  
	this->compare  = compare;
};

template<class T>
MyHeap<T>:: ~MyHeap()
{
	if(m_heapArray)
		delete[] m_heapArray; 
}

template<class T>
int MyHeap<T>::size()
{
	return m_heapSize;
}

template<class T>
bool MyHeap<T>::empty()
{
	return m_heapSize == 0;
}

template<class T>
T MyHeap<T>::top()
{
	T temp = 0;
	m_locker.lock();
	if (!empty())
		 temp = m_heapArray[0];
	m_locker.unlock();
	return temp;
}

/* 调整当前index节点的父节点以满足堆结构 , 通过 compare 确定是大根堆还是小根堆*/
template<class T>
void MyHeap<T>::heapUp( T* array, int index, bool(*compare)(T, T))
{
	if (index >= m_heapSize)
		return;
	while (compare(array[index] , array[(index - 1) / 2]))
	{
		swap(array[(index - 1) / 2], array[index]);
		index = (index - 1) / 2;
	}
}

/* 调整当前index节点的子节点以满足堆结构, 通过 compare 确定是大根堆还是小根堆 */
template<class T>
void MyHeap<T>::heapDowm(T* array, int index, bool (*compare)(T,T))
{
	if (index >= m_heapSize)
		return;
	int larger = 0;

	while (index * 2 + 1 < m_heapSize)  //有左节点
	{
		larger = index * 2 + 1;
		if (index * 2 + 2 < m_heapSize)  //有右节点
		{
			larger = compare(array[index * 2 + 2] , array[index * 2 + 1]) ? index * 2 + 2 : index * 2 + 1;
		}
		if (compare(array[index] , array[larger])) // 有子树大于根
			break;
		swap(array[index], array[larger]);
		index = larger;
	}
}

template<class T>
void MyHeap<T>::heapArrayReallocDeal()  //插入
{
	
	if(m_heapSize >= m_heapArraySize)
	{
		T * old_heapArray = m_heapArray;
		int old_heapArraySize = m_heapArraySize;

		m_heapArraySize = m_heapSize + 100;
		m_heapArray = new T[m_heapArraySize];
		if(!m_heapArray)
		{
			perror("MyHeap::heapArrayReallocDeal error");
			exit(0);
			return;
		}
			
		memset(m_heapArray, 0, sizeof(T)*m_heapArraySize);
		memcpy(m_heapArray, old_heapArray, sizeof(T) * old_heapArraySize);

		delete[] old_heapArray;
	}
}

template<class T>
void MyHeap<T>::push(T  data)  //插入
{
	m_locker.lock();   
	T *array = m_heapArray;   
	array[m_heapSize] = data;
	m_heapSize++;
	heapArrayReallocDeal(); 
	heapUp(array, m_heapSize - 1, compare);
	m_locker.unlock();  
}

template<class T>
T MyHeap<T>::pop()
{
	m_locker.lock();  
	if (empty())
	{
		m_locker.unlock(); 
		return 0;
	}
	T *array = m_heapArray;
	T result = array[0];
	swap(array[0], array[m_heapSize - 1]);
	m_heapSize--;
	heapDowm(array, 0, compare);
	m_locker.unlock(); 

	return result;
}

template<class T>
void MyHeap<T>::travel_heap()
{
	cout<<"heap 遍历开始" << endl;

	m_locker.lock();
	for (int i = 0; i < m_heapSize; i++)
	{
		cout << m_heapArray[i] << "\t";
		if ((i > 0) && (i % 10 == 0))
		{
			cout << endl;
		}
	}
	m_locker.unlock();

	cout << endl;
}

/* 修改某个节点的值，还是大根堆 */
template<class T>
void MyHeap<T>::modifyAtIndex(T data, unsigned int index)
{
	m_locker.lock();
	if (index > m_heapSize - 1) //超出堆范围
	{
		m_locker.unlock(); 
		return ;
	}
		
	T *array = m_heapArray;
	array[index] = data;
	heapUp(array, index, compare);
	heapDowm(array, index, compare);
	m_locker.unlock(); 
}

/* 删除某个节点的值，还是大根堆, 返回删除的值 */
template<class T>
T MyHeap<T>::removeAtIndex(unsigned int index)
{
	m_locker.lock(); 
	if (index > m_heapSize - 1) //超出堆范围
	{
		m_locker.unlock(); 
		return 0;
	}
		
	T *array = m_heapArray;
	T result = array[index];
	swap(array[index], array[m_heapSize - 1]);
	m_heapSize--;
	m_locker.unlock(); 

	modifyAtIndex( array[index], index);

	return result;
}

template<class T>
void MyHeap<T>::test_heap_struction()
{
	#if 0
	for (int i = 0; i < HEAP_ARRAY_MAXSIZE; i++)
	{
		int temp = rand() % 100;
		push(temp);

		cout << temp << "\t ";
		if (i % 10 == 0 && i != 0)
			cout << endl;
	}

	cout << "heap size = " << m_heapSize << endl;
	travel_heap();
	//modifyAtIndex(100, 3);
	removeAtIndex( 3);
	travel_heap();

	cout << "heap 依次输出最大值" << endl;
	int i = 0;
	while (!empty())
	{
		cout << pop() << "\t ";
		if(++i%10 == 0)
			cout << endl;
	}
	#endif

}

#endif