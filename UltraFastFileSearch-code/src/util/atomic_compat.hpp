/**
 * @file atomic_compat.hpp
 * @brief Atomic operations and synchronization primitives compatibility layer
 *
 * Provides atomic types and synchronization primitives for pre-C++11 compilers.
 * When _ATOMIC_ is defined, delegates to std:: atomics.
 * Otherwise provides custom implementations using Windows intrinsics.
 *
 * Contents:
 *   - memory_order enum
 *   - atomic_flag (spinlock building block)
 *   - atomic<bool>, atomic<unsigned int>, atomic<long long>, atomic<unsigned long long>
 *   - unique_lock<Mutex> template
 *   - recursive_mutex (wraps CRITICAL_SECTION on Windows)
 *   - spin_lock class
 *   - spin_atomic<T> template for arbitrary types
 */

#pragma once

#ifndef UFFS_ATOMIC_COMPAT_HPP
#define UFFS_ATOMIC_COMPAT_HPP

#ifdef _ATOMIC_
#include <atomic>
#include <mutex>
#endif

namespace atomic_namespace
{
#ifdef _ATOMIC_
	using namespace std;
#else
	extern "C"
	{
		void _ReadWriteBarrier(void);
	}
#pragma intrinsic(_ReadWriteBarrier)

	enum memory_order
	{
		memory_order_relaxed,
		memory_order_consume,
		memory_order_acquire,
		memory_order_release,
		memory_order_acq_rel,
		memory_order_seq_cst
	};

	struct atomic_flag
	{
		long _value;
		bool test_and_set(memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return !!_interlockedbittestandset(&this->_value, 0);
		}

		void clear(memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			_interlockedbittestandreset(&this->_value, 0);
		}
	};

	template <class T>
	class atomic;

	template <>
	class atomic<bool>
	{
		typedef char storage_type;
		typedef bool value_type;
		storage_type value;
	public:
		atomic() {}

		explicit atomic(value_type
			const value) : value(value) {}

		operator value_type() const volatile
		{
			return this->load();
		}

		atomic volatile& operator=(value_type
			const& value) volatile
		{
			this->store(value);
			return *this;
		}

		value_type exchange(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return !!_InterlockedExchange8(const_cast<storage_type*>(&this->value), static_cast<storage_type>(value));
		}

		value_type load(memory_order
			const order = memory_order_seq_cst) const volatile
		{
			(void)order;
			return !!_InterlockedOr8(const_cast<storage_type*>(&this->value), storage_type());
		}

		value_type store(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return !!_InterlockedExchange8(const_cast<storage_type*>(&this->value), static_cast<storage_type>(value));
		}
	};

	template <>
	class atomic<unsigned int>
	{
		typedef long storage_type;
		typedef unsigned int value_type;
		storage_type value;
	public:
		atomic() {}

		explicit atomic(value_type
			const value) : value(value) {}

		operator value_type() const volatile
		{
			return this->load();
		}

		atomic volatile& operator=(value_type
			const& value) volatile
		{
			this->store(value);
			return *this;
		}

		value_type exchange(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchange(&this->value, static_cast<storage_type>(value)));
		}

