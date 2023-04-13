#pragma once

// https://sourceforge.net/p/predef/wiki/Libraries/ for _CPPLIB_VER

#ifdef __cplusplus

#pragma warning(disable: 4005)  // 'WCHAR_{MIN,MAX}': macro redefinition

#pragma warning(push)
#pragma warning(disable: 4265)  // class has virtual functions, but destructor is not virtual
#pragma warning(disable: 4365)  // conversion from '...' to '...', signed/unsigned mismatch
#pragma warning(disable: 4435)  // Object layout under /vd2 will change due to virtual base '...'
#pragma warning(disable: 4571)  // Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
#pragma warning(disable: 4619)  // there is no warning number ''
#pragma warning(disable: 4625)  // copy constructor could not be generated because a base class copy constructor is inaccessible or deleted
#pragma warning(disable: 4626)  // assignment operator could not be generated because a base class assignment operator is inaccessible or deleted
#pragma warning(disable: 4643)  // Forward declaring '' in namespace std is not permitted by the C++ Standard.
#pragma warning(disable: 4668)  // '...' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning(disable: 4710)  // function not inlined
#pragma warning(disable: 4711)  // function selected for automatic inline expansion
#pragma warning(disable: 4774)  // format string is not a string literal
#pragma warning(disable: 4820)  // bytes padding added after data member '...'
#pragma warning(disable: 4986)  // 'operator new': exception specification does not match previous declaration
#pragma warning(disable: 5026)  // move constructor was implicitly defined as deleted
#pragma warning(disable: 5027)  // move assignment operator was implicitly defined as deleted

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wchar-subscripts"
#pragma clang diagnostic ignored "-Wcomma"
#pragma clang diagnostic ignored "-Wdangling-else"
#pragma clang diagnostic ignored "-Wdeprecated"  // warning : definition of implicit copy constructor for '...' is deprecated because it has a user-declared destructor
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wlogical-op-parentheses"
#pragma clang diagnostic ignored "-Wmicrosoft-cast"
#pragma clang diagnostic ignored "-Wmicrosoft-template"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wreorder"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wtypename-missing"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wuninitialized"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-template"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-value"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#ifndef __in_z
#define __in_z
#endif
#ifndef __out_z
#define __out_z
#endif
#ifndef _Check_return_
#define _Check_return_
#endif
#ifndef _In_
#define _In_
#endif
#ifndef _In_z_
#define _In_z_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif
#ifndef _In_opt_z_
#define _In_opt_z_
#endif
#ifndef _In_reads_
#define _In_reads_(_)
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _Inout_updates_
#define _Inout_updates_(_)
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Out_opt_
#define _Out_opt_
#endif
#ifndef _Outptr_
#define _Outptr_
#endif
#ifndef _Outptr_result_z_
#define _Outptr_result_z_
#endif
#ifndef _Outptr_result_maybenull_
#define _Outptr_result_maybenull_
#endif
#ifndef _Out_writes_
#define _Out_writes_(_)
#endif
#ifndef _Out_writes_opt_
#define _Out_writes_opt_(_)
#endif
#ifndef _Field_range_
#define _Field_range_(min, max)
#endif

#ifndef _CONSTEXPR
#if __cplusplus >= 201402L || defined(_MSC_VER) && _MSC_VER >= 1900 /* VC++ requirement for constexpr is 2015; we want constexpr max() for <regex> to work */
#define _CONSTEXPR constexpr
#else
#define _CONSTEXPR
#endif
#endif

#ifdef  _CSTDLIB_
#error include <cstdlib> already happened
#endif
#include <cstdlib>  // needed for _CPPLIB_VER
#ifdef  _CSTDDEF_
#error include <cstddef> already happened
#endif
#include <cstddef>

#if defined(_MSC_VER) && !defined(_CPPLIB_VER) || _CPPLIB_VER < 403

#define _STRALIGN_USE_SECURE_CRT 0
#define _FORWARD_LIST_  // prevent inclusion of this
#define _TUPLE_  // prevent inclusion of this
#define _TYPE_TRAITS_  // prevent inclusion of this

#ifndef _CONCATX
#define _CONCATX(x, y) x ## y
#endif
#ifndef _CONCAT
#define _CONCAT(x, y) _CONCATX(x, y)
#endif

#ifndef _CRT_STRINGIZE  // Might be already defined in crtdefs.h, but if not...
#define __CRT_STRINGIZE(Value) #Value
#define _CRT_STRINGIZE(Value) __CRT_STRINGIZE(Value)
#endif

#ifndef _XSTD
#define _X_STD_BEGIN _STD_BEGIN
#define _X_STD_END _STD_END
#define _XSTD _STD
#endif

#ifndef _NO_RETURN
#if defined(_MSC_VER) && _MSC_VER < 1900
#define _NO_RETURN(F) __declspec(noreturn) void F
#else
#define _NO_RETURN(F) [[noreturn]] void F
#endif
#endif

#ifndef _EXTERN_C
#ifdef __cplusplus
#define _EXTERN_C extern "C" {
#else
#define _EXTERN_C
#endif
#endif
#ifndef _END_EXTERN_C
#ifdef __cplusplus
#define _END_EXTERN_C }
#else
#define _END_EXTERN_C
#endif
#endif
#if defined(_HAS_EXCEPTIONS) &&_HAS_EXCEPTIONS
#define _NOEXCEPT	noexcept
#define _NOEXCEPT_OP(x)	noexcept(x)
#else
#define _NOEXCEPT	throw ()
#define _NOEXCEPT_OP(x)
#endif

#ifndef _CRTIMP2
#define _CRTIMP2
#endif
#ifndef _CRTIMP2_PURE
#define _CRTIMP2_PURE _CRTIMP2
#endif

#ifndef __CLRCALL_PURE_OR_CDECL
#ifdef _M_CEE_PURE
#define __CLRCALL_PURE_OR_CDECL __clrcall
#else
#define __CLRCALL_PURE_OR_CDECL __cdecl
#endif
#endif

extern "C" long __cdecl _InterlockedIncrement(long volatile *lpAddend);
#ifndef _MT_INCR
#define _MT_INCR(x) _InterlockedIncrement(reinterpret_cast<long volatile *>(&x))
#endif
extern "C" long __cdecl _InterlockedDecrement(long volatile *lpAddend);
#ifndef _MT_DECR
#define _MT_DECR(x) _InterlockedDecrement(reinterpret_cast<long volatile *>(&x))
#endif

#if __cplusplus >= 201103L || defined(__GXX_EXPERIMENTAL_CXX0X__) || defined(_MSC_VER) && _MSC_VER >= 1600
#define X_HAS_MOVE_SEMANTICS
#elif defined(__clang__)
#if __has_feature(cxx_rvalue_references)
#define X_HAS_MOVE_SEMANTICS
#endif
#endif

#define _LONGLONG long long
typedef unsigned _LONGLONG _ULONGLONG;

extern "C"
{
#ifdef _WIN64
	typedef __int64(__stdcall *FARPROC)();
#else
	typedef int(__stdcall *FARPROC)();
#endif
	__declspec(dllimport) FARPROC __stdcall GetProcAddress(struct HINSTANCE__ *hModule, char const *lpProcName);
	__declspec(dllimport) int __stdcall GetModuleHandleExA(unsigned long dwFlags, char const *lpModuleName, struct HINSTANCE__** phModule);
	__declspec(dllimport) int __stdcall GetModuleHandleExW(unsigned long dwFlags, wchar_t const *lpModuleName, struct HINSTANCE__** phModule);
#if defined(_MSC_VER) && _MSC_VER >= 1400
#if defined(_WIN64) || defined(_M_X64)  // For some reason _M_X64 doesn't seem to be defined in WinDDK 2003 x64
	void *__cdecl _InterlockedCompareExchangePointer(void *volatile *Destination, void *ExChange, void *Comparand);
#	pragma intrinsic(_InterlockedCompareExchangePointer)
#else
#if _MSC_VER < 1800
	long __cdecl _InterlockedCompareExchange(long volatile *, long, long);
#	pragma intrinsic(_InterlockedCompareExchange)
	static void *__cdecl _InterlockedCompareExchangePointer(void *volatile *Destination, void *ExChange, void *Comparand)
	{
		return (void *)_InterlockedCompareExchange((long volatile *)Destination, (long)ExChange, (long)Comparand);
	}
#else
	void *__cdecl _InterlockedCompareExchangePointer(void *volatile *Destination, void *ExChange, void *Comparand);
#pragma intrinsic(_InterlockedCompareExchangePointer)
#endif
#endif
#endif
}
#ifndef _C_STD_BEGIN
#	define _C_STD_BEGIN namespace std {
#endif
#ifndef _C_STD_END
#	define _C_STD_END }
#endif
#ifndef _CSTD
#	define _CSTD ::std::
#endif

