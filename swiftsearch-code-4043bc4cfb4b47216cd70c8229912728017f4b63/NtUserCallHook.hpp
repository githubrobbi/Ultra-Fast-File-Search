#pragma once

#include <memory.h>

#include <Windows.h>

namespace hook_detail
{
	template<class T>
	struct temporary_swap_via_copy  // via copy, to avoid importing <algorithm>
	{
		typedef T value_type;
		typedef temporary_swap_via_copy this_type;
		~temporary_swap_via_copy() { if (ptr) { *ptr = this->_old; } }
		temporary_swap_via_copy() : ptr(), _old() { }
		explicit temporary_swap_via_copy(value_type *const &ptr, value_type const &f) : ptr(ptr), _old(ptr ? *ptr : value_type())
		{
			if (ptr)
			{
				*ptr = f;
			}
		}
		void swap(this_type &other) { { value_type temp(this->_old); this->_old = other._old; other._old = temp; } { value_type *temp(this->ptr); this->ptr = other.ptr; other.ptr = temp; }; }
		friend void swap(this_type &a, this_type &b) { return a.swap(b); }
		value_type const &old() const { return this->_old; }
	private:
		value_type *ptr, _old;
		temporary_swap_via_copy(this_type const &);
		this_type &operator =(this_type const &);
	};

	template<class F> struct vfunction;

	template<class H, class F, class Base> struct static_hook_helper;
	template<class F> struct thread_hook_swap_base;

#define Z()
#if defined(_M_X64) || defined(_WIN64)
#define G(W, X, Y) X(W, Y, Z())
#else
#define G(W, X, Y) X(W, Y, __cdecl); X(W, Y, __stdcall)
#endif
#define X(W, Y, CC)  \
	template<W() class R, class T1> Y(R, CC, (T1 v1), (v1));  \
	template<W() class R, class T1, class T2> Y(R, CC, (T1 v1, T2 v2), (v1, v2));  \
	template<W() class R, class T1, class T2, class T3> Y(R, CC, (T1 v1, T2 v2, T3 v3), (v1, v2, v3));  \
	template<W() class R, class T1, class T2, class T3, class T4> Y(R, CC, (T1 v1, T2 v2, T3 v3, T4 v4), (v1, v2, v3, v4));  \
	template<W() class R, class T1, class T2, class T3, class T4, class T5> Y(R, CC, (T1 v1, T2 v2, T3 v3, T4 v4, T5 v5), (v1, v2, v3, v4, v5));  \
	template<W() class R, class T1, class T2, class T3, class T4, class T5, class T6> Y(R, CC, (T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6), (v1, v2, v3, v4, v5, v6));  \
	template<W() class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7> Y(R, CC, (T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7), (v1, v2, v3, v4, v5, v6, v7));  \
	template<W() class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8> Y(R, CC, (T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8), (v1, v2, v3, v4, v5, v6, v7, v8));  \
	template<W() class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9> Y(R, CC, (T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9), (v1, v2, v3, v4, v5, v6, v7, v8, v9));  \
	template<W() class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10> Y(R, CC, (T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10), (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10));  \
	template<W() class R, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11> Y(R, CC, (T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10, T11 v11), (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11))
#define Y(R, CC, P, A) struct vfunction<R CC P> { virtual R CC operator() P = 0; }
#define W()
	G(W, X, Y);
#undef  W
#undef  Y

#define Y(R, CC, P, A) struct static_hook_helper<H, R CC P, Base> : vfunction<R CC P>, Base { protected: R CC operator() P /* override */ { return static_cast<H *>(this)->base A; } static R CC invoke P { typename H::thread_value_type *const p = &H::thread(); return (p && *p) ? (*p)->operator() A : (*H::instance()) A; } }
#define W() class Base, class H,
	G(W, X, Y);
#undef  W
#undef  Y

	template<class F> struct thread_hook_swap_callable;
#define Y(R, CC, P, A) struct thread_hook_swap_callable<R CC P> { typedef R CC func_type P; typedef thread_hook_swap_base<func_type> B; virtual R operator() P { typename B::base_pair_type const &base = static_cast<B &>(*this).base(); return base.first ? (*base.first) A : (*base.second) A; } }
#define W()
	G(W, X, Y);
#undef  W
#undef  Y
#undef  X
#undef  G
#undef  Z

	template<class F>
	struct thread_hook_swap_base : public thread_hook_swap_callable<F>
	{
		typedef thread_hook_swap_base hook_base_type;
		typedef thread_hook_swap_callable<F> base_type;
		typedef base_type *value_type;
		struct base_pair_type { typedef value_type first_type; typedef vfunction<F> *second_type; first_type first; second_type second; };
		explicit thread_hook_swap_base(value_type &value) :
#pragma warning(push)
#pragma warning(disable: 4355)
			_swap(&value, this)
#pragma warning(pop)

