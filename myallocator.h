#pragma once
/*
	��ֲSGI STL �����ռ�������Դ��
	��ͬ��nginx�ڴ�أ������ռ���������ʹ�õ����ڴ����Ҫ���ǵ����߳�����
	nginx�ڴ�ؿ���һ���̴߳���һ���ڴ��
	�������ռ��������Ǹ�����ʹ�õģ���vector���󣬶�һ��vector����ܶ�����ڶ���߳��в���ʹ��
*/
#include<iostream>
#include<mutex>
// һ���ռ������������������ڴ濪�ٷֿ�
template<typename T>
class first_level_myallocator 
{
public:
	// �����ڴ�
	T* allocate(size_t size)
	{
		T* m = malloc(size);
		return m;
	}
	// �����ڴ�
	void deallocate(T* p)
	{
		free(p);
		p = nullptr;
	}
	// ������󣬶�λnew
	void construct(T* p, const T& val)
	{
		new (p) T(val);
	}

	// ��λnew���������ֵ����
	void construct(T* p, const T&& rval)
	{
		new (p) T(std::move(rval));
	}
	// ����
	void destroy(T* p)
	{
		p->~T();
	}
};

template <int __inst>   // ������ģ�����
class __malloc_alloc_template    //һ���ռ��������ڴ������
{
private:
	// Ԥ�ûص����� _S_oom_malloc -> __malloc_alloc_oom_handler
	static void* _S_oom_malloc(size_t);
	static void (*__malloc_alloc_oom_handler)();
public:
	// ���Է����ڴ�
	static void* allocate(size_t __n)
	{
		//����malloc
		void* __result = malloc(__n);
		if (__result == nullptr)
		{
			// mallocʧ�������_S_oom_malloc
			__result = _S_oom_malloc(__n);
		}
		return __result;
	}

	// �ͷ��ڴ�
	static void deallocate(void* __p, size_t __n)
	{
		free(__p);
	}