namespace std
{
	template<bool, class T1, class T2> struct _If { typedef T2 type; };
	template<class T1, class T2> struct _If<true, T1, T2> { typedef T1 type; };
	template<class T, T v>
	struct integral_constant
	{
		static T const value = v;
		typedef T value_type;
		typedef integral_constant type;
		operator value_type() const _NOEXCEPT { return value; }
		value_type operator()() const _NOEXCEPT { return value; }
	};
	typedef integral_constant<bool, true > true_type;
	typedef integral_constant<bool, false> false_type;
	template<bool B> struct _Cat_base : integral_constant<bool, B> { };
	template<class> struct is_unsigned;
	template<> struct is_unsigned<unsigned char     > : true_type { };
	template<> struct is_unsigned<unsigned short    > : true_type { };
	template<> struct is_unsigned<unsigned int      > : true_type { };
	template<> struct is_unsigned<unsigned long     > : true_type { };
	template<> struct is_unsigned<unsigned long long> : true_type { };
	template<> struct is_unsigned<         char     > : _Cat_base<(~char() >= 0)> { };
#if defined(_NATIVE_WCHAR_T_DEFINED) && _NATIVE_WCHAR_T_DEFINED
	template<> struct is_unsigned<        wchar_t   > : _Cat_base<(~wchar_t() >= 0)> { };
#endif
	template<> struct is_unsigned<  signed char     > : false_type { };
	template<> struct is_unsigned<  signed short    > : false_type { };
	template<> struct is_unsigned<  signed int      > : false_type { };
	template<> struct is_unsigned<  signed long     > : false_type { };
	template<> struct is_unsigned<  signed long long> : false_type { };
	template<class> struct make_unsigned;
	template<> struct make_unsigned<unsigned char     > { typedef unsigned char      type; };
	template<> struct make_unsigned<unsigned short    > { typedef unsigned short     type; };
	template<> struct make_unsigned<unsigned int      > { typedef unsigned int       type; };
	template<> struct make_unsigned<unsigned long     > { typedef unsigned long      type; };
	template<> struct make_unsigned<unsigned long long> { typedef unsigned long long type; };
	template<> struct make_unsigned<       char     > : make_unsigned<unsigned char     > { };
	template<> struct make_unsigned<signed char     > : make_unsigned<unsigned char     > { };
	template<> struct make_unsigned<signed short    > : make_unsigned<unsigned short    > { };
	template<> struct make_unsigned<signed int      > : make_unsigned<unsigned int      > { };
	template<> struct make_unsigned<signed long     > : make_unsigned<unsigned long     > { };
	template<> struct make_unsigned<signed long long> : make_unsigned<unsigned long long> { };
#ifdef X_HAS_MOVE_SEMANTICS
	template<class T> struct add_rvalue_reference { typedef T &&type; };
	template<> struct add_rvalue_reference<void> { typedef void type; };
	template<> struct add_rvalue_reference<void const> { typedef void const type; };
	template<> struct add_rvalue_reference<void volatile> { typedef void volatile type; };
	template<> struct add_rvalue_reference<void const volatile> { typedef void const volatile type; };
#endif
	template<class T> struct remove_const          { typedef T type; };
	template<class T> struct remove_const<T const> { typedef T type; };
	template<class T> struct remove_volatile             { typedef T type; };
	template<class T> struct remove_volatile<T volatile> { typedef T type; };
	template<class T> struct remove_cv { typedef typename remove_volatile<typename remove_const<T>::type>::type type; };
	template<class T> struct remove_reference       { typedef T type; };
	template<class T> struct remove_reference<T &>  { typedef T type; };
#ifdef X_HAS_MOVE_SEMANTICS
	template<class T> struct remove_reference<T &&> { typedef T type; };
#endif
	template<class> struct _Always_false { static bool const value = false; };
	template<class _Ty> struct _Is_floating_point : false_type { };
	template<> struct _Is_floating_point<float> : true_type { };
	template<> struct _Is_floating_point<double> : true_type { };
	template<> struct _Is_floating_point<long double> : true_type { };
	template<class _Ty> struct is_floating_point : _Is_floating_point<typename remove_cv<_Ty>::type> { };
	template<class T1, class T2 = T1, class T3 = T2> struct common_type;
	template<class T> struct common_type<T, T, T> { typedef T type; };
#if _MSC_VER >= 1900
	template<class T, T... Vals> struct integer_sequence;
	static struct piecewise_construct_t { } const piecewise_construct = piecewise_construct_t();
#endif
	template<class, class> struct is_same : false_type {};
	template<class T> struct is_same<T, T> : true_type { };
	template<class T> struct is_void : is_same<void, typename remove_cv<typename remove_reference<T>::type>::type> { };
	template<class> struct is_lvalue_reference : false_type { };
	template<class T> struct is_lvalue_reference<T &> : true_type { };
	template<class> struct is_rvalue_reference : false_type { };
#ifdef X_HAS_MOVE_SEMANTICS
	template<class T> struct is_rvalue_reference<T &&> : true_type { };
#endif
	template<class T> struct is_reference : _Cat_base<is_lvalue_reference<T>::value || is_rvalue_reference<T>::value> { };
	template<class T> struct is_function;
	template<class T> struct is_function<T &> : false_type { };
#ifdef X_HAS_MOVE_SEMANTICS
	template<class T> struct is_function<T &&> : false_type { };
#endif
	template<> struct is_function<void>
	{
		static bool const value = false;
	protected:
		template<class U>
		static char (&test(U const &))[1];
	};
	template<class T> struct is_function : is_function<void>
	{
	private:
		using is_function<void>::test;
		static T &declval();
		static char (&test(T *))[2];
	public:
		static bool const value = sizeof(test(declval())) > 1;
	};
	template<class T> struct is_object : _Cat_base<!is_function<T>::value && !is_reference<T>::value && !is_void<T>::value> { };

#if defined(_MSC_VER) && _MSC_VER >= 1900
	template<class T> struct is_trivially_copyable : _Cat_base<__is_trivially_copyable(T) /* On older _MSC_VER < 1900 this would be different */> { };
#endif
#ifdef X_HAS_MOVE_SEMANTICS
	template<class T>
	typename remove_reference<T>::type &&move(T &&value) _NOEXCEPT { return static_cast<typename remove_reference<T>::type &&>(value); } 
	template<class T> T &&forward(typename remove_reference<T>::type& _Arg) _NOEXCEPT { return static_cast<T &&>(_Arg); }
	template<class T> T &&forward(typename remove_reference<T>::type&& _Arg) _NOEXCEPT { unsigned char bad_forward_call_checker[is_lvalue_reference<T>::value]; return static_cast<T &&>(_Arg); }
	template<class T> /* TODO: WARN: technically this is a wrong implementation... if is_function<T>::value || is_void<T>::value, it should not add an r-value reference at all */
	T &&declval() _NOEXCEPT;
#endif
}

#ifdef _YVALS
#error include <yvals.h> already happened
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1910
#define _XKEYCHECK_H
#define _VCRUNTIME_H
#define _CRT_BEGIN_C_HEADER __pragma(pack(push, 8)) extern "C" {
#define _CRT_END_C_HEADER  } __pragma(pack(pop))
#define mbstate_t mbstate_t_old
#include <yvals_core.h>
#undef  mbstate_t
#undef  _CRT_END_C_HEADER
#undef  _CRT_BEGIN_C_HEADER
#undef  _VCRUNTIME_H
#undef  _XKEYCHECK_H
#undef __cpp_lib_integer_sequence  // The DDK doesn't define this, but yvals_core.h does
#undef _CPPLIB_VER  // The DDK doesn't define this, but yvals_core.h does
#endif
#include <use_ansi.h>  // do this before #include <yvals.h> to avoid this header being affected
#pragma push_macro("_DLL")
#pragma push_macro("_CRTIMP")
#pragma push_macro("_MT")
#undef  _DLL
#undef  _MT  // so that _Lockit gets optimized out
#undef  _CRTIMP
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#endif
#include <yvals.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#pragma pop_macro("_MT")
#pragma pop_macro("_CRTIMP")
#pragma pop_macro("_DLL")

#ifdef assert
#error include <assert.h> already happened
#endif
#define _assert(A, B, C) _assert(const A, const B, C)
#include <cassert>
#undef  _assert

#include <cstdio>  // do this before #include <iosfwd> to avoid affecting that header
#include <cstring> // do this before #include <iosfwd> to avoid affecting that header
#include <cwchar>  // do this before #include <iosfwd> to avoid affecting that header
#include <xstddef> // do this before #include <iosfwd> to avoid affecting that header

#pragma push_macro("_DLL")
#pragma push_macro("_CRTIMP")
#undef _DLL
#undef  _CRTIMP
#define _CRTIMP
#ifdef _IOSFWD_
#error include <_IOSFWD_> already happened
#endif
#include <iosfwd>
#pragma pop_macro("_CRTIMP")
#pragma pop_macro("_DLL")