		{
		}
		virtual base_pair_type base() const = 0;
	protected:
		value_type const &old() const { return this->_swap.old(); }
	private:
		temporary_swap_via_copy<value_type> _swap;
		typedef thread_hook_swap_base this_type;
		// Disable copying/moving because we carry a self-pointer and I'm too lazy to implement allowing that
		thread_hook_swap_base(this_type const &);
		this_type &operator =(this_type const &);
	};

	template<class H>
	struct thread_hook_swap : public thread_hook_swap_base<typename H::func_type>
	{
		typedef thread_hook_swap_base<typename H::func_type> base_type;
		typedef typename base_type::value_type value_type;
		static value_type &value()
		{
			__declspec(thread) static value_type f = 0;
			return f;
		}
		thread_hook_swap() : base_type(value()) { }
	private:
		typename base_type::base_pair_type base() const /* override */ { typename base_type::base_pair_type result; result.first = this->base_type::old(); result.second = H::instance(); return result; }
	};

	typedef intptr_t(__stdcall *FARPROC)(void);

	template<class H, class F, class Base>
	struct static_hook : static_hook_helper<H, F, Base>
	{
		typedef F func_type;
		typedef thread_hook_swap_callable<func_type> *thread_value_type;
		func_type *base;
		static_hook() : base() { (void)static_cast<this_type *>(this) /* ensure derived */; }
		bool init(FARPROC f)
		{ typedef typename Base::func_ptr_type P; return this->Base::init(reinterpret_cast<P *&>(base), reinterpret_cast<P *>(f), reinterpret_cast<P *>(&H::invoke)); }
		static thread_value_type &thread() { return thread_hook_swap<this_type>::value(); }
		static H *instance() { return &_instance; }
	protected:
		static H _instance;
		typedef static_hook base_type;
		typedef H this_type;
	};
}

class Hook
{
	typedef Hook this_type;
	Hook(this_type const &);
	this_type &operator =(this_type const &);
	int set_hook(void *new_proc);
	int unset_hook();
	hook_detail::FARPROC **ptr, *old_func;
	unsigned char old_proc[32];
	size_t old_proc_size;
protected:
	typedef hook_detail::FARPROC func_ptr_type;
	~Hook() { this->term(); }
	Hook() : ptr(), old_func(), old_proc_size() { }
	bool init(func_ptr_type *&ptr, func_ptr_type *old_func, func_ptr_type *new_func)
	{
		bool result = false;
		if (!this->ptr && old_func)
		{
			this->ptr = &ptr;
			if (this->ptr && !*this->ptr)
			{
				*this->ptr = old_func;
				result = !!this->set_hook(reinterpret_cast<void *>(new_func));
			}
		}
		return result;
	}
public:
	void term()
	{
		if (this->ptr)
		{
			if (*this->ptr)
			{
				if (this->unset_hook())
				{
					*this->ptr = 0;
				}
			}
			this->ptr = 0;
		}
	}
	virtual char const *name() const { return NULL; }
};

