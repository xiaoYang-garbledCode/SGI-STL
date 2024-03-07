#pragma once
/*
	移植SGI STL 二级空间配置器源码
	不同于nginx内存池，二级空间配置器中使用到的内存池需要考虑到多线程问题
	nginx内存池可用一个线程创建一个内存池
	而二级空间配置器是给容器使用的，如vector对象，而一个vector对象很多可能在多个线程中并发使用
*/
#include<iostream>
#include<mutex>
// 一级空间配置器将对象构造与内存开辟分开
template<typename T>
class first_level_myallocator 
{
public:
	// 分配内存
	T* allocate(size_t size)
	{
		T* m = malloc(size);
		return m;
	}
	// 回收内存
	void deallocate(T* p)
	{
		free(p);
		p = nullptr;
	}
	// 构造对象，定位new
	void construct(T* p, const T& val)
	{
		new (p) T(val);
	}

	// 定位new构造对象，右值引用
	void construct(T* p, const T&& rval)
	{
		new (p) T(std::move(rval));
	}
	// 析构
	void destroy(T* p)
	{
		p->~T();
	}
};

template <int __inst>   // 非类型模板参数
class __malloc_alloc_template    //一级空间配置器内存管理类
{
private:
	// 预置回调函数 _S_oom_malloc -> __malloc_alloc_oom_handler
	static void* _S_oom_malloc(size_t);
	static void (*__malloc_alloc_oom_handler)();
public:
	// 尝试分配内存
	static void* allocate(size_t __n)
	{
		//调用malloc
		void* __result = malloc(__n);
		if (__result == nullptr)
		{
			// malloc失败则调用_S_oom_malloc
			__result = _S_oom_malloc(__n);
		}
		return __result;
	}

	// 释放内存
	static void deallocate(void* __p, size_t __n)
	{
		free(__p);
	}

	//用户通过这个接口来预置自己的回调函数。 
	// 回调函数涉及释放指定的内容，从而解决因为内存不足而导致malloc失败
	// 
	//__set_malloc_handler 接受一个函数指针作为参数，这个函数指针f没有参数且返回值为0.
	/*
		void myFunction(){}

		void (*myFunctionPtr)() = & myFunction;
		(*myFunctionPtr)(); //调用function函数
	*/

	static void (*__set_malloc_handler(void(*__f)()))()
	{
		void(*__old)() = __malloc_alloc_oom_handler();
		__malloc_alloc_oom_handler = __f;
		return (__old);
	}
};

template<int __inst>
void* __malloc_alloc_template<__inst>::_S_oom_malloc(size_t __n)
{
	// 调用用户预置好的__malloc_alloc_oom_handler 回调函数
	// 如果有回调函数，则执行回调函数后，malloc，若仍然失败，继续重复
	// 如果没有回调函数 直接使用 throw bad_alloc

	void(*__my_malloc_handler)();
	void* __result;
	for (;;)
	{
		__my_malloc_handler = __malloc_alloc_oom_handler;
		if (__my_malloc_handler == nullptr)
		{
			throw std::bad_alloc();
		}
		(*__my_malloc_handler)();
		__result = malloc(__n);
		if (__result) return (__result);
	}
}

template<int __inst>
// 静态变量void(*__malloc_alloc_oom_handler)()类外初始化
void(*__malloc_alloc_template<__inst>::__malloc_alloc_oom_handler)() = nullptr;
using malloc_alloc = __malloc_alloc_template<0>;

template<typename T>
class myallocator
{
public:
	using value_type = T;

	constexpr myallocator() noexcept {}

	constexpr myallocator(const myallocator&) noexcept = default;
	template <class _Other>
	constexpr myallocator(const myallocator<_Other>&) noexcept {}
	// 开辟内存
	T* allocate(size_t __n);

	// 释放内存
	void deallocate(void* __p, size_t __n);

	// 重新分配内存，并把原先的内存归还给操作系统，且不再使用p的原指针地址
	//void* reallocate(void* __p, size_t __old_sz, size_t __new_sz);

	void construct(T* __p, const T& val)
	{
		new(__p) T(val);
	}
	void construct(T* __p, const T&& val)
	{
		new(__p) T(std::move(val));
	}
	void destroy(T* __p)
	{
		__p->~T();
	}

private:
	//使用枚举 enum Weekday{SUNDAY=1, MONDAY=2,TUESDAY=3,WEDESDAY=4,THURSDAY=5,FRIDAY=6,SATURDAY=7};
	// 
	// 自由链表从8bytes开始，以8bytes为对齐方式，一直扩充到128bytes
	enum { _ALIGN = 8};
	// 最大块的大小， 大于128的块不会放到内存池里即不会使用二级空间配置器而是使用一级空间配置器
	enum {_MAX_BYTES = 128};
	// 自由链表成员个数  _MAX_BYTES / _ALIGN
	enum {_NFREELISTS = 16};

	// 将byte上调至8的倍数
	static size_t _S_round_up(size_t __bytes)
	{
		return (__bytes + (size_t)_ALIGN - 1) & ~(((size_t)_ALIGN - 1));
	}