namespace std { template<class C, class T, class D = ptrdiff_t, class P = T *, class R = void> struct iterator; }
#define iterator iterator_bad
#define inserter inserter_bad
#define insert_iterator insert_iterator_bad
#define back_inserter back_inserter_bad
#define back_insert_iterator back_insert_iterator_bad
#define iterator_traits iterator_traits_bad
#define reverse_iterator reverse_iterator_bad
#ifdef  _UTILITY_
#error include <utility> already happened
#endif
#include <utility>  // iterator_traits
template<class T>
struct std::iterator_traits_bad<T *>
{
	typedef std::random_access_iterator_tag iterator_category;
	typedef T value_type;
	typedef ptrdiff_t difference_type;
	typedef ptrdiff_t distance_type;
	typedef T *pointer;
	typedef T &reference;
};
#undef reverse_iterator
namespace std { template<class RanIt, class T = typename iterator_traits<RanIt>::value_type, class R = T &, class P = T *, class D = ptrdiff_t> class reverse_iterator; }
#ifdef  _ITERATOR_
#error include <iterator> already happened
#endif
#include <iterator>
#undef iterator_traits
#undef back_insert_iterator
#undef back_inserter
#undef insert_iterator
#undef inserter
#undef iterator

typedef __int64 _Longlong;
typedef unsigned __int64 _ULonglong;

#ifdef  _INC_MATH
#error include <math.h> already happened
#endif
#include <math.h>  // ::ceil
#ifdef  _LIMITS_
#error include <limits> already happened
#endif
#define numeric_limits numeric_limits_bad
#include <limits>
#undef  numeric_limits
#ifndef SIZE_MAX
static size_t const SIZE_MAX = ~size_t();
#endif

namespace
{
	size_t strnlen( char   const *s, size_t n) { size_t i = 0; if (s) { while (i < n && s[i]) { ++i; } } return i; }
	size_t wcsnlen(wchar_t const *s, size_t n) { size_t i = 0; if (s) { while (i < n && s[i]) { ++i; } } return i; }
}

namespace std
{
	typedef long long intmax_t;
	typedef unsigned long long uintmax_t;
	using ::ptrdiff_t;
	using ::size_t;
#ifdef  _WIN64
	typedef long long intptr_t;
#else
	typedef _W64 int intptr_t;
#endif
#ifdef  _WIN64
	typedef unsigned long long uintptr_t;
#else
	typedef _W64 unsigned int uintptr_t;
#endif
	using ::memcmp;
	using ::memcpy;
	using ::memset;
	using ::strlen;
	using ::wcslen;
	using ::abort;
	using ::strerror;
	using ::ceil;
	using ::va_list;

	template<class T> T *operator &(T &p) { return reinterpret_cast<T *>(&reinterpret_cast<unsigned char &>(p)); }
	template<class T> T const *operator &(T const &p) { return reinterpret_cast<T const *>(&reinterpret_cast<unsigned char const &>(p)); }
	template<class T> T volatile *operator &(T volatile &p) { return reinterpret_cast<T volatile *>(&reinterpret_cast<unsigned char volatile &>(p)); }
	template<class T> T const volatile *operator &(T const volatile &p) { return reinterpret_cast<T const volatile *>(&reinterpret_cast<unsigned char const volatile &>(p)); }

	template<bool B,class T= void> struct enable_if {};
	template<class T> struct enable_if<true, T> { typedef T type; };

	template<class T> struct numeric_limits : numeric_limits_bad<T> { };
	template<> struct numeric_limits<unsigned char> : numeric_limits_bad<unsigned char>
	{
		static _CONSTEXPR _Ty(min)() { return 0; }
		static _CONSTEXPR _Ty(max)() { return UCHAR_MAX; }
	};
	template<> struct numeric_limits<unsigned short> : numeric_limits_bad<unsigned short>
	{
		static _CONSTEXPR _Ty(min)() { return 0; }
		static _CONSTEXPR _Ty(max)() { return USHRT_MAX; }
	};
	template<> struct numeric_limits<unsigned int> : numeric_limits_bad<unsigned int>
	{
		static _CONSTEXPR _Ty(min)() { return 0; }
		static _CONSTEXPR _Ty(max)() { return UINT_MAX; }
	};

	// TODO: Conflicts with <boost/limits.hpp> due to BOOST_NO_MS_INT64_NUMERIC_LIMITS
	// template<> class numeric_limits<long long> { };
	// template<> class numeric_limits<unsigned long long> { };

	template<class T>
	struct ref_or_void { typedef T &type; };

	template<>
	struct ref_or_void<void> { typedef void type; };

	template<class C, class T, class D, class P, class R>
	struct iterator : public iterator_bad<C, T, D>
	{
		typedef D difference_type;
		typedef P pointer;
		typedef R reference;
	};

	template<class C>
	class insert_iterator : public iterator<output_iterator_tag, void, void>
	{
	public:
		typedef C container_type;
		typedef typename C::value_type value_type;
		insert_iterator(C& _X, typename C::iterator _I) : container(&_X), iter(_I) {}
		insert_iterator<C>& operator=(const value_type& _V) { iter = container->insert(iter, _V); ++iter; return (*this); }
		insert_iterator<C>& operator*() { return (*this); }
		insert_iterator<C>& operator++() { return (*this); }
		insert_iterator<C>& operator++(int) { return (*this); }
	protected:
		C *container;
		typename C::iterator iter;
	};

	template<class C, class _XI>
	inline insert_iterator<C> inserter(C& _X, _XI _I)
	{ return (insert_iterator<C>(_X, typename C::iterator(_I))); }

	template<typename T, typename Sign>
	struct has_push_back
	{
		typedef char yes[1];
		typedef char no [2];
		template <typename U, U> struct type_check;
		template <typename _1> static yes &chk(type_check<Sign, &_1::push_back> *);
		template <typename   > static no  &chk(...);
		enum { value = sizeof(chk<T>(0)) == sizeof(yes) };
	};

	template<class C, class T> static typename enable_if< has_push_back<C, void(C::*)(T const &)>::value>::type push_back(C &c, T const &v) { c.push_back(v); }
	template<class C, class T> static typename enable_if<!has_push_back<C, void(C::*)(T const &)>::value>::type push_back(C &c, T const &v) { c.insert(c.end(), v); }

	template<class C>
	class back_insert_iterator : public iterator<output_iterator_tag, void, void>
	{
	public:
		typedef C container_type;
		typedef typename C::value_type value_type;
		explicit back_insert_iterator(C& _X) : container(&_X) {}
		back_insert_iterator<C>& operator=(const value_type& _V) { push_back(*container, _V); return (*this); }
		back_insert_iterator<C>& operator*() { return (*this); }
		back_insert_iterator<C>& operator++() { return (*this); }
		back_insert_iterator<C>& operator++(int) { return (*this); }
	protected:
		C *container;
	};

	template<class C>
	inline back_insert_iterator<C> back_inserter(C& _X)
	{ return (back_insert_iterator<C>(_X)); }

	template<class It>
	struct iterator_traits //: public iterator_traits_bad<It>
	{
		typedef typename It::iterator_category iterator_category;
		typedef typename It::value_type value_type;
		typedef ptrdiff_t difference_type;
		typedef difference_type distance_type;
		typedef value_type *pointer;
		typedef value_type &reference;
	};

	template<class T>
	struct iterator_traits<T *>
	{
		typedef random_access_iterator_tag iterator_category;
		typedef T value_type;
		typedef ptrdiff_t difference_type;
		typedef ptrdiff_t distance_type;
		typedef T *pointer;
		typedef T &reference;
	};

	template<class T>
	struct iterator_traits<T const *>
	{
		typedef random_access_iterator_tag iterator_category;
		typedef T value_type;
		typedef ptrdiff_t difference_type;
		typedef ptrdiff_t distance_type;
		typedef T const *pointer;
		typedef T const &reference;
	};

	template<class C>
	struct iterator_traits<insert_iterator<C> >
	{
		typedef output_iterator_tag iterator_category;
		typedef typename C::value_type value_type;
		typedef void difference_type;
		typedef void distance_type;
		typedef void pointer;
		typedef void reference;
	};

	template<class It> inline typename iterator_traits<It>::iterator_category __cdecl _Iter_cat(It const &) { return typename iterator_traits<It>::iterator_category(); }
#if defined(_MSC_VER) && _MSC_VER >= 1800
	template<class It> using _Iter_value_t = typename iterator_traits<It>::value_type;
	template<class It> using _Iter_diff_t = typename iterator_traits<It>::difference_type;
	template<class It> using _Iter_cat_t = typename iterator_traits<It>::iterator_category;
#endif
}

namespace boost
{
	namespace iterators
	{
		namespace detail
		{
			template<class C, class T, class D, class P, class R>
			struct iterator : public std::iterator<C, T, D, P, R> { };
		}
	}

	namespace re_detail_106700
	{
		template<class It, class OutIt>
		OutIt plain_copy(It begin, It end, OutIt out) { while (begin != end) { *out = *begin; ++begin; } return out; }
		struct re_syntax_base;
		template <class Results>
		struct recursion_info;
		template<class T>
		inline recursion_info<T> *copy(recursion_info<T> *begin, recursion_info<T> *end, recursion_info<T> *out) { return plain_copy<recursion_info<T> *, recursion_info<T> *>(begin, end, out); }
		inline std::pair<bool, re_syntax_base *> *copy(std::pair<bool, re_syntax_base *> *begin, std::pair<bool, re_syntax_base *> *end, std::pair<bool, re_syntax_base *> *out) { return plain_copy<std::pair<bool, re_syntax_base *> *, std::pair<bool, re_syntax_base *> *>(begin, end, out); }
	}
}