int Hook::set_hook(void *new_proc)
{
	unsigned char proc_buf_size;
	unsigned char *const proc_buf = reinterpret_cast<unsigned char *const &>(*this->ptr);
#ifdef _WIN64
	if (memcmp(&proc_buf[0], "\x4C\x8B\xD1\xB8", 4) == 0 &&
		memcmp(&proc_buf[8], "\xF6\x04\x25", 3) == 0 &&
		memcmp(&proc_buf[16], "\x75\x03\x0F\x05\xC3\xCD\x2E\xC3", 8) == 0)
	{
		proc_buf_size = 24;
	}
#else
	if (memcmp(&proc_buf[0], "\xB8", 1) == 0 &&
		memcmp(&proc_buf[5], "\xE8", 1) == 0 &&
		memcmp(&proc_buf[10], "\xC2", 1) == 0)  // raw 32-bit
	{
		proc_buf_size = 13;
	}
	else if (memcmp(&proc_buf[0], "\xB8", 1) == 0 &&
		memcmp(&proc_buf[5], "\xBA", 1) == 0 &&
		memcmp(&proc_buf[10], "\xFF", 1) == 0 &&
		memcmp(&proc_buf[12], "\xC2", 1) == 0)  // WOW64
	{
		proc_buf_size = 15;
	}
#endif
	else { proc_buf_size = 0; }

	int r;
	DWORD old_protect;
	if (proc_buf_size && VirtualProtect(this->old_proc, proc_buf_size, PAGE_EXECUTE_READWRITE, &old_protect) &&
		VirtualProtect(proc_buf, proc_buf_size, PAGE_EXECUTE_READWRITE, &old_protect))
	{
#ifndef _WIN64
		if (memcmp(&proc_buf[5], "\xE8", 1) == 0)  // raw 32-bit, relative jump; requires fix-up
		{
			ptrdiff_t rel = 0;
			std::copy(&proc_buf[6], &proc_buf[6 + sizeof(rel)], *reinterpret_cast<unsigned char(*)[sizeof(rel)]>(&rel));
			rel += &proc_buf[6 + sizeof(rel)] - &this->old_proc[6 + sizeof(rel)];
			std::copy(reinterpret_cast<unsigned char const *>(&rel), reinterpret_cast<unsigned char const *>(&rel + 1), *reinterpret_cast<unsigned char(*)[sizeof(rel)]>(&proc_buf[6]));
		}
#endif
		std::copy(&proc_buf[0], &proc_buf[proc_buf_size], this->old_proc);
		ptrdiff_t j = 0;
#ifdef _WIN64
		proc_buf[j++] = 0x48;
#endif
		proc_buf[j++] = 0xB8;
		j += std::copy(reinterpret_cast<unsigned char const *>(&new_proc), reinterpret_cast<unsigned char const *>(&new_proc + 1), &proc_buf[j]) - &proc_buf[j];
		proc_buf[j++] = 0xFF;
		proc_buf[j++] = 0xE0;
		FlushInstructionCache(GetCurrentProcess(), proc_buf, proc_buf_size);
		VirtualProtect(proc_buf, proc_buf_size, old_protect, &old_protect);
		this->old_func = *this->ptr;
		this->old_proc_size = proc_buf_size;
		*this->ptr = reinterpret_cast<func_ptr_type *>(&this->old_proc[0]);
		r = 1;
	}
	else { r = 0; }
	return r;
}

int Hook::unset_hook()
{
	int r;
	size_t const proc_buf_size = this->old_proc_size;
	unsigned char *const proc_buf = reinterpret_cast<unsigned char *const &>(this->old_func);
	DWORD old_protect;
	if (proc_buf_size && VirtualProtect(proc_buf, proc_buf_size, PAGE_EXECUTE_READWRITE /* no execute permission, to make sure nobody calls it concurrently */, &old_protect))
	{
		std::copy(&this->old_proc[0], &this->old_proc[proc_buf_size], proc_buf);
#ifndef _WIN64
		if (memcmp(&proc_buf[5], "\xE8", 1) == 0)  // raw 32-bit, relative jump; requires fix-up
		{
			ptrdiff_t rel = 0;
			std::copy(&proc_buf[6], &proc_buf[6 + sizeof(rel)], *reinterpret_cast<unsigned char(*)[sizeof(rel)]>(&rel));
			rel -= &proc_buf[6 + sizeof(rel)] - &this->old_proc[6 + sizeof(rel)];
			std::copy(reinterpret_cast<unsigned char const *>(&rel), reinterpret_cast<unsigned char const *>(&rel + 1), *reinterpret_cast<unsigned char(*)[sizeof(rel)]>(&proc_buf[6]));
		}
#endif
		FlushInstructionCache(GetCurrentProcess(), proc_buf, proc_buf_size);
		VirtualProtect(proc_buf, proc_buf_size, old_protect, &old_protect);
		this->old_func = NULL;
		r = 1;
	}
	else { r = 0; }
	return r;
}

#define HOOK_STRINGIZE_(Name) #Name
#define HOOK_STRINGIZE(Name) HOOK_STRINGIZE_(Name)
#define HOOK_EMPTY_XARGS(Name)
#define HOOK_CONCAT(Prefix, Suffix) Prefix##Suffix
#define HOOK_TYPE(Name) HOOK_CONCAT(Hook_, Name)
#define HOOK_DECLARE(ConstructorCallback, Name, FuncType, ExtraMembers) struct HOOK_TYPE(Name) : hook_detail::static_hook<HOOK_TYPE(Name), FuncType, Hook>  \
	{  \
		explicit HOOK_TYPE(Name)(hook_detail::FARPROC const proc = NULL) { if (proc) { this->init(proc); } ConstructorCallback(*this); }  \
		static char const *static_name() { return #Name; }  \
		char const *name() const { return static_name(); }  \
		ExtraMembers(Name)  \
	}
#define HOOK_IMPLEMENT(Name, FuncType, XArgs)  \
	hook_detail::static_hook<HOOK_TYPE(Name), FuncType, Hook>::_instance XArgs(Name)
#define HOOK_DEFINE_DEFAULT(ReturnType, Name, Params, XArgs)  \
	HOOK_DECLARE(void, Name, ReturnType Params, HOOK_EMPTY_XARGS);  \
	template<> HOOK_TYPE(Name) HOOK_IMPLEMENT(Name, ReturnType Params, XArgs)