		value_type load(memory_order
			const order = memory_order_seq_cst) const volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedOr(const_cast<storage_type volatile*>(&this->value), storage_type()));
		}

		value_type store(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchange(&this->value, static_cast<storage_type>(value)));
		}



		value_type fetch_add(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchangeAdd(&this->value, static_cast<storage_type>(value)));
		}

		value_type fetch_sub(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			return this->fetch_add(value_type() - value, order);
		}
	};

	template<>
	class atomic<long long>
	{
		typedef long long storage_type;
		typedef long long value_type;
		storage_type value;

#ifdef _M_IX86
		static storage_type _InterlockedOr64(storage_type volatile* const result, storage_type
			const value)
		{
			storage_type prev, curr;
			_ReadWriteBarrier();
			do {
				prev = *result;
				curr = prev | value;
			} while (prev != _InterlockedCompareExchange64(result, curr, prev));
			_ReadWriteBarrier();
			return prev;
		}

		static storage_type _InterlockedExchange64(storage_type volatile* const result, storage_type
			const value)
		{
			storage_type prev;
			_ReadWriteBarrier();
			do {
				prev = *result;
			} while (prev != _InterlockedCompareExchange64(result, value, prev));
			_ReadWriteBarrier();
			return (prev);
		}

		static storage_type _InterlockedExchangeAdd64(storage_type volatile* const result, storage_type
			const value)
		{
			storage_type prev, curr;
			_ReadWriteBarrier();
			do {
				prev = *result;
				curr = prev + value;
			} while (prev != _InterlockedCompareExchange64(result, curr, prev));
			_ReadWriteBarrier();
			return prev;
		}
#endif

	public:
		atomic() {}

		explicit atomic(value_type
			const value) : value(value) {}

		operator value_type() const volatile
		{
			return this->load();
		}

		atomic volatile& operator=(value_type
			const& value) volatile
		{
			this->store(value);
			return *this;
		}

		value_type exchange(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchange64(&this->value, static_cast<storage_type>(value)));
		}

		value_type load(memory_order
			const order = memory_order_seq_cst) const volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedOr64(const_cast<storage_type volatile*>(&this->value), storage_type()));
		}

		value_type store(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchange64(&this->value, static_cast<storage_type>(value)));
		}

		value_type fetch_add(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchangeAdd64(&this->value, static_cast<storage_type>(value)));
		}

		value_type fetch_sub(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			return this->fetch_add(value_type() - value, order);
		}
	};

	template<>
	class atomic<unsigned long long>
	{
		typedef long long storage_type;
		typedef unsigned long long value_type;
		storage_type value;

#ifdef _M_IX86
		static storage_type _InterlockedOr64(storage_type volatile* const result, storage_type
			const value)
		{
			storage_type prev, curr;
			_ReadWriteBarrier();
			do {
				prev = *result;
				curr = prev | value;
			} while (prev != _InterlockedCompareExchange64(result, curr, prev));
			_ReadWriteBarrier();
			return prev;
		}

		static storage_type _InterlockedExchange64(storage_type volatile* const result, storage_type
			const value)
		{
			storage_type prev;
			_ReadWriteBarrier();
			do {
				prev = *result;
			} while (prev != _InterlockedCompareExchange64(result, value, prev));
			_ReadWriteBarrier();
			return (prev);
		}

		static storage_type _InterlockedExchangeAdd64(storage_type volatile* const result, storage_type
			const value)
		{
			storage_type prev, curr;
			_ReadWriteBarrier();
			do {
				prev = *result;
				curr = prev + value;
			} while (prev != _InterlockedCompareExchange64(result, curr, prev));
			_ReadWriteBarrier();
			return prev;
		}
#endif

	public:
		atomic() {}

		explicit atomic(value_type
			const value) : value(value) {}

		operator value_type() const volatile
		{
			return this->load();
		}

		atomic volatile& operator=(value_type
			const& value) volatile
		{
			this->store(value);
			return *this;
		}


		value_type exchange(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchange64(&this->value, static_cast<storage_type>(value)));
		}

		value_type load(memory_order
			const order = memory_order_seq_cst) const volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedOr64(const_cast<storage_type volatile*>(&this->value), storage_type()));
		}

		value_type store(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchange64(&this->value, static_cast<storage_type>(value)));
		}

		value_type fetch_add(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			return static_cast<value_type>(_InterlockedExchangeAdd64(&this->value, static_cast<storage_type>(value)));
		}

		value_type fetch_sub(value_type
			const value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			return this->fetch_add(value_type() - value, order);
		}
	};

#endif