#ifdef  _EXCEPTION_
#error include <exception> already happened
#endif
#include <exception>  // must be included before <xstring> to avoid renaming clash
namespace std
{
	class exception_ptr;
	__declspec(noreturn) void __cdecl terminate() throw();
}
#ifndef _STD_TERMINATE_DEFINED
#define _STD_TERMINATE_DEFINED
__declspec(noreturn) inline void __cdecl __std_terminate() throw() { terminate(); }
#endif

// std::allocator::rebind doesn't exist!!
#define allocator allocator_bad
#ifdef  _MEMORY_
#error include <memory> already happened
#endif
#include <memory>
#undef allocator
namespace std
{
	template<class T>
	class allocator : public allocator_bad<T>
	{
		typedef allocator_bad<T> Base;
	public:
		template<class U> struct rebind { typedef allocator<U> other; };
		allocator() { }
		allocator(Base const &v) : Base(v) { }
		template<class Other>
		allocator(allocator_bad<Other> const &) : Base() { }
		typename Base::pointer allocate(typename Base::size_type n, void const *hint = NULL)
		{ return this->Base::allocate(n, hint); }
#ifdef X_HAS_MOVE_SEMANTICS
		// using Base::construct;
		// void construct(typename Base::pointer const p, T &&value) { new(p) T(static_cast<T &&>(value)); }
#endif
	};

	template<class T>
	class allocator<T const> : public allocator<T>
	{
	};

	template<class Ax>
	struct allocator_traits
	{
		typedef typename Ax::size_type size_type;
	};
}

// The above re-implementation of std::allocator messes up some warnings...
#pragma warning(push)
#pragma warning(disable: 4251)  // class 'type' needs to have dll-interface to be used by clients of class 'type2'
#pragma warning(disable: 4512)  // assignment operator could not be generated
	namespace std
	{
		template<class RanIt, class T, class R, class P, class D>
		class reverse_iterator : public iterator<typename iterator_traits<RanIt>::iterator_category, T, D, P, R>
		{
		public:
			typedef reverse_iterator<RanIt> This;
			typedef typename iterator_traits<RanIt>::difference_type difference_type;
			typedef typename iterator_traits<RanIt>::pointer pointer;
			typedef typename iterator_traits<RanIt>::reference reference;
			typedef RanIt iterator_type;
			reverse_iterator() { }
			explicit reverse_iterator(RanIt right) : current(right) { }
			template<class Other>
			reverse_iterator(const reverse_iterator<Other>& right) : current(right.base()) { }
			RanIt base() const { return (current); }
			reference operator*() const { RanIt tmp = current; return (*--tmp); }
			pointer operator->() const { return (&**this); }
			This& operator++() { --current; return (*this); }
			This operator++(int) { This tmp = *this; --current; return (tmp); }
			This& operator--() { ++current; return (*this); }
			This operator--(int) { This tmp = *this; ++current; return (tmp); }
			bool operator ==(const reverse_iterator& right) const
			{ return (current == right.base()); }
			bool operator !=(const reverse_iterator& right) const
			{ return (current != right.base()); }
			This& operator+=(difference_type offset) { current -= offset; return (*this); }
			This operator+(difference_type offset) const { return (This(current - offset)); }
			This& operator-=(difference_type offset) { current += offset; return (*this); }
			This operator-(difference_type offset) const { return (This(current + offset)); }
			reference operator[](difference_type offset) const { return (*(*this + offset)); }
			bool operator <(const reverse_iterator& right) const { return (right.base() < current); }
			bool operator >(const reverse_iterator& right) const { return (right.base() > current); }
			bool operator <=(const reverse_iterator& right) const { return (right.base() <= current); }
			bool operator >=(const reverse_iterator& right) const { return (right.base() >= current); }
			difference_type operator -(const reverse_iterator& right) const { return (right.base() - current); }
		protected:
			RanIt current;
		};
		template<class RanIt, class Diff> inline reverse_iterator<RanIt> operator+(Diff offset, const reverse_iterator<RanIt>& right) { return (right + offset); }
		//template<class RanIt1, class RanIt2> inline typename reverse_iterator<RanIt1>::difference_type operator-(const reverse_iterator<RanIt1>& left, const reverse_iterator<RanIt2>& right) { return (left - right); }
		//template<class RanIt1, class RanIt2> inline bool operator==(const reverse_iterator<RanIt1>& left, const reverse_iterator<RanIt2>& right) { return (left == right); }
		//template<class RanIt1, class RanIt2> inline bool operator!=(const reverse_iterator<RanIt1>& left, const reverse_iterator<RanIt2>& right) { return (!(left.operator ==(right))); }
		//template<class RanIt1, class RanIt2> inline bool operator<(const reverse_iterator<RanIt1>& left, const reverse_iterator<RanIt2>& right) { return (left < right); }
		//template<class RanIt1, class RanIt2> inline bool operator>(const reverse_iterator<RanIt1>& left, const reverse_iterator<RanIt2>& right) { return (right < left); }
		//template<class RanIt1, class RanIt2> inline bool operator<=(const reverse_iterator<RanIt1>& left, const reverse_iterator<RanIt2>& right) { return (!(right < left)); }
		//template<class RanIt1, class RanIt2> inline bool operator>=(const reverse_iterator<RanIt1>& left, const reverse_iterator<RanIt2>& right) { return (!(left < right)); }
	}