	// 计算出bytes大小的chunk应该挂载到自由链表free-list的哪个成员下
	static size_t _S_fresslist_index(size_t __bytes)
	{
		return ((__bytes + (size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
	}

	// 开辟内存池，并挂载到freelist成员下，返回请求的内存块
	static void* _S_refill(size_t __n);

	// 从还没形成链表的元素内存中取内存分配给自由链表成员，去形成链表。如果原始空闲内存不够了，则再开辟
	static char* _S_chunk_alloc(size_t __size, int& __nobjs);

	
	// union(联合体)里的所有成员共享同一块内存，
	// 联合体的大小等于其最大成员的大小，且对一个成员遍历的修改可能会影响其他成员的值
	// (还有内存对齐)     4				4					10
	// 比如说 有成员 int intValue;float floatValue; char stringValue[10]; 这个union大小为12与4对齐
	
	// 每个chunk块的信息, _M_free_list_link指向下一个chunk块
	union _Obj {
		union _Obj* _M_free_list_link;
		char _M_client_data[1];
	};

	// 基于free-list的内存池，需要考虑线程安全  volatile防止线程缓存的原因,无法及时获得变量的最新值
	static _Obj* volatile _S_free_list[_NFREELISTS];
	static std::mutex mtx;

	// static: 类内声明，类外定义
	static char* _S_start_free;  // 内存free的起始start位置
	static char* _S_end_free;   // 内存free的结束end位置
	static size_t _S_heap_size; // 总共malloc过的内存大小
};

// static: 类内声明，类外定义/初始化
template<typename T>
std::mutex myallocator<T>::mtx;
template<typename T>
char* myallocator<T>::_S_start_free = 0;
template<typename T>
char* myallocator<T>::_S_end_free = 0;
template<typename T>
size_t myallocator<T>::_S_heap_size = 0;
template<typename T>
// typename的作用是明确myallocator<T>::_Obj*是一个类型，而不是成员函数，静态成员变量。
typename myallocator<T>::_Obj* volatile myallocator<T>::_S_free_list[myallocator<T>::_NFREELISTS] 
	= { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 8
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }; //8


template<typename T>
T* myallocator<T>::allocate(size_t __n)
{
	__n = __n * sizeof(T);   // 因为vector容器传来的是元素个数
	void* __ret = 0;
	
	//如果需要开辟的的内存大于最大块，则使用malloc直接开辟，使用到一级空间配置器
	if (__n > (size_t)_MAX_BYTES)
	{
		return (T*)malloc(__n);
	}
	// 开辟内存小于最大块
	else
	{
		// 1.先定位这个__n将会位于哪个块中
		_Obj* volatile* _my_free_list = _S_free_list + _S_fresslist_index(__n);
	
		// _S_free_list 是所有线程共享的，加锁实现线程安全
		std::lock_guard<std::mutex> guard(mtx);
		_Obj* __result = *_my_free_list;

		if (__result == 0)
		{
			// 2.如果该块还没有创建，则调用_S_refill创建块
			__ret = _S_refill((_S_round_up(__n)));
		}
		else
		{
			// 2.如果该块的存在，则直接将第一块return，并指向下一个空闲块
			*_my_free_list = __result->_M_free_list_link;
			__ret = __result;
		}
	}
	// 3.返回与__n内存匹配的块
	return static_cast<T*>(__ret);
}

template<typename T>
// 开辟内存池，并挂载到freelist成员下，返回请求的内存块
void* myallocator<T>::_S_refill(size_t __n) // __n是一个chunk块的大小
{
	_Obj* volatile* __my_free_list;
	_Obj* __current_obj;
	_Obj* __next_obj;
	_Obj* __result;
	// 分配指定大小的内存池 20个 __obj表示块的数量  __表示每个chunk的大小
	int __objs = 20; 
	//1. 调用_S_chunk_alloc,创建20个大小为__n的chunk块  !其中__objs传入的是引用，返回的时候表示的是开辟的块数
	char* __chunk = _S_chunk_alloc(__n, __objs);

	__result = (_Obj*)__chunk; //将result作为结果返回

	//2. 如果只申请到一块chunk块，直接返回给上一级，无需链接各个chunk关系(挂载到相应的freelist成员下)
	if (__objs == 1) return __chunk;
	//3. 不只一块，根据chunk块大小算出这个chunk应该属于freelist的第几个成员管理
	__my_free_list = _S_free_list + _S_fresslist_index(__n);
	
	//4. 将创建的块通过每个chunk里的指针连接起来
	//调用refill表示当前这个位置的freelist是空的，需要创建，所以freelist需要先移动__n分配出去的大小
	*__my_free_list = __next_obj= (_Obj*)(__chunk + __n);
	for (int __i = 1; ; __i++)
	{
		__current_obj = __next_obj; // __next_obj负责移动 __n大小，形成每个chunk块，形成一块就连接一块
		__next_obj = (_Obj*)((char*)__next_obj + __n);
		
		if (__i == __objs - 1)  // 最后一块的next域置为nullptr
		{
			__current_obj->_M_free_list_link = nullptr;
			break;
		}
		else {
			__current_obj->_M_free_list_link = __next_obj; // 把这一块连起来
		}
	}
	//5.返回第一块
	return __result; //返回第一个内存块
}

template<typename T>
// 从还没形成链表的元素内存(备用内存)中取内存分配给自由链表成员，去形成链表。如果原始空闲内存不够了，则再开辟
char* myallocator<T>::_S_chunk_alloc(size_t __size, int& __nobjs)
{
	// refill创建20个块的时候调用的就是这个方法
			//__total_bytes						__left_bytes						
	//1.需要计算本次请求的内存大小，以及从开始到现在请求的剩余空闲的内存大小(不包括回收的，只算开辟的)。
	char* __result;
	size_t __total_bytes = __size * __nobjs;
	size_t __left_bytes = _S_end_free - _S_start_free;

	//2.剩余的备用内存够支付本次请求的内存大小。 直接return返回分配的内存
	if (__left_bytes > __total_bytes)
	{
		__result = _S_start_free;
		_S_start_free += __total_bytes;
		return __result;
	}

	//3.剩余的内存不够支付total，但起码能够支付起一个内存块。(因为要返回的至少是一个内存块)
	// 修改__objs的值，告诉用户我现在给你分配了几个
	else if (__left_bytes >= __size)
	{
		__nobjs = (int)( __left_bytes / __size);
		__result = _S_start_free;
		_S_start_free += __nobjs * __size;
		return __result;
	}
	//4.当剩余的空间连一个内存块都不够支付了，需要把这块内存挂载到他能所属(需要定位)的freelist成员下(使用头插法)
	// 并需要向系统malloc内存，malloc的内存至少是本次请求的内存大小的两倍
	else
	{
		size_t __bytes_to_get = 2 * __total_bytes + _S_round_up(_S_heap_size >>4);
		if (__left_bytes > 0)
		{
			_Obj* volatile* __my_free_list;
			__my_free_list = _S_free_list + _S_fresslist_index(__left_bytes); //定位所属的freelist成员
			// 头插法插入
			((_Obj*)(_S_start_free))->_M_free_list_link = *__my_free_list;
			*__my_free_list = (_Obj*)(_S_start_free);
		}
		// 向系统malloc,应该用_S_start_free来接收，因为这是在开辟备用内存。
		_S_start_free = (char*)malloc(__bytes_to_get);

		//5.向系统申请内存失败,从当前freelist开始向后搜索，判断这之后的成员是否有空闲块
			// 至少借用size大小的chunk块
		if (_S_start_free == nullptr)
		{							//	用__my_free_list + _S_freelist_index(__i) 来搜索
			for (size_t __i = __size; __i<_MAX_BYTES; __i = __i + _ALIGN)
			{
				_Obj* __p;
				_Obj* volatile* __my_free_list = _S_free_list + _S_fresslist_index(__i);
				__p = *__my_free_list;
				if (__p != nullptr) // 遍历到的这个freelist成员是有空闲块的
				{
					// 从这个freelist成员借一块
					_S_start_free = (char*)__p;
					_S_end_free = _S_start_free + __i;

					// 让__my_free_list成员指向下一个空闲块
					*__my_free_list = __p->_M_free_list_link;
					
					//递归调用_S_chunk_alloc，重新进入这个函数去判断此时分配的left_bytes与size的关系
					// 然后决定如何在left_bytes上分配，(1 - 20 块)，最终返回result
					return (_S_chunk_alloc(__size, __nobjs));
				}
			}

			// 其他成员都没空闲块，进入malloc_alloc::allocate(__bytes_to_get);
			_S_end_free = nullptr;
			_S_start_free = (char*)malloc_alloc::allocate(__bytes_to_get);
		}

		//修改相关变量的值
		// (两种情况可以走到这里，1.向系统malloc成功，2.大于_size的freelist成员中没有空闲内存，调用malloc_alloc::allocate)
		_S_heap_size += __bytes_to_get;
		_S_end_free = _S_start_free + __bytes_to_get;
		return (_S_chunk_alloc(__size, __nobjs)); // 递归调用
	}
}

template<typename T>
// 释放p指针指向的大小为__n的块的内存
void myallocator<T>::deallocate(void*__p, size_t __n)
{
	if (__n > (size_t)_MAX_BYTES) // n>128 和一级空间配置器一样
	{
		malloc_alloc::deallocate(__p, __n); // free(__p)
	}
	else
	{
		// 找到相应的freelist成员
		_Obj* volatile* _my_free_list = _S_free_list + _S_fresslist_index(__n);
		_Obj* __q = (_Obj*)__p;
		// 将这块内存头插法插入,对_S_free_list进行操作需要考虑线程安全问题p
		std::unique_lock<std::mutex> lck(mtx);
		__q->_M_free_list_link = *_my_free_list;
		*_my_free_list = __q;
	}
}