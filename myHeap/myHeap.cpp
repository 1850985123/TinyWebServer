#include "myHeap.h"

#include "sys/time.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
using namespace std;


MyHeap:: MyHeap(bool (*compare)(HEAP_DATA_TYPE a, HEAP_DATA_TYPE b), int heapArraySize )
{
	if(heapArraySize == 0)
	{
		perror("MyHeap::MyHeap error");
		exit(0);
		return;
	}

	m_heapArraySize = heapArraySize;
	m_heapArray = new HEAP_DATA_TYPE[m_heapArraySize];
	if(!m_heapArray)
	{
		perror("MyHeap::MyHeap error");
		exit(0);
		return;
	}
	memset(m_heapArray, 0, sizeof(HEAP_DATA_TYPE)*m_heapArraySize);
	m_heapSize = 0;;  
	this->compare  = compare;
};

MyHeap:: ~MyHeap()
{
	if(m_heapArray)
		delete[] m_heapArray; 
}

int MyHeap::size()
{
	return m_heapSize;
}
bool MyHeap::empty()
{
	return m_heapSize == 0;
}
HEAP_DATA_TYPE MyHeap::top()
{
	HEAP_DATA_TYPE temp = 0;
	m_locker.lock();
	if (!empty())
		 temp = m_heapArray[0];
	m_locker.unlock();
	return temp;
}

/* 调整当前index节点的父节点以满足堆结构 , 通过 compare 确定是大根堆还是小根堆*/
void MyHeap::heapUp( HEAP_DATA_TYPE* array, int index, bool(*compare)(HEAP_DATA_TYPE, HEAP_DATA_TYPE))
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
void MyHeap::heapDowm(HEAP_DATA_TYPE* array, int index, bool (*compare)(HEAP_DATA_TYPE,HEAP_DATA_TYPE))
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

void MyHeap::heapArrayReallocDeal()  //插入
{
	
	if(m_heapSize >= m_heapArraySize)
	{
		HEAP_DATA_TYPE * old_heapArray = m_heapArray;
		int old_heapArraySize = m_heapArraySize;

		m_heapArraySize = m_heapSize + 100;
		m_heapArray = new HEAP_DATA_TYPE[m_heapArraySize];
		if(!m_heapArray)
		{
			perror("MyHeap::heapArrayReallocDeal error");
			exit(0);
			return;
		}
			
		memset(m_heapArray, 0, sizeof(HEAP_DATA_TYPE)*m_heapArraySize);
		memcpy(m_heapArray, old_heapArray, sizeof(HEAP_DATA_TYPE) * old_heapArraySize);

		delete[] old_heapArray;
	}
}


void MyHeap::push(HEAP_DATA_TYPE  data)  //插入
{
	m_locker.lock();   
	HEAP_DATA_TYPE *array = m_heapArray;   
	array[m_heapSize] = data;
	m_heapSize++;
	heapArrayReallocDeal(); 
	heapUp(array, m_heapSize - 1, compare);
	m_locker.unlock();  
}

HEAP_DATA_TYPE MyHeap::pop()
{
	m_locker.lock();  
	if (empty())
	{
		m_locker.unlock(); 
		return 0;
	}
	HEAP_DATA_TYPE *array = m_heapArray;
	HEAP_DATA_TYPE result = array[0];
	swap(array[0], array[m_heapSize - 1]);
	m_heapSize--;
	heapDowm(array, 0, compare);
	m_locker.unlock(); 

	return result;
}
void MyHeap::travel_heap()
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
void MyHeap::modifyAtIndex(HEAP_DATA_TYPE data, unsigned int index)
{
	m_locker.lock();
	if (index > m_heapSize - 1) //超出堆范围
	{
		m_locker.unlock(); 
		return ;
	}
		
	HEAP_DATA_TYPE *array = m_heapArray;
	array[index] = data;
	heapUp(array, index, compare);
	heapDowm(array, index, compare);
	m_locker.unlock(); 
}

/* 删除某个节点的值，还是大根堆, 返回删除的值 */
HEAP_DATA_TYPE MyHeap::removeAtIndex(unsigned int index)
{
	m_locker.lock(); 
	if (index > m_heapSize - 1) //超出堆范围
	{
		m_locker.unlock(); 
		return 0;
	}
		
	HEAP_DATA_TYPE *array = m_heapArray;
	HEAP_DATA_TYPE result = array[index];
	swap(array[index], array[m_heapSize - 1]);
	m_heapSize--;
	m_locker.unlock(); 

	modifyAtIndex( array[index], index);

	return result;
}

void MyHeap::test_heap_struction()
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