#define wstring wstring_bad
#define string string_bad
#ifdef _XSTRING_
#error #include <xstring> already happened
#endif
#pragma push_macro("_DLL")
#undef _DLL
#include <xstring>
#pragma pop_macro("_DLL")
#ifdef _STDEXCEPT_
#error #include <stdexcept> already happened
#endif
#include <stdexcept>  // implicitly #include <xstring>, because we want to get the references to string/wstring out of the way
#define xdigit  blank = space, xdigit  // this is for when #include <xlocale> occurs
#pragma push_macro("_DLL")
#undef _DLL
#ifdef _XLOCALE_
#error include <xlocale> already happened
#endif
#include <xlocale>  // this is included by <istream>, and we DO want our changing of _DLL to affect it! to prevent ctype<> from being imported
#pragma pop_macro("_DLL")
#ifdef _IOS_
#error #include <ios> already happened
#endif
#include <ios>  // this is included by <ostream>, but we don't want to affect it
#ifdef _OSTREAM_
#error #include <ostream> already happened
#endif
#define unitbuf ios::unitbuf
#include <ostream>  // this is included by <istream>, but we don't want to affect it
#undef  unitbuf
#ifdef _ISTREAM_
#error #include <istream> already happened
#endif
#define skipws ios::skipws
#include <istream>  // this is included by <string>, but we don't want our changing of _DLL to affect it
#undef  skipws
#pragma push_macro("_DLL")
#undef _DLL
#ifdef _STRING_
#error #include <string> already happened
#endif
#include <string>
#pragma pop_macro("_DLL")
#include <algorithm>  // for rotate()
#undef  xdigit
	namespace std
	{
		template<class Traits>
		struct basic_string_good_detail
		{
			typedef basic_string_good_detail H;
			template<class C> static typename C::iterator begin(C &s) { return s.begin(); }
			template<class C> static typename C::const_iterator begin(C const &s) { return s.begin(); }
			template<size_t N> static typename Traits::char_type const *begin(typename Traits::char_type const (&s)[N]) { return s; }
			template<size_t N> static typename Traits::char_type *begin(typename Traits::char_type (&s)[N]) { return s; }
			static typename Traits::char_type const *begin(typename Traits::char_type const *const s) { return s; }
			static typename Traits::char_type *begin(typename Traits::char_type *const s) { return s; }
			template<class C> static typename C::iterator end(C &s) { return s.end(); }
			template<class C> static typename C::const_iterator end(C const &s) { return s.end(); }
			template<size_t N> static typename Traits::char_type const *end(typename Traits::char_type const (&s)[N]) { return s + static_cast<ptrdiff_t>(N); }
			template<size_t N> static typename Traits::char_type *end(typename Traits::char_type (&s)[N]) { return s + static_cast<ptrdiff_t>(N); }
			static typename Traits::char_type const *end(typename Traits::char_type const *const s) { return s + static_cast<ptrdiff_t>(s ? Traits::length(s) : 0); }
			static typename Traits::char_type *end(typename Traits::char_type *const s) { return s + static_cast<ptrdiff_t>(s ? Traits::length(s) : 0); }
			template<class C> static typename C::size_type size(C const &s) { return s.size(); }
			template<size_t N> static size_t size(typename Traits::char_type const (&)[N]) { return N; }
			template<size_t N> static size_t size(typename Traits::char_type (&)[N]) { return N; }
			static size_t size(typename Traits::char_type const *const s) { return s ? Traits::length(s) : 0; }
			static size_t size(typename Traits::char_type *const s) { return s ? Traits::length(s) : 0; }
			template<class It1, class It2>
			static int compare(It1 const &i1, It1 const &j1, It2 const &i2, It2 const &j2)
			{
				using std::distance;
				ptrdiff_t const a = j1 - i1, b = j2 - i2;
				bool contig1 = true, contig2 = true;
				for (It1 k1 = i1; k1 != j1; ++k1) { contig1 = contig1 && &*k1 == &*i1 + (k1 - i1); }
				for (It2 k2 = i2; k2 != j2; ++k2) { contig2 = contig2 && &*k2 == &*i2 + (k2 - i2); }
				int c;
				if (contig1 && contig2)
				{
					c = Traits::compare(a > 0 ? &*i1 : NULL, b > 0 ? &*i2 : NULL, a < b ? a : b);
				}
				else
				{
					c = 0;
					It1 k1 = i1;
					It2 k2 = i2;
					for (ptrdiff_t n = a < b ? a : b; c == 0 && n > 0; --n)
					{
						// TODO: This breaks for non-char-wise comparisons
						c = Traits::compare(&*k1, &*k2, 1);
					}
				}
				if (c == 0) { c = a < b ? -1 : (a > b ? +1 : c); }
				return c;
			}
			template<class C1, class C2>
			static int compare(C1 const &a, C2 const &b) { return compare(H::begin(a), H::end(a), H::begin(b), H::end(b)); }
		};
		using ::strchr;
		template<class T, class Ax = allocator<T> >
		class vector;
		template<class Char, class Traits = char_traits<Char>, class Ax = allocator<Char> >
		class basic_string_good : public basic_string<Char, Traits, Ax>
		{
			typedef basic_string_good_detail<Traits> H;
			typedef basic_string_good this_type;
			typedef basic_string<Char, Traits, Ax> base_type;
		public:
			basic_string_good() : base_type() { }
			basic_string_good(typename base_type::size_type const count, typename base_type::value_type const value, Ax const &ax = Ax()) : base_type(count, value, ax) { }
			basic_string_good(typename base_type::const_pointer const begin, typename base_type::const_pointer const end, Ax const &ax = Ax()) : base_type(begin, end, ax) { }
			basic_string_good(typename base_type::const_pointer const s, typename base_type::size_type const count, Ax const &ax = Ax()) : base_type(s, count, ax) { }
			basic_string_good(typename base_type::const_pointer const s, Ax const &ax = Ax()) : base_type(s, ax) { }
			basic_string_good(base_type const &right, typename base_type::size_type const off, typename base_type::size_type const count = base_type::npos, Ax const &ax = Ax()) : base_type(right, off, count, ax) { }
			basic_string_good(base_type base) : base_type() { base.swap(static_cast<base_type &>(*this)); }
			basic_string_good(vector<Char, Ax> const &base) : base_type() { this->insert(this->end(), base.begin(), base.end()); }
			// basic_string_good(basic_string_view<Char, Traits> const &other, Ax const &ax = Ax()) : base_type(other.begin(), other.end(), ax) { }
			using base_type::append;
			// this_type &append(basic_string_view<Char, Traits> const &other) { return static_cast<this_type &>(this->base_type::append(other.begin(), other.end())); }
			typename base_type::reference back() { return --*this->base_type::end(); }
			typename base_type::const_reference back() const { return --*this->base_type::end(); }
			typename base_type::reference front() { return *this->base_type::begin(); }
			typename base_type::const_reference front() const { return *this->base_type::begin(); }
			using base_type::data;
			typename base_type::pointer data() { return this->empty() ? NULL : &*this->begin(); }
#if 0
			using base_type::insert;
			this_type &insert(size_type const index, value_type const *const s, size_type const count)
			{
				this->base_type::append(s, count);
				rotate(this->base_type::begin() + static_cast<ptrdiff_t>(index), this->base_type::end() - static_cast<ptrdiff_t>(count), this->base_type::end());
				return *this;
			}
			this_type &insert(size_type const index, value_type const *const s)
			{
				size_type const n = this->base_type::size();
				this->base_type::append(s);
				rotate(this->base_type::begin() + static_cast<ptrdiff_t>(index), this->base_type::begin() + static_cast<ptrdiff_t>(n), this->base_type::end());
				return *this;
			}
			this_type &insert(size_type const index, size_type const count, value_type const &ch)
			{
				this->base_type::append(count, ch);
				rotate(this->base_type::begin() + static_cast<ptrdiff_t>(index), this->base_type::end() - static_cast<ptrdiff_t>(count), this->base_type::end());
				return *this;
			}
#endif
			void pop_back() { this->base_type::erase(this->base_type::end() - 1); }
			void push_back(typename base_type::value_type const &value) { this->base_type::append(static_cast<typename base_type::size_type>(1), value); }
			void clear() { this->base_type::erase(0, this->base_type::size()); }
			this_type operator +(typename base_type::value_type const &c) const { this_type s(*this); s.insert(s.end(), c); return s; }
			this_type operator +(typename base_type::const_pointer const &other) const { this_type s(other); s.insert(s.begin(), this->base_type::begin(), this->base_type::end()); return s; }
			this_type operator +(base_type const &other) const { this_type s(other); s.insert(s.begin(), this->base_type::begin(), this->base_type::end()); return s; }
			void swap(this_type &other) { this->base_type::swap(static_cast<base_type &>(other)); }
			friend void swap(this_type &a, this_type &b) { using std::swap; swap(static_cast<base_type &>(a), static_cast<base_type &>(b)); }
			template<class Other> bool operator==(Other const &other) const { return H::compare(*this, other) == 0; }
			template<class Other> bool operator!=(Other const &other) const { return H::compare(*this, other) != 0; }
			template<class Other> bool operator< (Other const &other) const { return H::compare(*this, other) <  0; }
			template<class Other> bool operator> (Other const &other) const { return H::compare(*this, other) >  0; }
			template<class Other> bool operator<=(Other const &other) const { return H::compare(*this, other) <= 0; }
			template<class Other> bool operator>=(Other const &other) const { return H::compare(*this, other) >= 0; }
			friend bool operator==(typename base_type::const_pointer const left, this_type const &right) { return H::compare(left, right) == 0; }
			friend bool operator!=(typename base_type::const_pointer const left, this_type const &right) { return H::compare(left, right) != 0; }
			friend bool operator< (typename base_type::const_pointer const left, this_type const &right) { return H::compare(left, right) <  0; }
			friend bool operator> (typename base_type::const_pointer const left, this_type const &right) { return H::compare(left, right) >  0; }
			friend bool operator<=(typename base_type::const_pointer const left, this_type const &right) { return H::compare(left, right) <= 0; }
			friend bool operator>=(typename base_type::const_pointer const left, this_type const &right) { return H::compare(left, right) >= 0; }
		};
		template<class Traits, class Alloc> float stof(basic_string<char, Traits, Alloc> const &str) { return atof(static_cast<char const *>(str.c_str())); }
		template<class Traits, class Alloc> float stof(basic_string<wchar_t, Traits, Alloc> const &str) { return _wtof(static_cast<wchar_t const *>(str.c_str())); }
		template<class Traits, class Alloc> int stoi(basic_string<char, Traits, Alloc> const &str) { return atoi(static_cast<char const *>(str.c_str())); }
		template<class Traits, class Alloc> int stoi(basic_string<wchar_t, Traits, Alloc> const &str) { return _wtoi(static_cast<wchar_t const *>(str.c_str())); }
		template<class Traits, class Alloc> long stol(basic_string<char, Traits, Alloc> const &str) { return atol(static_cast<char const *>(str.c_str())); }
		template<class Traits, class Alloc> long stol(basic_string<wchar_t, Traits, Alloc> const &str) { return _wtol(static_cast<wchar_t const *>(str.c_str())); }
		template<class Traits, class Alloc> long long stoll(basic_string<char, Traits, Alloc> const &str) { return _atoi64(static_cast<char const *>(str.c_str())); }
		template<class Traits, class Alloc> long long stoll(basic_string<wchar_t, Traits, Alloc> const &str) { return _wtoi64(static_cast<wchar_t const *>(str.c_str())); }
		template<class T> basic_string_good<char> to_string(T const &);
		template<> inline basic_string_good<char> to_string<int>(int const &value) { char buf[32]; buf[0] = '\0'; return _itoa(value, buf, 10); }
		template<> inline basic_string_good<char> to_string<unsigned int>(unsigned int const &value) { char buf[32]; buf[0] = '\0'; return _ultoa(value, buf, 10); }
		template<> inline basic_string_good<char> to_string<long>(long const &value) { char buf[32]; buf[0] = '\0'; return _ltoa(value, buf, 10); }
		template<> inline basic_string_good<char> to_string<unsigned long>(unsigned long const &value) { char buf[32]; buf[0] = '\0'; return _ultoa(value, buf, 10); }
		template<> inline basic_string_good<char> to_string<long long>(long long const &value) { char buf[32]; buf[0] = '\0'; return _i64toa(value, buf, 10); }
		template<> inline basic_string_good<char> to_string<unsigned long long>(unsigned long long const &value) { char buf[32]; buf[0] = '\0'; return _ui64toa(value, buf, 10); }
	}

	// Fixes for 'vector' -- boost::ptr_vector chokes on the old implementation!
