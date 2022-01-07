#pragma once
#include <mutex>
#include "BaseControl.h"





// 该联合体存在的意义只是为了在操作内存时方便赋值
union CObj
{
	CObj* MdNext;
	char* MdData;
};

template <class T>
class CMemoryPool
{
private:
	int				MdObjectLen;			// 一个T类型对象的长度
	int				MdNumbers;				// 能分配多少个对象
	char*			MdPPool;				// 整个内存池所占用的空间地址起点
	CObj*			MdObjectLinkHead;		// 内存池被转化为链表后的链表头
	std::mutex		MdMtx;					// 使用该内存池时的锁
	int				MdLackCount;			// 内存池使用不足时计数
public:
	CMemoryPool(int numbers);	
	~CMemoryPool();				
	CMemoryPool(const CMemoryPool&) = delete;
	CMemoryPool& operator=(const CMemoryPool&) = delete;
	T*		MfApplyMemory();			// 申请一块内存
	void	MfReturnMemory(T* obj);		// 归还一块内存
};

template <class T>
CMemoryPool<T>::CMemoryPool(int numbers):MdObjectLen(0), MdNumbers(numbers), MdPPool(nullptr), MdLackCount(0)
{
	if (0 >= MdNumbers)								// 数量 没有被声明为正确的值话，就退出
	{
		printf("CObjectPool init fail!\tmsg: MDObjectNumber uninitialized\n11");
		ErrorWaitExit();
	}
	MdObjectLen = sizeof(T);				// 确定一个对象的长度
	if (8 > MdObjectLen)					// 兼容64位
	{
		printf("WARRING!\tmsg: MDObjectLen < 8，already set MDObjectLen is 8\n11");
		MdObjectLen = 8;
	}
	MdPPool = new char[(MdObjectLen + 1) * MdNumbers]{ 0 };					// 申请对象池的空间		对象长度*数量
	// 这里要解释一下为了多申请1字节的空间
	// 在调用掉该对象从对象池中返回对象池时，可能会有内存池中没有内存可分配的情况
	// 此时的行为应该是用new来申请空间，然后返回内存
	// 为了区分返回的内存是由内存池还是new分配，在分配的内存前加1字节的标志
	// 因为在对象池在申请时所有字节都已经设0，所以这1字节的标志为0就代表是内存池分配
	// new时，主动把该标志设为1，这样归还对象时，根据该标志来执行不同的操作

	// 接下来把申请的对象池初始化成一个链表
	MdObjectLinkHead = (CObj*)(MdPPool + 1);
	CObj* temp = MdObjectLinkHead;
	for (int i = 0; i < MdNumbers; ++i)
	{
		temp->MdNext = (CObj*)((char*)MdObjectLinkHead + (i * (MdObjectLen + 1)));
		temp = (CObj*)((char*)MdObjectLinkHead + (i * (MdObjectLen + 1)));
	}
	((CObj*)(MdPPool + (MdNumbers - 1) * (MdObjectLen + 1)))->MdNext = nullptr;
}

template <class T>
CMemoryPool<T>::~CMemoryPool()
{
	if (!MdPPool)
		delete MdPPool;					// 释放对象池的空间
	MdPPool = nullptr;
}

template <class T>
T* CMemoryPool<T>::MfApplyMemory()
{
	char* ret = nullptr;
	std::unique_lock<std::mutex> lk(MdMtx);
	if (nullptr != MdObjectLinkHead)
	{
		ret = (char*)MdObjectLinkHead;
		MdObjectLinkHead = MdObjectLinkHead->MdNext;
		return (T*)ret;
	}
	else
	{
		lk.unlock();
		++MdLackCount;
		ret = new char[MdObjectLen + 1];
		ret[0] = 1;
		return (T*)(ret + 1);
	}
	return nullptr;
}

template <class T>
void CMemoryPool<T>::MfReturnMemory(T* obj)
{
	if (nullptr == obj)
		return;
	std::unique_lock<std::mutex> lk(MdMtx);
	if (0 == *((char*)obj - 1))
	{
		if (nullptr == MdObjectLinkHead)
		{
			((CObj*)obj)->MdNext = nullptr;
			MdObjectLinkHead = (CObj*)obj;
		}
		else
		{
			((CObj*)obj)->MdNext = MdObjectLinkHead->MdNext;
			MdObjectLinkHead = (CObj*)obj;
		}
		return;
	}
	else
	{
		lk.unlock();
		delete[]((char*)obj - 1);
	}
}

template <class T>
class CObjectPool
{
private:
	CMemoryPool<T>	MdMemoryPool;
public:
	CObjectPool(int numbers):MdMemoryPool(numbers){};
	~CObjectPool() {};
	CObjectPool(const CObjectPool&) = delete;
	CObjectPool& operator=(const CObjectPool&) = delete;
	template <typename ...Arg>
	T*		MfApplyObject(Arg... a);	// 申请一快内存并构造对象
	void	MfReturnObject(T* obj);		// 析构对象并归还内存
};

template <class T>
template <typename ...Arg>
T* CObjectPool<T>::MfApplyObject(Arg... arg)
{
	T* tmp = MdMemoryPool.MfApplyMemory();
	return new(tmp)T(arg...);
}

template <class T>
void CObjectPool<T>::MfReturnObject(T* obj)
{
	obj->~T();
	MdMemoryPool.MfReturnMemory(obj);
}


// 这两个函数用于DLL的导出，因为模板类不能直接导出，所以用函数包一层
template<class T>
CMemoryPool<T>* GetCMemoryPool()
{
	CMemoryPool<T>* instance = new CMemoryPool<T>();
	return instance;
}
template<class T>
CObjectPool<T>* GetCObjectPool()
{
	CObjectPool<T>* instance = new CObjectPool<T>();
	return instance;
}