#ifndef _MUTEX_
	template<class Mutex>
	class unique_lock
	{
		Mutex* p;
	public:
		typedef Mutex mutex_type;
		~unique_lock()
		{
			if (p)
			{
				p->unlock();
			}
		}

		unique_lock() : p() {}

		explicit unique_lock(mutex_type* const mutex, bool
			const already_locked = false) : p(mutex)
		{
			if (p && !already_locked)
			{
				p->lock();
			}
		}

		explicit unique_lock(mutex_type& mutex, bool
			const already_locked = false) : p(&mutex)
		{
			if (!already_locked)
			{
				p->lock();
			}
		}

		unique_lock(unique_lock
			const& other) : p(other.p)
		{
			if (p)
			{
				p->lock();
			}
		}

		unique_lock& operator=(unique_lock other)
		{
			return this->swap(other), *this;
		}

		void swap(unique_lock& other)
		{
			using std::swap;
			swap(this->p, other.p);
		}

		friend void swap(unique_lock& a, unique_lock& b)
		{
			return a.swap(b);
		}

		mutex_type* mutex() const
		{
			return this->p;
		}
	};

	class recursive_mutex
	{
		void init()
		{
#if defined(_WIN32)
			InitializeCriticalSection(&p);
#elif defined(_OPENMP) || defined(_OMP_LOCK_T)
			omp_init_lock(&p);
#endif
		}

		void term()
		{
#if defined(_WIN32)
			DeleteCriticalSection(&p);
#elif defined(_OPENMP) || defined(_OMP_LOCK_T)
			omp_destroy_lock(&p);
#endif
		}

	public:
		recursive_mutex& operator=(recursive_mutex
			const&)
		{
			return *this;
		}

#if defined(_WIN32)
		CRITICAL_SECTION p;
#elif defined(_OPENMP) || defined(_OMP_LOCK_T)
		omp_lock_t p;
#elif defined(BOOST_THREAD_MUTEX_HPP)
		boost::recursive_mutex m;
#else
		std::recursive_mutex m;
#endif

		recursive_mutex(recursive_mutex
			const&)
		{
			this->init();
		}

		recursive_mutex()
		{
			this->init();
		}

		~recursive_mutex()
		{
			this->term();
		}

		void lock()
		{
#if defined(_WIN32)
			EnterCriticalSection(&p);
#elif defined(_OPENMP) || defined(_OMP_LOCK_T)
			omp_set_lock(&p);
#else
			return m.lock();
#endif
		}

		void unlock()
		{
#if defined(_WIN32)
			LeaveCriticalSection(&p);
#elif defined(_OPENMP) || defined(_OMP_LOCK_T)
			omp_unset_lock(&p);
#else
			return m.unlock();
#endif
		}

		bool try_lock()
		{
#if defined(_WIN32)
			return !!TryEnterCriticalSection(&p);
#elif defined(_OPENMP) || defined(_OMP_LOCK_T)
			return !!omp_test_lock(&p);
#else
			return m.try_lock();
#endif
		}
	};
#endif


	// ============================================================================
	// spin_lock - Simple spinlock based on atomic_flag
	// ============================================================================

	class spin_lock : private atomic_flag
	{
	public: spin_lock() : atomic_flag() {}

		  void lock()
		  {
			  while (this->test_and_set()) {}
		  }

		  void unlock()
		  {
			  this->clear();
		  }
	};

	// ============================================================================
	// spin_atomic<T> - Atomic operations for arbitrary types using spin_lock
	// ============================================================================

	template < class T>
	class spin_atomic
	{
		typedef spin_lock mutex;
		typedef unique_lock<mutex> unique_lock_t;
		mutex _mutex;
		typedef T storage_type;
		typedef T value_type;
		storage_type value;
	public:
		spin_atomic() {}

		explicit spin_atomic(value_type
			const& value) : value(value) {}

		value_type exchange(value_type value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			unique_lock_t
				const lock(const_cast<mutex&> (this->_mutex));
			using std::swap;
			swap(const_cast<storage_type&> (this->value), value);
			return value;
		}

		value_type load(memory_order
			const order = memory_order_seq_cst) const volatile
		{
			(void)order;
			unique_lock_t
				const lock(const_cast<mutex&> (this->_mutex));
			return const_cast<storage_type&> (this->value);
		}

		void store(value_type
			const& value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			unique_lock_t
				const lock(const_cast<mutex&> (this->_mutex));
			const_cast<storage_type&> (this->value) = value;
		}

		value_type fetch_add(value_type
			const& value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			unique_lock_t
				const lock(const_cast<mutex&> (this->_mutex));
			value_type old = const_cast<storage_type&> (this->value);
			const_cast<storage_type&> (this->value) += value;
			return old;
		}

		value_type fetch_sub(value_type
			const& value, memory_order
			const order = memory_order_seq_cst) volatile
		{
			(void)order;
			unique_lock_t
				const lock(const_cast<mutex&> (this->_mutex));
			value_type old = const_cast<storage_type&> (this->value);
			const_cast<storage_type&> (this->value) -= value;
			return old;
		}
	};

} // namespace atomic_namespace

#endif // ATOMIC_COMPAT_HPP