#define vector vector_bad
#define copy(It1, It2, OutIt) std::copy(It1, It2, OutIt)
#ifdef _VECTOR_
#error #include <vector> already happened
#endif
#include <vector>
#undef  copy
#undef  vector
	namespace std
	{
		template<class T, class Ax>
		class vector : public vector_bad<T, Ax>
		{
			typedef vector this_type;
			typedef vector_bad<T, Ax> base_type;
		public:
			typedef typename base_type::value_type value_type;
			typedef typename base_type::size_type size_type;
			typedef typename base_type::allocator_type allocator_type;
			typedef typename base_type::allocator_type::      pointer       pointer;
			typedef typename base_type::allocator_type::const_pointer const_pointer;
			typedef typename base_type::      iterator       iterator;
			typedef typename base_type::const_iterator const_iterator;
			typedef typename base_type::      reverse_iterator       reverse_iterator;
			typedef typename base_type::const_reverse_iterator const_reverse_iterator;
			vector() : base_type() { }
			explicit vector(size_type const count) : base_type(count) { }
			explicit vector(Ax const &ax) : base_type(ax) { }
			vector(size_type const count, Ax const &ax) : base_type(count, ax) { }
			vector(size_type const count, value_type const &value, Ax const &ax = Ax()) : base_type(count, value, ax) { }
			vector(const_pointer const begin, const_pointer const end, Ax const &ax = Ax()) : base_type(begin, end, ax) { }
			vector(this_type const &base) : base_type(static_cast<base_type const &>(base)) { }
			this_type &operator =(this_type const &other) { return static_cast<this_type &>(this->base_type::operator =(static_cast<base_type const &>(other))); }
#ifdef X_HAS_MOVE_SEMANTICS
			vector(this_type &&other) : base_type() { static_cast<base_type &>(other).swap(static_cast<base_type &>(*this)); other.clear(); }
			this_type &operator =(this_type &&other) { return (this_type(static_cast<this_type &&>(other)).swap(*this), *this); }
#endif
			const_iterator cbegin() const { return this->begin(); }
			const_iterator cend() const { return this->end(); }
			pointer data() { return this->empty() ? NULL : &*this->begin(); }
			const_pointer data() const { return this->empty() ? NULL : &*this->begin(); }
			void emplace_back() { this->push_back(value_type()); }
			template<class T1>
			void emplace_back(T1 const &arg1) { this->push_back(value_type(arg1)); }
			template<class T1, class T2>
			void emplace_back(T1 const &arg1, T2 const &arg2) { this->push_back(value_type(arg1, arg2)); }
			void shrink_to_fit() { vector(*this).swap(*this); }
			friend void swap(this_type &a, this_type &b) { using std::swap; swap(static_cast<base_type &>(a), static_cast<base_type &>(b)); }
		};
		struct _PVOID
		{
			void *p;
			_PVOID(void *const &ptr = 0) : p(ptr) { }
			template<class T> operator T *&() { return reinterpret_cast<T *&>(p); }
			template<class T> operator T *const &() const { return reinterpret_cast<T *const &>(p); }
		};
	}
	template<>
	class std::vector<void *, std::allocator<void *> > : public std::vector<void *, std::allocator<_PVOID> >
	{
	public:
		using std::vector<void *, std::allocator<_PVOID> >::insert;
		template<class It>
		void insert(iterator it, It begin, It end) { std::copy(begin, end, std::inserter(*this, it)); }
	};

#if 0
	template<>
	struct std::iterator_traits<std::vector<_Bool,_Bool_allocator>::_It>
	{
		typedef std::random_access_iterator_tag iterator_category;
		typedef unsigned int value_type;
		typedef ptrdiff_t difference_type;
		typedef ptrdiff_t distance_type;
		typedef unsigned int *pointer;
		typedef unsigned int &reference;
	};
#endif

#pragma warning(push)
#pragma warning(disable: 4100)  // unreferenced formal parameter
#ifdef  _LOCALE_
#error include <locale> already happened
#endif
#include <locale>
	using std::codecvt;
#pragma warning(pop)

#ifdef __clang__
namespace std
{
	static ios_base::openmode const in = ios_base::in, out = ios_base::out, trunc = ios_base::trunc;
}
#endif

#ifdef  _SSTREAM_
#error include <sstream> already happened
#endif
#include <sstream>  // get rid of warnings
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4127)  // conditional expression is constant
#pragma warning(disable: 5038)  // data member '...' will be initialized after base class '...'
#ifdef  _FSTREAM_
#error include <fstream> already happened
#endif
#include <fstream>
#pragma warning(pop)

#ifdef  _FUNCTIONAL_
#error include <functional> already happened
#endif
#include <functional>