	//�û�ͨ������ӿ���Ԥ���Լ��Ļص������� 
	// �ص������漰�ͷ�ָ�������ݣ��Ӷ������Ϊ�ڴ治�������mallocʧ��
	// 
	//__set_malloc_handler ����һ������ָ����Ϊ�������������ָ��fû�в����ҷ���ֵΪ0.
	/*
		void myFunction(){}

		void (*myFunctionPtr)() = & myFunction;
		(*myFunctionPtr)(); //����function����
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
	// �����û�Ԥ�úõ�__malloc_alloc_oom_handler �ص�����
	// ����лص���������ִ�лص�������malloc������Ȼʧ�ܣ������ظ�
	// ���û�лص����� ֱ��ʹ�� throw bad_alloc

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
// ��̬����void(*__malloc_alloc_oom_handler)()�����ʼ��
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
	// �����ڴ�
	T* allocate(size_t __n);

	// �ͷ��ڴ�
	void deallocate(void* __p, size_t __n);

	// ���·����ڴ棬����ԭ�ȵ��ڴ�黹������ϵͳ���Ҳ���ʹ��p��ԭָ���ַ
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
	//ʹ��ö�� enum Weekday{SUNDAY=1, MONDAY=2,TUESDAY=3,WEDESDAY=4,THURSDAY=5,FRIDAY=6,SATURDAY=7};
	// 
	// ���������8bytes��ʼ����8bytesΪ���뷽ʽ��һֱ���䵽128bytes
	enum { _ALIGN = 8};
	// ����Ĵ�С�� ����128�Ŀ鲻��ŵ��ڴ���Ｔ����ʹ�ö����ռ�����������ʹ��һ���ռ�������
	enum {_MAX_BYTES = 128};
	// ���������Ա����  _MAX_BYTES / _ALIGN
	enum {_NFREELISTS = 16};

	// ��byte�ϵ���8�ı���
	static size_t _S_round_up(size_t __bytes)
	{
		return (__bytes + (size_t)_ALIGN - 1) & ~(((size_t)_ALIGN - 1));
	}

	// �����bytes��С��chunkӦ�ù��ص���������free-list���ĸ���Ա��
	static size_t _S_fresslist_index(size_t __bytes)
	{
		return ((__bytes + (size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
	}

	// �����ڴ�أ������ص�freelist��Ա�£�����������ڴ��
	static void* _S_refill(size_t __n);

	// �ӻ�û�γ������Ԫ���ڴ���ȡ�ڴ��������������Ա��ȥ�γ��������ԭʼ�����ڴ治���ˣ����ٿ���
	static char* _S_chunk_alloc(size_t __size, int& __nobjs);

	
	// union(������)������г�Ա����ͬһ���ڴ棬
	// ������Ĵ�С����������Ա�Ĵ�С���Ҷ�һ����Ա�������޸Ŀ��ܻ�Ӱ��������Ա��ֵ
	// (�����ڴ����)     4				4					10
	// ����˵ �г�Ա int intValue;float floatValue; char stringValue[10]; ���union��СΪ12��4����
	
	// ÿ��chunk�����Ϣ, _M_free_list_linkָ����һ��chunk��
	union _Obj {
		union _Obj* _M_free_list_link;
		char _M_client_data[1];
	};

	// ����free-list���ڴ�أ���Ҫ�����̰߳�ȫ  volatile��ֹ�̻߳����ԭ��,�޷���ʱ��ñ���������ֵ
	static _Obj* volatile _S_free_list[_NFREELISTS];
	static std::mutex mtx;

	// static: �������������ⶨ��
	static char* _S_start_free;  // �ڴ�free����ʼstartλ��
	static char* _S_end_free;   // �ڴ�free�Ľ���endλ��
	static size_t _S_heap_size; // �ܹ�malloc�����ڴ��С
};

// static: �������������ⶨ��/��ʼ��
template<typename T>
std::mutex myallocator<T>::mtx;
template<typename T>
char* myallocator<T>::_S_start_free = 0;
template<typename T>
char* myallocator<T>::_S_end_free = 0;
template<typename T>
size_t myallocator<T>::_S_heap_size = 0;
template<typename T>
// typename����������ȷmyallocator<T>::_Obj*��һ�����ͣ������ǳ�Ա��������̬��Ա������
typename myallocator<T>::_Obj* volatile myallocator<T>::_S_free_list[myallocator<T>::_NFREELISTS] 
	= { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,  // 8
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }; //8


template<typename T>
T* myallocator<T>::allocate(size_t __n)
{
	__n = __n * sizeof(T);   // ��Ϊvector������������Ԫ�ظ���
	void* __ret = 0;
	
	//�����Ҫ���ٵĵ��ڴ�������飬��ʹ��mallocֱ�ӿ��٣�ʹ�õ�һ���ռ�������
	if (__n > (size_t)_MAX_BYTES)
	{
		return (T*)malloc(__n);
	}
	// �����ڴ�С������
	else
	{
		// 1.�ȶ�λ���__n����λ���ĸ�����
		_Obj* volatile* _my_free_list = _S_free_list + _S_fresslist_index(__n);
	
		// _S_free_list �������̹߳���ģ�����ʵ���̰߳�ȫ
		std::lock_guard<std::mutex> guard(mtx);
		_Obj* __result = *_my_free_list;

		if (__result == 0)
		{
			// 2.����ÿ黹û�д����������_S_refill������
			__ret = _S_refill((_S_round_up(__n)));
		}
		else
		{
			// 2.����ÿ�Ĵ��ڣ���ֱ�ӽ���һ��return����ָ����һ�����п�
			*_my_free_list = __result->_M_free_list_link;
			__ret = __result;
		}
	}
	// 3.������__n�ڴ�ƥ��Ŀ�
	return static_cast<T*>(__ret);
}

template<typename T>
// �����ڴ�أ������ص�freelist��Ա�£�����������ڴ��
void* myallocator<T>::_S_refill(size_t __n) // __n��һ��chunk��Ĵ�С
{
	_Obj* volatile* __my_free_list;
	_Obj* __current_obj;
	_Obj* __next_obj;
	_Obj* __result;
	// ����ָ����С���ڴ�� 20�� __obj��ʾ�������  __��ʾÿ��chunk�Ĵ�С
	int __objs = 20; 
	//1. ����_S_chunk_alloc,����20����СΪ__n��chunk��  !����__objs����������ã����ص�ʱ���ʾ���ǿ��ٵĿ���
	char* __chunk = _S_chunk_alloc(__n, __objs);

	__result = (_Obj*)__chunk; //��result��Ϊ�������

	//2. ���ֻ���뵽һ��chunk�飬ֱ�ӷ��ظ���һ�����������Ӹ���chunk��ϵ(���ص���Ӧ��freelist��Ա��)
	if (__objs == 1) return __chunk;
	//3. ��ֻһ�飬����chunk���С������chunkӦ������freelist�ĵڼ�����Ա����
	__my_free_list = _S_free_list + _S_fresslist_index(__n);
	
	//4. �������Ŀ�ͨ��ÿ��chunk���ָ����������
	//����refill��ʾ��ǰ���λ�õ�freelist�ǿյģ���Ҫ����������freelist��Ҫ���ƶ�__n�����ȥ�Ĵ�С
	*__my_free_list = __next_obj= (_Obj*)(__chunk + __n);
	for (int __i = 1; ; __i++)
	{
		__current_obj = __next_obj; // __next_obj�����ƶ� __n��С���γ�ÿ��chunk�飬�γ�һ�������һ��
		__next_obj = (_Obj*)((char*)__next_obj + __n);
		
		if (__i == __objs - 1)  // ���һ���next����Ϊnullptr
		{
			__current_obj->_M_free_list_link = nullptr;
			break;
		}
		else {
			__current_obj->_M_free_list_link = __next_obj; // ����һ��������
		}
	}
	//5.���ص�һ��
	return __result; //���ص�һ���ڴ��
}

template<typename T>
// �ӻ�û�γ������Ԫ���ڴ�(�����ڴ�)��ȡ�ڴ��������������Ա��ȥ�γ��������ԭʼ�����ڴ治���ˣ����ٿ���
char* myallocator<T>::_S_chunk_alloc(size_t __size, int& __nobjs)
{
	// refill����20�����ʱ����õľ����������
			//__total_bytes						__left_bytes						
	//1.��Ҫ���㱾��������ڴ��С���Լ��ӿ�ʼ�����������ʣ����е��ڴ��С(���������յģ�ֻ�㿪�ٵ�)��
	char* __result;
	size_t __total_bytes = __size * __nobjs;
	size_t __left_bytes = _S_end_free - _S_start_free;

	//2.ʣ��ı����ڴ湻֧������������ڴ��С�� ֱ��return���ط�����ڴ�
	if (__left_bytes > __total_bytes)
	{
		__result = _S_start_free;
		_S_start_free += __total_bytes;
		return __result;
	}

	//3.ʣ����ڴ治��֧��total���������ܹ�֧����һ���ڴ�顣(��ΪҪ���ص�������һ���ڴ��)
	// �޸�__objs��ֵ�������û������ڸ�������˼���
	else if (__left_bytes >= __size)
	{
		__nobjs = (int)( __left_bytes / __size);
		__result = _S_start_free;
		_S_start_free += __nobjs * __size;
		return __result;
	}
	//4.��ʣ��Ŀռ���һ���ڴ�鶼����֧���ˣ���Ҫ������ڴ���ص���������(��Ҫ��λ)��freelist��Ա��(ʹ��ͷ�巨)
	// ����Ҫ��ϵͳmalloc�ڴ棬malloc���ڴ������Ǳ���������ڴ��С������
	else
	{
		size_t __bytes_to_get = 2 * __total_bytes + _S_round_up(_S_heap_size >>4);
		if (__left_bytes > 0)
		{
			_Obj* volatile* __my_free_list;
			__my_free_list = _S_free_list + _S_fresslist_index(__left_bytes); //��λ������freelist��Ա
			// ͷ�巨����
			((_Obj*)(_S_start_free))->_M_free_list_link = *__my_free_list;
			*__my_free_list = (_Obj*)(_S_start_free);
		}
		// ��ϵͳmalloc,Ӧ����_S_start_free�����գ���Ϊ�����ڿ��ٱ����ڴ档
		_S_start_free = (char*)malloc(__bytes_to_get);

		//5.��ϵͳ�����ڴ�ʧ��,�ӵ�ǰfreelist��ʼ����������ж���֮��ĳ�Ա�Ƿ��п��п�
			// ���ٽ���size��С��chunk��
		if (_S_start_free == nullptr)
		{							//	��__my_free_list + _S_freelist_index(__i) ������
			for (size_t __i = __size; __i<_MAX_BYTES; __i = __i + _ALIGN)
			{
				_Obj* __p;
				_Obj* volatile* __my_free_list = _S_free_list + _S_fresslist_index(__i);
				__p = *__my_free_list;
				if (__p != nullptr) // �����������freelist��Ա���п��п��
				{
					// �����freelist��Ա��һ��
					_S_start_free = (char*)__p;
					_S_end_free = _S_start_free + __i;

					// ��__my_free_list��Աָ����һ�����п�
					*__my_free_list = __p->_M_free_list_link;
					
					//�ݹ����_S_chunk_alloc�����½����������ȥ�жϴ�ʱ�����left_bytes��size�Ĺ�ϵ
					// Ȼ����������left_bytes�Ϸ��䣬(1 - 20 ��)�����շ���result
					return (_S_chunk_alloc(__size, __nobjs));
				}
			}

			// ������Ա��û���п飬����malloc_alloc::allocate(__bytes_to_get);
			_S_end_free = nullptr;
			_S_start_free = (char*)malloc_alloc::allocate(__bytes_to_get);
		}

		//�޸���ر�����ֵ
		// (������������ߵ����1.��ϵͳmalloc�ɹ���2.����_size��freelist��Ա��û�п����ڴ棬����malloc_alloc::allocate)
		_S_heap_size += __bytes_to_get;
		_S_end_free = _S_start_free + __bytes_to_get;
		return (_S_chunk_alloc(__size, __nobjs)); // �ݹ����
	}
}

template<typename T>
// �ͷ�pָ��ָ��Ĵ�СΪ__n�Ŀ���ڴ�
void myallocator<T>::deallocate(void*__p, size_t __n)
{
	if (__n > (size_t)_MAX_BYTES) // n>128 ��һ���ռ�������һ��
	{
		malloc_alloc::deallocate(__p, __n); // free(__p)
	}
	else
	{
		// �ҵ���Ӧ��freelist��Ա
		_Obj* volatile* _my_free_list = _S_free_list + _S_fresslist_index(__n);
		_Obj* __q = (_Obj*)__p;
		// ������ڴ�ͷ�巨����,��_S_free_list���в�����Ҫ�����̰߳�ȫ����p
		std::unique_lock<std::mutex> lck(mtx);
		__q->_M_free_list_link = *_my_free_list;
		*_my_free_list = __q;
	}
}