#pragma warning(push)
#pragma warning(disable: 4512)  // assignment operator could not be generated
#define set set_bad
#define multiset multiset_bad
#ifdef  _SET_
#error include <set> already happened
#endif
#include <set>
#undef  multiset
#undef  set
namespace std
{
	template<class K, class Pr = less<K>, class Ax = allocator<K> >
	class set : public set_bad<K, Pr, Ax>
	{
		typedef set this_type;
		typedef set_bad<K, Pr, Ax> base_type;
	public:
		explicit set(Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(pred, ax) { }
		set(typename base_type::const_iterator const &first, typename base_type::const_iterator const &last, Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(first, last, pred, ax) { }
		template<class It> set(It const &first, It const &last, Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(pred, ax) { this->insert<It>(first, last); }
		using base_type::insert;
		template<class It> void insert(It const &first, It const &last) { for (It i = first; i != last; ++i) { this->base_type::insert(*i); } }
	};
	template<class K, class Pr = less<K>, class Ax = allocator<K> >
	class multiset : public multiset_bad<K, Pr, Ax>
	{
		typedef multiset this_type;
		typedef multiset_bad<K, Pr, Ax> base_type;
	public:
		explicit multiset(Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(pred, ax) { }
		multiset(typename base_type::const_iterator const &first, typename base_type::const_iterator const &last, Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(first, last, pred, ax) { }
		template<class It> multiset(It const &first, It const &last, Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(pred, ax) { this->insert<It>(first, last); }
		using base_type::insert;
		template<class It> void insert(It const &first, It const &last) { for (It i = first; i != last; ++i) { this->base_type::insert(*i); } }
	};
}
#pragma warning(pop)

#pragma warning(push)
#define map map_bad
#define multimap multimap_bad
#ifdef  _MAP_
#error include <map> already happened
#endif
#include <map>
#undef  multimap
#undef  map
namespace std
{
	template<class K, class V, class Pr = less<K>, class Ax = allocator<V> >
	class map : public map_bad<K, V, Pr, Ax>
	{
		typedef map this_type;
		typedef map_bad<K, V, Pr, Ax> base_type;
	public:
		explicit map(Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(pred, ax) { }
		map(typename base_type::const_iterator const &first, typename base_type::const_iterator const &last, Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(first, last, pred, ax) { }
		template<class It> map(It const &first, It const &last, Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(pred, ax) { this->insert<It>(first, last); }
		using base_type::insert;
		template<class It> void insert(It const &first, It const &last) { for (It i = first; i != last; ++i) { this->base_type::insert(*i); } }
	};
	template<class K, class V, class Pr = less<K>, class Ax = allocator<V> >
	class multimap : public multimap_bad<K, V, Pr, Ax>
	{
		typedef multimap this_type;
		typedef multimap_bad<K, V, Pr, Ax> base_type;
	public:
		explicit multimap(Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(pred, ax) { }
		multimap(typename base_type::const_iterator const &first, typename base_type::const_iterator const &last, Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(first, last, pred, ax) { }
		template<class It> multimap(It const &first, It const &last, Pr const &pred = Pr(), Ax const &ax = Ax()) : base_type(pred, ax) { this->insert<It>(first, last); }
		using base_type::insert;
		template<class It> void insert(It const &first, It const &last) { for (It i = first; i != last; ++i) { this->base_type::insert(*i); } }
	};
}
#pragma warning(pop)

namespace std
{
	template<class> struct hash;
#ifdef _M_X64
	template<> struct hash<size_t> { size_t operator()(size_t const value) const { return value; } };
	template<> struct hash<unsigned int> { size_t operator()(unsigned int const value) const { return value; } };
#else
	template<> struct hash<unsigned long long> { size_t operator()(unsigned long long const value) const { return value ^ (value >> (CHAR_BIT * sizeof(unsigned int))); } };
	template<> struct hash<size_t> { size_t operator()(size_t const value) const { return value; } };
#endif
}

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#ifdef  _VALARRAY_
#error include <valarray> already happened
#endif
#include <valarray>
#ifndef _CPPLIB_VER
namespace std
{
	template<class T> inline T const &min(T const &a, T const &b) { return b < a ? b : a; }
	template<class T> inline T const &max(T const &a, T const &b) { return b < a ? a : b; }
#define __MTA__
}
#endif
#pragma pop_macro("max")
#pragma pop_macro("min")


namespace std
{
	template<class T>
	class unique_ptr  // actually a reference-counted pointer here, to allow it to be copyable and thus storable in std::vector.
	{
		typedef unique_ptr this_type;
		void this_type_does_not_support_comparisons() const { }
	protected:
		typedef void (this_type::*bool_type)() const;
	public:
		T *p;
		ptrdiff_t *refcount;
	public:
		~unique_ptr()
		{
			if (T *const ptr = this->p)
			{
				--*this->refcount;
				if (!*this->refcount)
				{
					delete this->refcount;
					delete ptr;
				}
				this->refcount = NULL;
				this->p = NULL;
			}
		}
		unique_ptr() : p(), refcount() { }
		explicit unique_ptr(T *const p) : p(p), refcount() { if (this->p) { if (!this->refcount) { this->refcount = new ptrdiff_t(); } ++*this->refcount; } }
		unique_ptr(this_type const &other) : p(other.p), refcount(other.refcount) { if (this->p) { ++*this->refcount; } }
#ifdef X_HAS_MOVE_SEMANTICS
		unique_ptr(this_type &&other) : p(other.p), refcount(other.refcount) { other.p = NULL; other.refcount = NULL; }
		template<class U> unique_ptr(unique_ptr<U> &&other) : p(other.p), refcount(other.refcount) { other.p = NULL; other.refcount = NULL; }
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1700
		unique_ptr(nullptr_t const &) : p(), refcount() { }
#else
		unique_ptr(void const volatile *const &) : p(), refcount() { }
#endif
		typedef T element_type;
		void swap(this_type &other) { using std::swap; swap(this->p, other.p); swap(this->refcount, other.refcount); }
		friend void swap(this_type &a, this_type &b) { return a.swap(b); }
		element_type *get() const { return this->p; }
		element_type &operator *() const { return *this->p; }
		element_type *operator->() const { return &**this; }
		void reset(element_type *const other = NULL) { this_type(other).swap(*this); }
		this_type &operator =(this_type other) { return (other.swap(*this), *this); }
		bool operator==(element_type *const other) const { return this->p == other; }
		bool operator!=(element_type *const other) const { return this->p != other; }
		bool operator<=(element_type *const other) const { return !std::less<element_type *>()(other, this->p); }
		bool operator>=(element_type *const other) const { return !std::less<element_type *>()(this->p, other); }
		bool operator< (element_type *const other) const { return std::less<element_type *>()(this->p, other); }
		bool operator> (element_type *const other) const { return std::less<element_type *>()(other, this->p); }
		bool operator==(this_type const &other) const { return *this == other.p; }
		bool operator!=(this_type const &other) const { return *this != other.p; }
		bool operator<=(this_type const &other) const { return *this <= other.p; }
		bool operator>=(this_type const &other) const { return *this >= other.p; }
		bool operator< (this_type const &other) const { return *this <  other.p; }
		bool operator> (this_type const &other) const { return *this >  other.p; }
		operator bool_type() const { return this->p ? &this_type::this_type_does_not_support_comparisons : NULL; }
	};
	template<class T>
	class unique_ptr<T[]> : public unique_ptr<T>
	{
		typedef unique_ptr this_type;
		typedef unique_ptr<T> base_type;
	public:
		~unique_ptr()
		{
			if (T *const other = this->p)
			{
				--*this->refcount;
				if (!*this->refcount)
				{
					delete this->refcount;
					delete [] other;
				}
				this->refcount = NULL;
				this->p = NULL;
			}
		}
		unique_ptr() : base_type() { }
		explicit unique_ptr(typename base_type::element_type *const other) : base_type(other) { }
		unique_ptr(this_type const &other) : base_type(static_cast<base_type const &>(other)) { }
#ifdef X_HAS_MOVE_SEMANTICS
		unique_ptr(this_type &&other) : base_type(static_cast<base_type &&>(other)) { }
		template<class U> unique_ptr(unique_ptr<U[]> &&other) : base_type(static_cast<unique_ptr<U> &&>(other)) { }
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1700
		unique_ptr(nullptr_t const &other) : base_type(other) { }
#else
		unique_ptr(void const volatile *const &other) : base_type(other) { }
#endif
		this_type &operator =(this_type other) { return (other.swap(*this), *this); }
		typename base_type::element_type &operator[](size_t const i) const { return this->p[i]; }
		void reset(typename base_type::element_type *const other = NULL) { this_type(other).swap(*this); }
	};
	template<class T>
	struct unique_maker
	{
		static unique_ptr<T> make_unique() { return unique_ptr<T>(new T()); }
		template<class T1> static unique_ptr<T> make_unique(T1 const &arg1) { return unique_ptr<T>(new T(arg1)); }
		template<class T1, class T2> static unique_ptr<T> make_unique(T1 const &arg1, T2 const &arg2) { return unique_ptr<T>(new T(arg1, arg2)); }
		template<class T1, class T2, class T3> static unique_ptr<T> make_unique(T1 const &arg1, T2 const &arg2, T3 const &arg3) { return unique_ptr<T>(new T(arg1, arg2, arg3)); }
		template<class T1, class T2, class T3, class T4> static unique_ptr<T> make_unique(T1 const &arg1, T2 const &arg2, T3 const &arg3, T4 const &arg4) { return unique_ptr<T>(new T(arg1, arg2, arg3, arg4)); }
	};
	template<class T>
	struct unique_maker<T[]>
	{
		template<class T1>
		static unique_ptr<T[]> make_unique(T1 const &size) { return unique_ptr<T[]>(new T[size]()); }
	};
	template<class T> unique_ptr<T> make_unique() { return unique_maker<T>::make_unique(); }
	template<class T, class T1> unique_ptr<T> make_unique(T1 const &arg1) { return unique_maker<T>::make_unique<T1>(arg1); }
	template<class T, class T1, class T2> unique_ptr<T> make_unique(T1 const &arg1, T2 const &arg2) { return unique_maker<T>::make_unique<T1, T2>(arg1, arg2); }
	template<class T, class T1, class T2, class T3> unique_ptr<T> make_unique(T1 const &arg1, T2 const &arg2, T3 const &arg3) { return unique_maker<T>::make_unique<T1, T2, T3>(arg1, arg2, arg3); }
	template<class T, class T1, class T2, class T3, class T4> unique_ptr<T> make_unique(T1 const &arg1, T2 const &arg2, T3 const &arg3, T4 const &arg4) { return unique_maker<T>::make_unique<T1, T2, T3, T4>(arg1, arg2, arg3, arg4); }

}

namespace std
{
	template<class T> typename T::iterator begin(T &value) { return value.begin(); }
	template<class T> typename T::const_iterator begin(T const &value) { return value.begin(); }
	template<class T, size_t N> T *begin(T (&value)[N]) { return &value[0]; }
	template<class T, size_t N> T const *begin(T const (&value)[N]) { return &value[0]; }
	template<class T> typename T::iterator end(T &value) { return value.end(); }
	template<class T> typename T::const_iterator end(T const &value) { return value.end(); }
	template<class T, size_t N> T *end(T (&value)[N]) { return &value[N]; }
	template<class T, size_t N> T const *end(T const (&value)[N]) { return &value[N]; }

#ifdef X_HAS_MOVE_SEMANTICS
	template<class InputIt, class OutputIt>
	OutputIt move(InputIt first, InputIt last, OutputIt d_first) { while (first != last) { *d_first++ = move(*first++); } return d_first; }
	template<class BidirIt1, class BidirIt2>
	BidirIt2 move_backward(BidirIt1 first, BidirIt1 last, BidirIt2 d_last) { while (first != last) { *(--d_last) = move(*(--last)); } return d_last; }
#endif
	template<class InputIt, class UnaryPredicate>
	InputIt find_if_not(InputIt first, InputIt last, UnaryPredicate q) { return find_if(first, last, not1(q)); }
	template<class InputIt, class UnaryPredicate>
	bool all_of(InputIt first, InputIt last, UnaryPredicate p) { return find_if_not(first, last, p) == last; }
	template<class InputIt, class UnaryPredicate>
	bool any_of(InputIt first, InputIt last, UnaryPredicate p) { return find_if(first, last, p) != last; }
	template<class InputIt, class UnaryPredicate>
	bool none_of(InputIt first, InputIt last, UnaryPredicate p) { return find_if(first, last, p) == last; }
}

namespace std
{
	template<class From, class To> struct propagate_const_from { typedef To type; };
	template<class From, class To> struct propagate_const_from<From const, To> : propagate_const_from<From, To const> { };
	template<class From, class To> struct propagate_const_from<From &, To> : propagate_const_from<From, To> { };

	template<class, class = void, class = void> struct tuple;
	template<class T1, class T2>
	struct tuple<T1, T2> : pair<T1, T2>
	{
		typedef pair<T1, T2> base_type;
		tuple() : base_type() { }
		explicit tuple(T1 const &arg1, T2 const &arg2) : base_type(arg1, arg2) { }
	};
	template<size_t I, class Tuple> struct tuple_element;
	template<class Tuple> struct tuple_element<1 - 1, Tuple> { typedef typename propagate_const_from<Tuple, typename Tuple:: first_type>::type type; static type &get(Tuple &tup) { return tup.first ; } };
	template<class Tuple> struct tuple_element<2 - 1, Tuple> { typedef typename propagate_const_from<Tuple, typename Tuple::second_type>::type type; static type &get(Tuple &tup) { return tup.second; } };
	template<class Tuple> struct tuple_element<3 - 1, Tuple> { typedef typename propagate_const_from<Tuple, typename Tuple:: third_type>::type type; static type &get(Tuple &tup) { return tup.third ; } };
	template<size_t I, class Tuple> typename tuple_element<I, Tuple>::type &get(Tuple &tup) { return tuple_element<I, Tuple>::get(tup); }
}

template<class To, class From>
inline To do_round(From value) { return static_cast<To>(value < From() ? ceil(value - static_cast<From>(0.5)) : floor(value + static_cast<From>(0.5))); }
inline float round(float value) { return do_round<float, float>(value); }
inline double round(double value) { return do_round<double, double>(value); }
inline long double round(long double value) { return do_round<long double, long double>(value); }
inline long lround(float value) { return do_round<long, float>(value); }
inline long lround(double value) { return do_round<long, double>(value); }
inline long lround(long double value) { return do_round<long, long double>(value); }
inline long long llround(float value) { return do_round<long long, float>(value); }
inline long long llround(double value) { return do_round<long long, double>(value); }
inline long long llround(long double value) { return do_round<long long, long double>(value); }
inline long long abs(long long const value) { return ::_abs64(value); }
namespace std
{
	using ::abs;
	using ::fabs;
	using ::round;
	using ::lround;
	using ::llround;
}

#include <list>
namespace std
{
	template<class T, class Ax = allocator<T> >
	class forward_list : public list<T, Ax>
	{
		typedef forward_list this_type;
		typedef list<T, Ax> base_type;
		struct before_begin_iterator { typename base_type::iterator i; };
	public:
		using base_type::remove_if;
		before_begin_iterator before_begin() { before_begin_iterator r = { this->base_type::begin() }; return r; }
		void splice_after(before_begin_iterator pos, this_type &other) { this->base_type::splice(pos.i, other); }
		template<class Pr>
		void remove_if(Pr pr) { typename base_type::iterator l = this->base_type::end(); for (typename base_type::iterator f = this->base_type::begin(); f != l; ) if (pr(*f)) { this->base_type::erase(f++); } else { ++f; } }
	};
}

namespace std
{
	template<class T>
	class initializer_list
	{
	public:
		typedef T value_type;
		typedef const T& reference;
		typedef const T& const_reference;
		typedef size_t size_type;
		typedef const T* iterator;
		typedef const T* const_iterator;
		initializer_list() _NOEXCEPT : _begin(), _end() { }
		initializer_list(T const *first, T const *last) _NOEXCEPT : _begin(first), _end(last) { }
		T const *begin() const _NOEXCEPT { return begin; }
		T const *end() const _NOEXCEPT { return _end; }
		size_t size() const _NOEXCEPT { return (size_t) (_end - _begin); }
	private:
		T const *_begin, *_end;
	};
}

namespace std
{
	_CRTIMP2_PURE inline void __CLRCALL_PURE_OR_CDECL _Xbad_alloc() { throw bad_alloc(); }
	template<class InIt1, class InIt2, class Pr>
	inline bool equal(InIt1 First1, InIt1 Last1, InIt2 First2, InIt2 Last2, Pr Pred)
	{
		for (; First1 != Last1 && First2 != Last2; ++First1, (void)++First2)
			if (!Pred(*First1, *First2))
				return false;
		return (First1 == Last1 && First2 == Last2);
	}
	template<class T> inline void _Swap_adl(T &a, T &b) { return swap(a, b); }
	struct _Container_base { };
	struct _Container_base0 { void _Orphan_all() { } void _Swap_all(_Container_base0 &) { } };
	struct _Iterator_base0 { void _Adopt(const void *) { } _Container_base0 const *_Getcont() const { return 0; } };
	typedef _Iterator_base0 _Iterator_base;
	template<class Category, class Ty, class Diff, class Pointer, class Reference, class Base>
	struct _Iterator012 : public Base
	{
		typedef Category iterator_category;
		typedef Ty value_type;
		typedef Diff difference_type;
		typedef Pointer pointer;
		typedef Reference reference;
	};
	typedef unsigned long _Uint32t;
	typedef _Uint32t _Uint4_t;
	typedef _Uint4_t _Atomic_integral_t;
	typedef _Atomic_integral_t _Atomic_counter_t;
	template<class It> inline It _Unchecked(It s) { return s; }
}
#ifdef _BITMASK_OPS
#undef _BITMASK_OPS
#endif
#define _BITMASK_OPS(T) \
	inline T operator |(T const a, T const b) { return static_cast<T>(static_cast<int>(a) | static_cast<int>(b)); } \
	inline T operator &(T const a, T const b) { return static_cast<T>(static_cast<int>(a) & static_cast<int>(b)); } \
	inline T operator ^(T const a, T const b) { return static_cast<T>(static_cast<int>(a) ^ static_cast<int>(b)); } \
	inline T operator ~(T const v) { return static_cast<T>(~static_cast<int>(v)); } \
	inline T &operator |=(T &a, T const b) { return a = (a | b); } \
	inline T &operator &=(T &a, T const b) { return a = (a & b); } \
	inline T &operator ^=(T &a, T const b) { return a = (a ^ b); }

#define _DEBUG_RANGE(first, last)
#define _SCL_SECURE_VALIDATE(cond)
#define _SCL_SECURE_VALIDATE_RANGE(cond)

#undef  X_HAS_MOVE_SEMANTICS

// This should be AFTER all standard headers are included

#undef wstring
#undef string
#define basic_string basic_string_good
namespace std
{
	typedef basic_string<char> string;
	typedef basic_string<wchar_t> wstring;
}

namespace std
{
	using ::isalpha;
	using ::isalnum;
}


#if defined __cplusplus_cli
#define EHVEC_CALEETYPE __clrcall
#else
#define EHVEC_CALEETYPE __stdcall
#endif
#if defined __cplusplus_cli
#define EHVEC_CALLTYPE __clrcall 
#elif defined _M_IX86
#define EHVEC_CALLTYPE __thiscall
#else
#define EHVEC_CALLTYPE __stdcall
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1900
void EHVEC_CALEETYPE __ArrayUnwind(
	void*       ptr,                // Pointer to array to destruct
	size_t      size,               // Size of each element (including padding)
	int         count,              // Number of elements in the array
	void(EHVEC_CALLTYPE *pDtor)(void*)    // The destructor to call
);

inline void EHVEC_CALEETYPE __ehvec_ctor(
	void*       ptr,                // Pointer to array to destruct
	size_t      size,               // Size of each element (including padding)
	//  int         count,              // Number of elements in the array
	size_t      count,              // Number of elements in the array
	void(EHVEC_CALLTYPE *pCtor)(void*),   // Constructor to call
	void(EHVEC_CALLTYPE *pDtor)(void*)    // Destructor to call should exception be thrown
) {
	size_t i = 0;      // Count of elements constructed
	int success = 0;

	__try
	{
		// Construct the elements of the array
		for (; i < count; i++)
		{
			(*pCtor)(ptr);
			ptr = (char*)ptr + size;
		}
		success = 1;
	}
	__finally
	{
		if (!success)
			__ArrayUnwind(ptr, size, (int)i, pDtor);
	}
}

inline void EHVEC_CALEETYPE __ehvec_dtor(
	void*       ptr,                // Pointer to array to destruct
	size_t      size,               // Size of each element (including padding)
	//  int         count,              // Number of elements in the array
	size_t      count,              // Number of elements in the array
	void(EHVEC_CALLTYPE *pDtor)(void*)    // The destructor to call
) {
	int success = 0;

	// Advance pointer past end of array
	ptr = (char*)ptr + size*count;

	__try
	{
		// Destruct elements
		while (count-- > 0)
		{
			ptr = (char*)ptr - size;
			(*pDtor)(ptr);
		}
		success = 1;
	}
	__finally
	{
		if (!success)
			__ArrayUnwind(ptr, size, (int)count, pDtor);
	}
}
#endif

#include <eh.h>  // for _se_translator_function
#pragma push_macro("_set_se_translator")
extern "C" __inline _se_translator_function __cdecl __set_se_translator(_se_translator_function f)
{
	_se_translator_function (__cdecl *p_set_se_translator)(_se_translator_function f) = &_set_se_translator;
	return p_set_se_translator(f);
}
#define _set_se_translator __set_se_translator
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;
typedef unsigned int UINT;
#include <ProvExce.h>
#pragma pop_macro("_set_se_translator")

#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#pragma warning(pop)

#endif
