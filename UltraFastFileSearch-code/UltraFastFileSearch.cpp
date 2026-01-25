// ============================================================================
// UltraFastFileSearch - Main Source File
// ============================================================================
// Known Issues (TODO):
// - Sort is not 100% ==> UPPER and lowercase treated differently
// - Search results are NOT sorted ... should we?
// - Check if FS needs REFRESH between searches
// - Return error if NO results
// ============================================================================

// ============================================================================
// Project Configuration
// ============================================================================
#include "targetver.h"

// ============================================================================
// Standard C Headers
// ============================================================================
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <tchar.h>
#include <time.h>
#include <wchar.h>

// ============================================================================
// Standard C++ Headers
// ============================================================================
#include <algorithm>
#include <cassert>
#include <chrono>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// Compiler-Specific Headers
// ============================================================================
#ifdef _OPENMP
#include <omp.h>
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1800
// We don't check _CPPLIB_VER here, because we want to use the <atomic>
// that came with the compiler, even if we use a different STL
#define is_trivially_copyable_v sizeof is_trivially_copyable
#include <atomic>
#undef is_trivially_copyable_v
#endif

#if defined(_CPPLIB_VER) && _CPPLIB_VER >= 610
#include <mutex>
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif
#include <mmintrin.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

// ============================================================================
// Utility Headers (extracted from monolith)
// ============================================================================
#include "src/util/atomic_compat.hpp"
#include "src/util/intrusive_ptr.hpp"
#include "src/util/lock_ptr.hpp"
#include "src/io/overlapped.hpp"

// ============================================================================
// Windows SDK Headers
// ============================================================================
#include <Windows.h>
#include <Dbt.h>
#include <muiload.h>
#include <ProvExce.h>
#include <ShlObj.h>
#include <strsafe.h>
#include <WinNLS.h>

// ============================================================================
// ATL/WTL Headers
// ============================================================================
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atldlgs.h>
#include <atlframe.h>
#include <atlmisc.h>
#include <atltheme.h>
#include <atlwin.h>

// ============================================================================
// Third-Party Headers
// ============================================================================
#include <boost/algorithm/string.hpp>

// ============================================================================
// Project Headers
// ============================================================================
#include "resource.h"
#include "BackgroundWorker.hpp"
#include "CDlgTemplate.hpp"
#include "CModifiedDialogImpl.hpp"
#include "CommandLineParser.hpp"
#include "nformat.hpp"
#include "NtUserCallHook.hpp"
#include "path.hpp"
#include "ShellItemIDList.hpp"
#include "string_matcher.hpp"
#include "src/core/ntfs_types.hpp"
#include "src/util/handle.hpp"
// Note: intrusive_ptr.hpp not included - conflicts with existing RefCounted ADL functions
#include "src/io/winnt_types.hpp"
#include "src/io/io_priority.hpp"
#include "src/util/wow64.hpp"
// Note: overlapped.hpp is documentation only - class not extracted yet
// Note: src/index/ntfs_index.hpp is documentation only - class not extracted yet
// Note: src/cli/cli_main.hpp is documentation only - entry point not extracted yet
// Note: src/gui/gui_main.hpp is documentation only - entry point not extracted yet
// Note: src/gui/main_dialog.hpp is documentation only - CMainDlg not extracted yet

namespace WTL
{
	using std::min;
	using std::max;
}

#ifndef ILIsEmpty
inline BOOL ILIsEmpty(LPCITEMIDLIST pidl) { return ((pidl == nullptr) || (pidl->mkid.cb == 0)); }
#endif

extern WTL::CAppModule _Module;

#ifdef __clang__
#pragma clang diagnostic push# pragma clang diagnostic ignored "-Wignored-attributes"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifndef ILIsEmpty
inline BOOL ILIsEmpty(LPCITEMIDLIST pidl)
{
	return ((pidl == nullptr) || (pidl->mkid.cb == 0));
}
#endif

namespace WTL
{
	using std::min;
	using std::max;
}

extern WTL::CAppModule _Module;

#ifdef __clang__

#ifndef _CPPLIB_VER

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-qualification"	// for _Fpz
	namespace std
	{
		fpos_t
			const std::_Fpz = { 0 };

	}
#pragma clang diagnostic pop

#endif

	EXTERN_C
	const GUID DECLSPEC_SELECTANY IID_IShellFolder = { 0x000214E6L, 0, 0,
		{
			0xC0, 0, 0, 0, 0, 0, 0, 0x46
		}
	};

#endif

template<class T, class Traits = std::char_traits<T>, class Ax = std::allocator<T> >
class basic_vector_based_string : public

#ifdef _DEBUG
	std::basic_string<T, Traits, Ax>
#else
	std::vector<T, Ax>
#endif

{
	typedef basic_vector_based_string this_type;
	typedef std::basic_string<T, Traits, Ax> string_type;
	typedef
#ifdef _DEBUG
	string_type
#else
	std::vector<T, Ax>
#endif
	base_type;

public:
	
	typedef typename base_type::size_type size_type;
	typedef typename base_type::allocator_type allocator_type;

	typedef typename base_type::value_type
		const* const_pointer;

	typedef typename base_type::value_type value_type;

	static size_type
		const npos = ~size_type();

	basic_vector_based_string() : base_type() {}

	explicit basic_vector_based_string(const_pointer
		const value, size_t
		const n = npos) : base_type(value, value + static_cast<ptrdiff_t> (n == npos ? Traits::length(value) : n)) {}

	explicit basic_vector_based_string(size_type
		const n) : base_type(n) {}

	explicit basic_vector_based_string(size_type
		const n, value_type
		const& value) : base_type(n, value) {}

	explicit basic_vector_based_string(allocator_type
		const& ax) : base_type(ax) {}

	explicit basic_vector_based_string(size_type
		const n, allocator_type
		const& ax) : base_type(n, ax) {}

	explicit basic_vector_based_string(size_type
		const n, value_type
		const& value, allocator_type
		const& ax) : base_type(n, value, ax) {}

	using base_type::insert;
	using base_type::operator=; 

#ifndef _DEBUG
	typedef Traits traits_type;

	typedef typename base_type::difference_type difference_type;
	typedef typename base_type::iterator iterator;
	typedef typename base_type::const_iterator const_iterator;
	
	using base_type::erase;
	
	void append(size_t
		const n, value_type
		const& value)
	{
		if (!n)
		{
			return;
		}

#if defined(_MSC_VER) && !defined(_CPPLIB_VER)

		if (this->_End - this->_Last < static_cast<ptrdiff_t> (n))
		{
			this->reserve(this->size() + n);
			for (size_t i = 0; i != n; ++i)
			{
				this->push_back(value);
			}
		}
		else
		{
			std::uninitialized_fill(this->_Last, this->_Last + static_cast<ptrdiff_t> (n), value);
			this->_Last += n;
		}

#else

		this->reserve(this->size() + n);
		for (size_t i = 0; i != n; ++i)
		{
			this->push_back(value);
		}

#endif

	}

	void append(const_pointer
		const begin, const_pointer
		const end)

	{

#if defined(_MSC_VER) && !defined(_CPPLIB_VER)

		ptrdiff_t
		const n = end - begin;
		
		if (this->_End - this->_Last < n)
		{
			this->insert(this->end(), begin, end);
		}
		else
		{
			std::uninitialized_copy(begin, end, this->_Last);
			this->_Last += n;
		}
#else

			this->insert(this->end(), begin, end); 

#endif

	}

	void append(const_pointer
		const value, size_type n = npos)
	{
		return this->append(value, value + static_cast<ptrdiff_t> (n == npos ? Traits::length(value) : n));
	}

	size_type find(value_type
		const& value, size_type
		const offset = 0) const
	{
		const_iterator begin = this->begin() + static_cast<difference_type> (offset), end = this->end();
		size_type result = static_cast<size_type> (std::find(begin, end, value) - begin);
		if (result >= static_cast<size_type> (end - begin))
		{
			result = npos;
		}
		else
		{
			result += offset;
		}

		return result;
	}

	const_pointer c_str()
	{
		const_pointer p;
		size_t
			const n = this->size();
		if (n == 0 || this->capacity() <= n || *(&*this->begin() + static_cast<ptrdiff_t> (n)) != value_type())
		{
			this->push_back(value_type());
			p = &*this->begin();
			this->pop_back();
		}
		else
		{
			p = &*this->begin();
		}

		return p;
	}

	const_pointer c_str() const
	{
		// Delegate to non-const version via const_cast
		// This is safe because the modification (push_back/pop_back) is temporary
		// and leaves the object in the same logical state
		return const_cast<this_type*>(this)->c_str();
	}

	const_pointer data() const
	{
		return this->empty() ? nullptr : &*this->begin();
	}

	iterator erase(size_t
		const pos, size_type
		const n = npos)
	{
		return this->erase(this->begin() + static_cast<difference_type> (pos), this->begin() + static_cast<difference_type> (pos) + (n == npos ? this->size() - pos : n));
	}

	iterator insert(size_t
		const pos, const_pointer
		const value, size_type
		const n = npos)
	{
		return this->insert(this->begin() + static_cast<difference_type> (pos), value, n);
	}

	this_type operator+(base_type
		const& other) const
	{
		this_type result;
		result.reserve(this->size() + other.size());
		result += *this;
		result += other;
		return result;
	}

#if defined(_MSC_VER) && !defined(_CPPLIB_VER)

	void push_back(value_type
		const& value)
	{
		if (this->_Last != this->_End)
		{
			this->allocator.construct(this->_Last, value);
			++this->_Last;
		}
		else
		{
			this->base_type::push_back(value);
		}
	}

#endif

	friend this_type operator+(const_pointer
		const left, base_type
		const& right)
	{
		size_t
				const nleft = Traits::length(left);
			this_type result;
			result.reserve(nleft + right.size());
			result.append(left, nleft);
			result += right;
			return result;
	}

	this_type& operator+=(base_type
		const& value)
	{
		if (!value.empty())
		{
			this->append(&*value.begin(), &*(value.end() - 1) + 1);
		}

		return *this;
	}

#else

	using base_type::operator+=;
	this_type& operator+=(this_type
		const& value)
	{
		if (!value.empty())
		{
			this->append(&*value.begin(), &*(value.end() - 1) + 1);
		}

		return *this;
	}

#endif

	template < class Ax2>
	friend std::basic_string<T, Traits, Ax2>& operator+=(std::basic_string<T, Traits, Ax2> &out, this_type
		const& me)
	{
		out.append(me.begin(), me.end());
		return out;
	}

	iterator insert(iterator
		const i, const_pointer
		const value, size_type
		const n = npos)
	{
		size_type
			const pos = static_cast<size_type> (i - this->begin());
		this->insert(i, value, value + static_cast<ptrdiff_t> (n == npos ? Traits::length(value) : n));
		return this->begin() + static_cast<difference_type> (pos);
	}

	this_type& operator=(const_pointer
		const value)
	{
		this->clear();
		this->append(value);
		return *this;
	}

};

template < class It, class Less>
bool is_sorted_ex(It begin, It
	const end, Less less, bool
	const reversed = false)
{
	if (begin != end)
	{
		It i(begin);
		It
			const& left = reversed ? i : begin, & right = reversed ? begin : i;
		++i;
		while (i != end)
		{
			if (less(*right, *left))
			{
				return false;
			}

			begin = i;
			++i;
		}
	}

	return true;
}

template < class ValueType, class KeyComp>
struct stable_sort_by_key_comparator : KeyComp
{
	explicit stable_sort_by_key_comparator(KeyComp
		const& comp = KeyComp()) : KeyComp(comp) {}

	typedef ValueType value_type;
	bool operator()(value_type
		const& a, value_type
		const& b) const
	{
		return this->KeyComp::operator()(a.first, b.first) || (!this->KeyComp::operator()(b.first, a.first) && (a.second < b.second));
	}
};

template < class It, class Key, class Swapper>
void stable_sort_by_key(It begin, It end, Key key, Swapper swapper)
{
	typedef typename std::iterator_traits<It>::difference_type Diff;
	typedef std::less < typename Key::result_type > KeyComp;
	typedef std::vector<std::pair < typename Key::result_type, Diff>> Keys;
	Keys keys;
	Diff
		const n = std::distance(begin, end);
	keys.reserve(static_cast<typename Keys::size_type> (n));
	{
		Diff j = 0;
		for (It i = begin; i != end; ++i)
		{
			keys.push_back(typename Keys::value_type(key(*i), j++));
		}
	}

	std::stable_sort(keys.begin(), keys.end(), stable_sort_by_key_comparator < typename Keys::value_type, KeyComp>());
	for (Diff i = 0; i != n; ++i)
	{
		for (Diff j = i;;)
		{
			using std::swap;
			swap(j, keys[j].second);
			swapper(*(begin + j), *(begin + j));
			if (j == i)
			{
				break;
			}

			using std::iter_swap;
			swapper(*(begin + j), *(begin + keys[j].second));
		}
	}
}


#include "src/util/allocators.hpp"
using uffs::DynamicAllocator;
using uffs::dynamic_allocator;
using uffs::SingleMovableGlobalAllocator;

namespace std
{
	typedef basic_string<TCHAR> tstring;
	typedef basic_vector_based_string<TCHAR, std::char_traits < TCHAR>, dynamic_allocator<TCHAR>> tvstring;
}

namespace std
{
	// Old MSVC STL internal hacks - disabled for VS 2019+ (modern STL)
#if defined(_MSC_VER) && _MSC_VER < 1920

	////POINTER ITERATOR TAGS
	//struct _General_ptr_iterator_tag
	//{	// general case, no special optimizations
	//};

	//struct _Trivially_copyable_ptr_iterator_tag
	//	: _General_ptr_iterator_tag
	//{	// iterator is a pointer to a trivially copyable type
	//};

	struct _Scalar_ptr_iterator_tag
	{
		// pointer to scalar type
	};

	template<class> struct is_scalar;

#if defined(_CPPLIB_VER) && 600 <= _CPPLIB_VER

#ifdef _XMEMORY_
	template < class T1, class T2 > struct is_scalar<std::pair<T1, T2> > : integral_constant<bool, is_pod<T1>::value&& is_pod<T2>::value > {};

	template < class T1, class T2, class _Diff, class _Valty>

	inline void _Uninit_def_fill_n(std::pair<T1, T2> * _First, _Diff _Count, _Wrap_alloc<allocator<std::pair<T1, T2> >>&, _Valty*,
#if defined(_CPPLIB_VER) && _CPPLIB_VER >= 650
		_Trivially_copyable_ptr_iterator_tag
#else
		_Scalar_ptr_iterator_tag
#endif
		)

	{
		_Fill_n(_First, _Count, _Valty());
	}

#endif

#else
	template < class T > struct is_pod
	{
		static bool
			const value = __is_pod(T);
	};
#endif

#endif // _MSC_VER < 1920


	template < class It, class T, class Traits, class Ax>
	back_insert_iterator<basic_vector_based_string<T, Traits, Ax>> copy(It begin, It end, back_insert_iterator<basic_vector_based_string<T, Traits, Ax>> out)
	{
		typedef back_insert_iterator<basic_vector_based_string<T, Traits, Ax>> Base;
		struct Derived : Base
		{
			using Base::container;
		};

		typename Base::container_type& container = *static_cast<Derived&> (out).container;
		container.append(begin, end);
		return out;
	}
}


namespace stdext
{
	template < class T > struct remove_const
	{
		typedef T type;
	};

	template < class T > struct remove_const<const T >
	{
		typedef T type;
	};

	template < class T > struct remove_volatile
	{
		typedef T type;
	};

	template < class T > struct remove_volatile < volatile T>
	{
		typedef T type;
	};

	template < class T>
	struct remove_cv
	{
		typedef typename remove_volatile < typename remove_const<T>::type>::type type;
	};

}

struct File
{
	typedef int handle_type;
	handle_type f;
	~File()
	{
		if (f)
		{
			_close(f);
		}
	}

	operator handle_type& ()
	{
		return this->f;
	}

	operator handle_type() const
	{
		return this->f;
	}

	handle_type* operator&()
	{
		return &this->f;
	}
};

#define X(Class)(GetProcAddress(GetModuleHandle(TEXT("win32u.dll")), HOOK_TYPE(Class)::static_name()))
HOOK_DEFINE_DEFAULT(HANDLE __stdcall, NtUserGetProp, (HWND hWnd, ATOM PropId), X);
HOOK_DEFINE_DEFAULT(BOOL __stdcall, NtUserSetProp, (HWND hWnd, ATOM PropId, HANDLE value), X);
HOOK_DEFINE_DEFAULT(LRESULT __stdcall, NtUserMessageCall, (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, ULONG_PTR xParam, DWORD xpfnProc, BOOL bAnsi), X);
HOOK_DEFINE_DEFAULT(BOOL __stdcall, NtUserRedrawWindow, (HWND hWnd, CONST RECT * lprcUpdate, HRGN hrgnUpdate, UINT flags), X); 
#undef X

// atomic_namespace moved to src/util/atomic_compat.hpp




template < size_t N>
int safe_stprintf(TCHAR(&s)[N], TCHAR
	const* const format, ...)
{
	int result;
	va_list args;
	va_start(args, format);
	result = _vsntprintf(s, N - 1, format, args);
	va_end(args);
	if (result < 0)
	{
		s[0] = _T('\0');
	}
	else if (result == N - 1)
	{
		s[result] = _T('\0');
	}

	return result;
}

ATL::CWindow topmostWindow;
atomic_namespace::recursive_mutex global_exception_mutex;

long global_exception_handler(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	long result;
	if (ExceptionInfo->ExceptionRecord->ExceptionCode == 0x40010006 /*DBG_PRINTEXCEPTION_C*/ ||
		ExceptionInfo->ExceptionRecord->ExceptionCode == 0x4001000A /*DBG_PRINTEXCEPTION_WIDE_C*/ ||
		ExceptionInfo->ExceptionRecord->ExceptionCode == 0xE06D7363 /*C++ exception*/ ||
		ExceptionInfo->ExceptionRecord->ExceptionCode == RPC_S_SERVER_UNAVAILABLE)
	{
		result = EXCEPTION_CONTINUE_SEARCH;
	}
	else
	{
		atomic_namespace::unique_lock<atomic_namespace::recursive_mutex>
			const guard(global_exception_mutex);
		TCHAR buf[512];
		safe_stprintf(buf, _T("The program encountered an error 0x%lX.\r\n\r\nPLEASE send me an email, so I can try to fix it.!\r\n\r\nIf you see OK, press OK.\r\nOtherwise:\r\n- Press Retry to attempt to handle the error (recommended)\r\n- Press Abort to quit\r\n- Press Ignore to continue (NOT recommended)"), ExceptionInfo->ExceptionRecord->ExceptionCode);
		buf[_countof(buf) - 1] = _T('\0');  // Ensure null termination for static analysis
		int
			const r = MessageBox(topmostWindow.m_hWnd, buf, _T("Fatal Error"), MB_ICONERROR | ((ExceptionInfo->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) ? MB_OK : MB_ABORTRETRYIGNORE) | MB_TASKMODAL);
		if (r == IDABORT)
		{
			_exit(ExceptionInfo->ExceptionRecord->ExceptionCode);
		}

		result = r == IDIGNORE ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
	}

	return result;
}


[[nodiscard]] bool is_ascii(wchar_t
	const* const s, size_t
	const n)
{
	bool result = true;
	for (size_t i = 0; i != n; ++i)
	{
		wchar_t
			const ch = s[i];
		result &= (SCHAR_MIN <= static_cast<long long> (ch)) & (ch <= SCHAR_MAX);
	}

	return result;
}

void DisplayError(LPTSTR lpszFunction)
// Routine Description:
// Retrieve and output the system error message for the last-error code
{
	LPVOID lpMsgBuf = nullptr;
	LPVOID lpDisplayBuf = nullptr;
	DWORD dw = GetLastError();

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0,
		nullptr);

	lpDisplayBuf =
		(LPVOID)LocalAlloc(LMEM_ZEROINIT,
			(lstrlen((LPCTSTR)lpMsgBuf) +
				lstrlen((LPCTSTR)lpszFunction) +
				40)	// account for format string
			*
			sizeof(TCHAR));

	if (lpDisplayBuf && FAILED(StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error code %d as follows:\n%s"),
		lpszFunction,
		dw,
		(LPTSTR)lpMsgBuf)))
	{
		printf("FATAL ERROR: Unable to output error code.\n");
	}

	_tprintf(TEXT("ERROR: %s\n"), (LPCTSTR)lpDisplayBuf);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
}

static void append_directional(std::tvstring& str, TCHAR
	const sz[], size_t
	const cch, int
	const ascii_mode /*-1 = decompress ASCII, 0 = no change, +1 = compress ASCII */, bool
	const reverse = false)
{
	typedef std::tvstring Str;
	size_t
		const cch_in_str = ascii_mode > 0 ? (cch + 1) / 2 : cch;
	size_t
		const n = str.size(); 
#if defined(_MSC_VER) && !defined(_CPPLIB_VER)
			struct Derived : Str
		{
			using Str::_First;
			using Str::_Last;
			using Str::_End;
			using Str::allocator;
};

	Derived & derived = static_cast<Derived&> (str); 
#endif
		if (n + cch_in_str > str.capacity())
		{
#if defined(_MSC_VER) && !defined(_CPPLIB_VER)
				dynamic_allocator<TCHAR>& alloc /*ensure correct type of allocator! */ = derived.allocator;
			size_t
				const min_capacity = str.capacity() + str.capacity() / 2;
			size_t new_capacity = n + cch_in_str;
			if (new_capacity < min_capacity)
			{
				new_capacity = min_capacity;
			}

			if (TCHAR* const p = alloc.reallocate(derived._First, new_capacity, true /*TCHAR is movable */))
			{
				derived._First = p;
				derived._Last = derived._First + static_cast<ptrdiff_t> (n);
				derived._End = derived._First + static_cast<ptrdiff_t> (new_capacity);
			}
#endif
				str.reserve(n + n / 2 + cch_in_str * 2);
		}
#if defined(_MSC_VER) && !defined(_CPPLIB_VER)
				// we have enough capacity, so just extend
				derived._Last += static_cast<ptrdiff_t> (cch_in_str); 
#else
				str.resize(n + cch_in_str); 
#endif
				if (cch)
				{
					TCHAR* const o = &str[n];
					if (reverse)
					{
						if (ascii_mode < 0)
						{
							std::reverse_copy(static_cast<char
								const*> (static_cast<void
									const*> (sz)), static_cast<char
								const*> (static_cast<void
									const*> (sz)) + static_cast<ptrdiff_t> (cch), o);
						}
						else if (ascii_mode > 0)
						{
							for (size_t i = 0; i != cch; ++i)
							{
								static_cast<char*> (static_cast<void*> (o))[i] = static_cast<char> (sz[cch - 1 - i]);
							}
						}
						else
						{
							std::reverse_copy(sz, sz + static_cast<ptrdiff_t> (cch), o);
						}
					}
					else
					{
						if (ascii_mode < 0)
						{
							std::copy(static_cast<char
								const*> (static_cast<void
									const*> (sz)), static_cast<char
								const*> (static_cast<void
									const*> (sz)) + static_cast<ptrdiff_t> (cch), o);
						}
						else if (ascii_mode > 0)
						{
							for (size_t i = 0; i != cch; ++i)
							{
								static_cast<char*> (static_cast<void*> (o))[i] = static_cast<char> (sz[i]);
							}
						}
						else
						{
							std::copy(sz, sz + static_cast<ptrdiff_t> (cch), o);
						}
					}
				}
}


void CppRaiseException(unsigned long
	const error)
{
	struct _EXCEPTION_POINTERS* pExPtrs = nullptr;
	bool thrown = false;
	int exCode = 0;
	struct CppExceptionThrower
	{
		void operator()(int exCode, struct _EXCEPTION_POINTERS* pExPtrs)
		{
			throw CStructured_Exception(exCode, pExPtrs);
		}

		static bool assign(struct _EXCEPTION_POINTERS** to, struct _EXCEPTION_POINTERS* from)
		{
			*to = from;
			return true;
		}
	};

	__try
	{
		ATL::_AtlRaiseException(error, 0);
	}

	__except (CppExceptionThrower::assign(&pExPtrs, static_cast<struct _EXCEPTION_POINTERS*> (GetExceptionInformation())) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		exCode = GetExceptionCode();
		thrown = true;
	}

	if (thrown)
	{
		CppExceptionThrower()(exCode, pExPtrs);
	}
}

void CheckAndThrow(int
	const success)
{
	if (!success)
	{
		CppRaiseException(GetLastError());
	}
}

[[nodiscard]] LPTSTR GetAnyErrorText(DWORD errorCode, va_list* pArgList = nullptr)
{
	static TCHAR buffer[1 << 15];
	ZeroMemory(buffer, sizeof(buffer));
	if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | (pArgList == nullptr ? FORMAT_MESSAGE_IGNORE_INSERTS : 0), nullptr, errorCode, 0, buffer, sizeof(buffer) / sizeof(*buffer), pArgList))
	{
		if (!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | (pArgList == nullptr ? FORMAT_MESSAGE_IGNORE_INSERTS : 0), GetModuleHandle(_T("NTDLL.dll")), errorCode, 0, buffer, sizeof(buffer) / sizeof(*buffer), pArgList))
		{
			safe_stprintf(buffer, _T("%#lx"), errorCode);
		}
}

	return buffer;
}

// Wow64 and Wow64Disable classes extracted to src/util/wow64.hpp

// Import winnt types from extracted header (uffs::winnt namespace)
// This replaces the duplicate namespace that was previously defined here
using namespace uffs::winnt;
namespace winnt = uffs::winnt;  // Alias for code that uses winnt:: prefix

// Global fsinfo variable (preserved from original code)
FILE_FS_DEVICE_INFORMATION fsinfo;


[[nodiscard]] bool isdevnull(int fd)
{
	winnt::IO_STATUS_BLOCK iosb;
	winnt::FILE_FS_DEVICE_INFORMATION fsinfo = {};
	return winnt::NtQueryVolumeInformationFile((HANDLE)_get_osfhandle(fd), &iosb, &fsinfo, sizeof(fsinfo), 4) == 0 && fsinfo.DeviceType == 0x00000015;
}

[[nodiscard]] bool isdevnull(FILE* f)
{
	return isdevnull(

#ifdef _O_BINARY 
		_fileno(f)
#else
		fileno(f)
#endif
			);
}


namespace ntfs
{
#pragma pack(push, 1)
	struct NTFS_BOOT_SECTOR
	{
		unsigned char Jump[3];
		unsigned char Oem[8];
		unsigned short BytesPerSector;
		unsigned char SectorsPerCluster;
		unsigned short ReservedSectors;
		unsigned char Padding1[3];
		unsigned short Unused1;
		unsigned char MediaDescriptor;
		unsigned short Padding2;
		unsigned short SectorsPerTrack;
		unsigned short NumberOfHeads;
		unsigned long HiddenSectors;
		unsigned long Unused2;
		unsigned long Unused3;
		long long TotalSectors;
		long long MftStartLcn;
		long long Mft2StartLcn;
		signed char ClustersPerFileRecordSegment;
		unsigned char Padding3[3];
		unsigned long ClustersPerIndexBlock;
		long long VolumeSerialNumber;
		unsigned long Checksum;

		unsigned char BootStrap[0x200 - 0x54];
		unsigned int file_record_size() const
		{
			return this->ClustersPerFileRecordSegment >= 0 ? this->ClustersPerFileRecordSegment * this->SectorsPerCluster * this->BytesPerSector : 1U << static_cast<int> (-this->ClustersPerFileRecordSegment);
		}

		unsigned int cluster_size() const
		{
			return this->SectorsPerCluster * this->BytesPerSector;
		}
	};
#pragma pack(pop)
	struct MULTI_SECTOR_HEADER
	{
		unsigned long Magic;
		unsigned short USAOffset;
		unsigned short USACount;

		bool unfixup(size_t max_size)
		{
			unsigned short* usa = reinterpret_cast<unsigned short*> (&reinterpret_cast<unsigned char*> (this)[this->USAOffset]);
			unsigned short
				const usa0 = usa[0];
			bool result = true;
			for (unsigned short i = 1; i < this->USACount; i++)
			{
				const size_t offset = static_cast<size_t>(i) * 512 - sizeof(unsigned short);
				unsigned short* const check = (unsigned short*)((unsigned char*)this + offset);
				if (offset < max_size)
				{
					result &= *check == usa0;
					*check = usa[i];
				}
				else
				{
					break;
				}
			}

			return result;
		}
	};

	enum class AttributeTypeCode : int
	{
		AttributeNone                = 0,      // For default-constructed comparison
		AttributeStandardInformation = 0x10,
		AttributeAttributeList       = 0x20,
		AttributeFileName            = 0x30,
		AttributeObjectId            = 0x40,
		AttributeSecurityDescriptor  = 0x50,
		AttributeVolumeName          = 0x60,
		AttributeVolumeInformation   = 0x70,
		AttributeData                = 0x80,
		AttributeIndexRoot           = 0x90,
		AttributeIndexAllocation     = 0xA0,
		AttributeBitmap              = 0xB0,
		AttributeReparsePoint        = 0xC0,
		AttributeEAInformation       = 0xD0,
		AttributeEA                  = 0xE0,
		AttributePropertySet         = 0xF0,
		AttributeLoggedUtilityStream = 0x100,
		AttributeEnd                 = -1,
	};

	struct ATTRIBUTE_RECORD_HEADER
	{
		AttributeTypeCode Type;
		unsigned long Length;
		unsigned char IsNonResident;
		unsigned char NameLength;
		unsigned short NameOffset;
		unsigned short Flags;	// 0x0001 = Compressed, 0x4000 = Encrypted, 0x8000 = Sparse
		unsigned short Instance;
		union
		{
			struct RESIDENT
			{
				unsigned long ValueLength;
				unsigned short ValueOffset;
				unsigned short Flags;
				inline void* GetValue()
				{
					return reinterpret_cast<void*> (reinterpret_cast<char*> (CONTAINING_RECORD(this, ATTRIBUTE_RECORD_HEADER, Resident)) + this->ValueOffset);
				}

				inline void
					const* GetValue() const
				{
					return reinterpret_cast<const void*> (reinterpret_cast<const char*> (CONTAINING_RECORD(this, ATTRIBUTE_RECORD_HEADER, Resident)) + this->ValueOffset);
				}
			}

			Resident;
			struct NONRESIDENT
			{
				long long LowestVCN;
				long long HighestVCN;
				unsigned short MappingPairsOffset;
				unsigned char CompressionUnit;
				unsigned char Reserved[5];
				long long AllocatedSize;
				long long DataSize;
				long long InitializedSize;
				long long CompressedSize;
			}

			NonResident;
		};

		ATTRIBUTE_RECORD_HEADER* next()
		{
			return reinterpret_cast<ATTRIBUTE_RECORD_HEADER*> (reinterpret_cast<unsigned char*> (this) + this->Length);
		}

		ATTRIBUTE_RECORD_HEADER
			const* next() const
		{
			return reinterpret_cast<ATTRIBUTE_RECORD_HEADER
				const*> (reinterpret_cast<unsigned char
					const*> (this) + this->Length);
		}

		wchar_t* name()
		{
			return reinterpret_cast<wchar_t*> (reinterpret_cast<unsigned char*> (this) + this->NameOffset);
		}

		wchar_t
			const* name() const
		{
			return reinterpret_cast<wchar_t
				const*> (reinterpret_cast<unsigned char
					const*> (this) + this->NameOffset);
		}
	};

	// Note: Kept as plain enum (not enum class) because these flags are used with
	// bitwise operations on unsigned short Flags field in FILE_RECORD_SEGMENT_HEADER
	enum FILE_RECORD_HEADER_FLAGS
	{
		FRH_IN_USE = 0x0001, /*Record is in use */
		FRH_DIRECTORY = 0x0002, /*Record is a directory */
	};

	struct FILE_RECORD_SEGMENT_HEADER
	{
		MULTI_SECTOR_HEADER MultiSectorHeader;
		unsigned long long LogFileSequenceNumber;
		unsigned short SequenceNumber;
		unsigned short LinkCount;
		unsigned short FirstAttributeOffset;
		unsigned short Flags /*FILE_RECORD_HEADER_FLAGS */;
		unsigned long BytesInUse;
		unsigned long BytesAllocated;
		unsigned long long BaseFileRecordSegment;
		unsigned short NextAttributeNumber;
		//http://blogs.technet.com/b/joscon/archive/2011/01/06/how-hard-links-work.aspx
		unsigned short SegmentNumberUpper_or_USA_or_UnknownReserved;	// WARNING: This does NOT seem to be the actual "upper" segment number of anything! I found it to be 0x26e on one of my drives... and checkdisk didn't say anything about it
		unsigned long SegmentNumberLower;
		ATTRIBUTE_RECORD_HEADER* begin()
		{
			return reinterpret_cast<ATTRIBUTE_RECORD_HEADER*> (reinterpret_cast<unsigned char*> (this) + this->FirstAttributeOffset);
		}

		ATTRIBUTE_RECORD_HEADER
			const* begin() const
		{
			return reinterpret_cast<ATTRIBUTE_RECORD_HEADER
				const*> (reinterpret_cast<unsigned char
					const*> (this) + this->FirstAttributeOffset);
		}

		void* end(size_t
			const max_buffer_size = ~size_t())
		{
			return reinterpret_cast<unsigned char*> (this) + (max_buffer_size < this->BytesInUse ? max_buffer_size : this->BytesInUse);
		}

		void
			const* end(size_t
				const max_buffer_size = ~size_t()) const
		{
			return reinterpret_cast<unsigned char
				const*> (this) + (max_buffer_size < this->BytesInUse ? max_buffer_size : this->BytesInUse);
		}
	};

	struct FILENAME_INFORMATION
	{
		unsigned long long ParentDirectory;
		long long CreationTime;
		long long LastModificationTime;
		long long LastChangeTime;
		long long LastAccessTime;
		long long AllocatedLength;
		long long FileSize;
		unsigned long FileAttributes;
		unsigned short PackedEaSize;
		unsigned short Reserved;
		unsigned char FileNameLength;
		unsigned char Flags;
		WCHAR FileName[1];
	};

	struct STANDARD_INFORMATION
	{
		long long CreationTime;
		long long LastModificationTime;
		long long LastChangeTime;
		long long LastAccessTime;
		unsigned long FileAttributes;
		// There's more, but only in newer versions
	};

	struct INDEX_HEADER
	{
		unsigned long FirstIndexEntry;
		unsigned long FirstFreeByte;
		unsigned long BytesAvailable;
		unsigned char Flags;	// '1' == has INDEX_ALLOCATION
		unsigned char Reserved[3];
	};

	struct INDEX_ROOT
	{
		AttributeTypeCode Type;
		unsigned long CollationRule;
		unsigned long BytesPerIndexBlock;
		unsigned char ClustersPerIndexBlock;
		INDEX_HEADER Header;
	};

	struct ATTRIBUTE_LIST
	{
		AttributeTypeCode AttributeType;
		unsigned short Length;
		unsigned char NameLength;
		unsigned char NameOffset;
		unsigned long long StartVcn;	// LowVcn
		unsigned long long FileReferenceNumber;
		unsigned short AttributeNumber;
		unsigned short AlignmentOrReserved[3];
	};

	enum class ReparseTypeFlags : unsigned int
	{
		ReparseIsMicrosoft              = 0x80000000,
		ReparseIsHighLatency            = 0x40000000,
		ReparseIsAlias                  = 0x20000000,
		ReparseTagNSS                   = 0x68000005,
		ReparseTagNSSRecover            = 0x68000006,
		ReparseTagSIS                   = 0x68000007,
		ReparseTagSDFS                  = 0x68000008,
		ReparseTagMountPoint            = 0x88000003,
		ReparseTagHSM                   = 0xA8000004,
		ReparseTagSymbolicLink          = 0xE8000000,
		ReparseTagMountPoint2           = 0xA0000003,
		ReparseTagSymbolicLink2         = 0xA000000C,
		ReparseTagWofCompressed         = 0x80000017,
		ReparseTagWindowsContainerImage = 0x80000018,
		ReparseTagGlobalReparse         = 0x80000019,
		ReparseTagAppExecLink           = 0x8000001B,
		ReparseTagCloud                 = 0x9000001A,
		ReparseTagGVFS                  = 0x9000001C,
		ReparseTagLinuxSymbolicLink     = 0xA000001D,
		// Lots more exist... see https://github.com/JFLarvoire/SysToolsLib/blob/master/C/MsvcLibX/include/reparsept.h
	};

	struct REPARSE_POINT
	{
		ReparseTypeFlags TypeFlags;
		unsigned short DataLength;
		unsigned short Padding;
		// Reparse GUID follows if third-party
		// Reparse data follows in all cases
		void* begin()
		{
			return reinterpret_cast<void*> (reinterpret_cast<unsigned char*> (this) + sizeof(*this));
		}

		void
			const* begin() const
		{
			return reinterpret_cast<void
				const*> (reinterpret_cast<unsigned char
					const*> (this) + sizeof(*this));
		}

		void* end(size_t
			const max_buffer_size = ~size_t())
		{
			return reinterpret_cast<unsigned char*> (this->begin()) + static_cast<ptrdiff_t> (this->DataLength);
		}

		void
			const* end(size_t
				const max_buffer_size = ~size_t()) const
		{
			return reinterpret_cast<unsigned char
				const*> (this->begin()) + static_cast<ptrdiff_t> (this->DataLength);
		}
	};

	struct REPARSE_MOUNT_POINT_BUFFER
	{
		USHORT SubstituteNameOffset;
		USHORT SubstituteNameLength;
		USHORT PrintNameOffset;
		USHORT PrintNameLength;
		WCHAR PathBuffer[1];
	};

	static struct 
	{ 
		TCHAR 
		const* data; 
		size_t size; 
	} 
	
	attribute_names[] =
	{
#define X(S) { _T(S), sizeof(_T(S)) / sizeof(*_T(S)) - 1 }
		X(""),
		X("$STANDARD_INFORMATION"),
		X("$ATTRIBUTE_LIST"),
		X("$FILE_NAME"),
		X("$OBJECT_ID"),
		X("$SECURITY_DESCRIPTOR"),
		X("$VOLUME_NAME"),
		X("$VOLUME_INFORMATION"),
		X("$DATA"),
		X("$INDEX_ROOT"),
		X("$INDEX_ALLOCATION"),
		X("$BITMAP"),
		X("$REPARSE_POINT"),
		X("$EA_INFORMATION"),
		X("$EA"),
		X("$PROPERTY_SET"),
		X("$LOGGED_UTILITY_STREAM"),
#undef  X
	};

}



void remove_path_stream_and_trailing_sep(std::tvstring& path)
{
	size_t ifirstsep = 0;
	while (ifirstsep < path.size())
	{
		if (isdirsep(path[ifirstsep]))
		{
			break;
		}

		++ifirstsep;
	}

	while (!path.empty() && isdirsep(*(path.end() - 1)))
	{
		if (path.size() <= ifirstsep + 1)
		{
			break;
		}

		path.erase(path.end() - 1);
	}

	for (size_t i = path.size(); i != 0 && ((void) --i, true);)
	{
		if (path[i] == _T(':'))
		{
			path.erase(path.begin() + static_cast<ptrdiff_t> (i), path.end());
		}
		else if (isdirsep(path[i]))
		{
			break;
		}
	}

	while (!path.empty() && isdirsep(*(path.end() - 1)))
	{
		if (path.size() <= ifirstsep + 1)
		{
			break;
		}

		path.erase(path.end() - 1);
	}

	if (!path.empty() && *(path.end() - 1) == _T('.') && (path.size() == 1 || isdirsep(*(path.end() - 2))))
	{
		path.erase(path.end() - 1);
	}
}

std::tvstring NormalizePath(std::tvstring const& path)
{
	std::tvstring result;
	bool wasSep = false;
	bool isCurrentlyOnPrefix = true;
	for (TCHAR const& c : path)
	{
		isCurrentlyOnPrefix &= isdirsep(c);
		if (isCurrentlyOnPrefix || !wasSep || !isdirsep(c))
		{
			result.push_back(c);
		}

		wasSep = isdirsep(c);
	}

	if (!isrooted(result.begin(), result.end()))
	{
		std::tvstring currentDir(32 * 1024, _T('\0'));
		currentDir.resize(GetCurrentDirectory(static_cast<DWORD> (currentDir.size()), &currentDir[0]));
		adddirsep(currentDir);
		result = currentDir + result;
	}

	remove_path_stream_and_trailing_sep(result);
	return result;
}

std::tstring GetDisplayName(HWND hWnd, const std::tstring& path, DWORD shgdn)
{
	ATL::CComPtr<IShellFolder> desktop;
	STRRET ret = {};
	LPITEMIDLIST shidl = nullptr;
	ATL::CComBSTR bstr;
	ULONG attrs = SFGAO_ISSLOW | SFGAO_HIDDEN;
	std::tstring result = (!path.empty() && SHGetDesktopFolder(&desktop) == S_OK &&
		desktop->ParseDisplayName(hWnd, nullptr, const_cast<LPWSTR>(path.c_str()), nullptr, &shidl, &attrs) == S_OK &&
		(attrs & SFGAO_ISSLOW) == 0 &&
		desktop->GetDisplayNameOf(shidl, shgdn, &ret) == S_OK &&
		StrRetToBSTR(&ret, shidl, &bstr) == S_OK
		) ? static_cast<LPCTSTR> (bstr) : std::tstring(basename(path.begin(), path.end()), path.end());
	return result;
}

int LCIDToLocaleName_XPCompatible(LCID lcid, LPTSTR name, int name_length)
{
	HMODULE hKernel32 = nullptr;
	if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCTSTR> (&GetSystemInfo), &hKernel32))
	{
		hKernel32 = nullptr;
	}

	typedef int WINAPI LCIDToLocaleName_t(LCID Locale, LPTSTR lpName, int cchName, DWORD dwFlags);
	if (hKernel32)
	if (LCIDToLocaleName_t* const LCIDToLocaleName = reinterpret_cast<LCIDToLocaleName_t*> (GetProcAddress(hKernel32, _CRT_STRINGIZE(LCIDToLocaleName))))
	{
		name_length = (*LCIDToLocaleName)(lcid, name, name_length, 0);
		name_length -= !!name_length;
	}
	else
	{
		ATL::CRegKey key;
		if (key.Open(HKEY_CLASSES_ROOT, TEXT("MIME\\Database\\Rfc1766"), KEY_QUERY_VALUE) == 0)
		{
			TCHAR value_data[64 + MAX_PATH] = {};
			TCHAR value_name[16];
			value_name[0] = _T('\0');
			safe_stprintf(value_name, _T("%04lX"), lcid);
			unsigned long value_data_length = sizeof(value_data) / sizeof(*value_data);
			LRESULT
				const result = key.QueryValue(value_data, value_name, &value_data_length);
			if (result == 0)
			{
				unsigned long i;
				for (i = 0; i != value_data_length; ++i)
				{
					if (value_data[i] == _T(';'))
					{
						break;
					}

					if (name_length >= 0 && i < static_cast<unsigned long> (name_length))
					{
						TCHAR ch = value_data[static_cast<ptrdiff_t> (i)];
						name[static_cast<ptrdiff_t> (i)] = ch;
					}
				}

				name_length = static_cast<int> (i);
			}
			else
			{
				name_length = 0;
			}
		}
		else
		{
			name_length = 0;
		}
	}

	return name_length;
}



WTL::CString LCIDToLocaleName_XPCompatible(LCID lcid)
{
	WTL::CString result;
	LPTSTR
		const buf = result.GetBufferSetLength(64);
	int
		const n = LCIDToLocaleName_XPCompatible(lcid, buf, result.GetLength());
	result.Delete(n, result.GetLength() - n);
	return result;
}

WTL::CString get_ui_locale_name()
{
	return LCIDToLocaleName_XPCompatible(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT));
}

// Use extracted Handle and IoPriority classes from src/util/handle.hpp and src/io/io_priority.hpp
using uffs::Handle;
using uffs::IoPriority;

#include "src/util/memheap_vector.hpp"

template < class T, class Alloc = uffs::default_memheap_alloc<T> >
using memheap_vector = uffs::memheap_vector<T, Alloc>;


unsigned int get_cluster_size(void* const volume)
{
	winnt::IO_STATUS_BLOCK iosb;
	winnt::FILE_FS_SIZE_INFORMATION info = {};

	if (unsigned long
		const error = winnt::RtlNtStatusToDosError(winnt::NtQueryVolumeInformationFile(volume, &iosb, &info, sizeof(info), 3)))
	{
		CppRaiseException(error);
	}

	return info.BytesPerSector * info.SectorsPerAllocationUnit;
}

[[nodiscard]] std::vector<std::pair < unsigned long long, long long >> get_retrieval_pointers(TCHAR
	const path[], long long* const size, long long
	const mft_start_lcn, unsigned int
	const file_record_size)
{
	(void)mft_start_lcn;
	(void)file_record_size;
	typedef std::vector<std::pair < unsigned long long, long long >> Result;
	Result result;
	Handle handle;
	if (path)
	{
		HANDLE
			const opened = CreateFile(path, FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_NO_BUFFERING, nullptr);
		unsigned long
			const error = opened != INVALID_HANDLE_VALUE ? ERROR_SUCCESS : GetLastError();
		if (!error)
		{
			Handle(opened).swap(handle);
		}
		else if (error == ERROR_FILE_NOT_FOUND)
		{ /*do nothing */
		}
		else if (error)
		{
			CppRaiseException(error);
		}
	}

	if (handle)
	{
		result.resize(1 + (sizeof(RETRIEVAL_POINTERS_BUFFER) - 1) / sizeof(Result::value_type));
		STARTING_VCN_INPUT_BUFFER input = {};

		BOOL success;
		for (unsigned long nr; !(success = DeviceIoControl(handle, FSCTL_GET_RETRIEVAL_POINTERS, &input, sizeof(input), &*result.begin(), static_cast<unsigned long> (result.size()) * sizeof(*result.begin()), &nr, nullptr), success) && GetLastError() == ERROR_MORE_DATA;)
		{
			size_t
				const n = result.size();
			Result(/*free old memory */).swap(result);
			Result(n * 2).swap(result);
		}

		CheckAndThrow(success);
		if (size)
		{
			LARGE_INTEGER large_size;
			CheckAndThrow(GetFileSizeEx(handle, &large_size));
			*size = large_size.QuadPart;
		}

		result.erase(result.begin() + 1 + reinterpret_cast<unsigned long
			const&> (*result.begin()), result.end());
		result.erase(result.begin(), result.begin() + 1);
	}

	return result;
}

// propagate_const and fast_subscript templates extracted to src/index/ntfs_index.hpp

#include "src/util/buffer.hpp"

#include "src/util/containers.hpp"
#include "src/index/ntfs_index.hpp"

// NtfsIndex class has been extracted to src/index/ntfs_index.hpp
struct MatchOperation
{
	value_initialized < bool>
		is_regex,
		is_path_pattern,
		is_stream_pattern,
		requires_root_path_match;

	std::tvstring root_path_optimized_away;

	string_matcher matcher;

	MatchOperation() {}

	void init(std::tstring pattern)
	{
		is_regex = !pattern.empty() &&
			*pattern.begin() == _T('>');

		if (is_regex)
		{
			pattern.erase(pattern.begin());
		}

		is_path_pattern = is_regex ||
			~pattern.find(_T('\\')) ||
			~pattern.find(_T("**"));

		is_stream_pattern =
			is_regex ||
			~pattern.find(_T(':'));

		requires_root_path_match =
			is_path_pattern &&
			!is_regex &&
			pattern.size() >= 2 &&
			*(pattern.begin() + 0) != _T('*') &&
			*(pattern.begin() + 0) != _T('?') &&
			*(pattern.begin() + 1) != _T('*') &&
			*(pattern.begin() + 1) != _T('?');

		if (requires_root_path_match)
		{
			root_path_optimized_away.insert(root_path_optimized_away.end(),
				pattern.begin(),
				std::find(pattern.begin(),
					pattern.end(),
					_T('\\')
				)
			);
			pattern.erase(pattern.begin(),
				pattern.begin() + static_cast<ptrdiff_t> (root_path_optimized_away.size())
			);
		}

		if (!is_path_pattern && !~pattern.find(_T('*')) && !~pattern.find(_T('?')))
		{
			pattern.insert(pattern.begin(), _T('*'));
			pattern.insert(pattern.begin(), _T('*'));
			pattern.insert(pattern.end()  , _T('*'));
			pattern.insert(pattern.end()  , _T('*'));
		}

		string_matcher(is_regex ?
			string_matcher::pattern_regex :
			is_path_pattern ?
			string_matcher::pattern_globstar :
			string_matcher::pattern_glob,
			string_matcher::pattern_option_case_insensitive,
			pattern.data(), pattern.size()).swap(matcher);
	}	//init

	bool prematch(std::tvstring
		const& root_path) const
	{
		return !requires_root_path_match || (root_path.size() >= root_path_optimized_away.size() &&
			std::equal(root_path.begin(),
				root_path.begin() + static_cast<ptrdiff_t> (root_path_optimized_away.size()),
				root_path_optimized_away.begin()
			)
			);
	}	//prematch

	std::tvstring get_current_path(std::tvstring
		const& root_path) const
	{
		std::tvstring current_path =
			root_path_optimized_away.empty() ? root_path : std::tvstring();

		while (!current_path.empty() &&
			*(current_path.end() - 1) == _T('\\')
			)
		{
			current_path.erase(current_path.end() - 1);
		}

		return current_path;
	}	//get current path

};
// MatchOperation

// std::is_scalar specializations are now in src/index/ntfs_index.hpp

#include "src/util/com_init.hpp"
#include "src/io/io_completion_port.hpp"
#include "src/io/mft_reader.hpp"

class CDisableListViewUnnecessaryMessages : hook_detail::thread_hook_swap < HOOK_TYPE(NtUserMessageCall) >
{
	typedef CDisableListViewUnnecessaryMessages hook_type;
	CDisableListViewUnnecessaryMessages(hook_type
		const&);
	hook_type& operator=(hook_type
		const&);
	enum
	{
		LVM_GETACCVERSION = 0x10C1
	};

	struct
	{
		HWND hwnd;
		bool valid;
		LRESULT value;
	}

	prev_result_LVM_GETACCVERSION;
	LRESULT operator()(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, ULONG_PTR xParam, DWORD xpfnProc, BOOL bAnsi) override
	{
		LRESULT result;
		if (msg == LVM_GETACCVERSION && this->prev_result_LVM_GETACCVERSION.valid && this->prev_result_LVM_GETACCVERSION.hwnd == hWnd)
		{
			result =
				msg == LVM_GETACCVERSION && this->prev_result_LVM_GETACCVERSION.valid ? this->prev_result_LVM_GETACCVERSION.value :
				0;
		}
		else
		{
			result = this->hook_base_type::operator()(hWnd, msg, wParam, lParam, xParam, xpfnProc, bAnsi);
			if (msg == LVM_GETACCVERSION)
			{
				this->prev_result_LVM_GETACCVERSION.hwnd  = hWnd;
				this->prev_result_LVM_GETACCVERSION.value = result;
				this->prev_result_LVM_GETACCVERSION.valid = true;
			}
		}

		return result;
	}

public: CDisableListViewUnnecessaryMessages() : prev_result_LVM_GETACCVERSION() {}
	  ~CDisableListViewUnnecessaryMessages() {}
};

#ifdef WM_SETREDRAW
class CSetRedraw
{
	static TCHAR
		const* key()
	{
		return _T("Redraw.{E8F1F2FD-7AD9-4AC6-88F9-59CE0F0BB173}");
	}

	struct Hooked
	{
		typedef Hooked hook_type;
		HWND prev_hwnd;
		BOOL prev_result;
		struct : hook_detail::thread_hook_swap < HOOK_TYPE(NtUserRedrawWindow) >
		{
			BOOL operator()(HWND hWnd, CONST RECT* lprcUpdate, HRGN hrgnUpdate, UINT flags) override
			{
				BOOL result;
				hook_type* const self = CONTAINING_RECORD(this, hook_type, HOOK_CONCAT(hook_, NtUserRedrawWindow));
				if (!self || self->prev_hwnd != hWnd)
				{
					result = this->hook_base_type::operator()(hWnd, lprcUpdate, hrgnUpdate, flags);
					if (self)
					{
						self->prev_result = result;
						self->prev_hwnd = hWnd;
					}
				}
				else
				{
					result = self->prev_result;
				}

				return result;
			}
		}

		HOOK_CONCAT(hook_, NtUserRedrawWindow);
		Hooked() : prev_hwnd(), prev_result() {}
		~Hooked() {}
	}

	HOOK_CONCAT(hook_, NtUserProp);
public:
	HWND hWnd;
	HANDLE notPrev;
	CSetRedraw(HWND
		const hWnd, BOOL redraw) : hWnd(hWnd), notPrev(GetProp(hWnd, key()))
	{
		SendMessage(hWnd, WM_SETREDRAW, redraw, 0);
		SetProp(hWnd, key(), (HANDLE)(!redraw));
	}~CSetRedraw()
	{
		SetProp(hWnd, key(), notPrev);
		SendMessage(hWnd, WM_SETREDRAW, !this->notPrev, 0);
	}
};

#endif

class RefCountedCString : public WTL::CString	// ref-counted in order to ensure that copying doesn't move the buffer (e.g. in case a containing vector resizes)
{
	typedef RefCountedCString this_type;
	typedef WTL::CString base_type;
	void check_same_buffer(this_type
		const& other) const
	{
		// In newer ATL, GetData() is private. Use GetString() instead -
		// if strings share the same buffer, GetString() returns the same pointer.
		if (this->GetString() != other.GetString())
		{
			throw std::logic_error("expected the same buffer for both strings");
		}
	}

public:
	RefCountedCString() : base_type() {}

	RefCountedCString(this_type
		const& other) : base_type(other)
	{
		this->check_same_buffer(other);
	}

	this_type& operator=(this_type
		const& other)
	{
		this_type& result = static_cast<this_type&> (this->base_type::operator=(static_cast<base_type
			const&> (other)));
		result.check_same_buffer(other);
		return result;
	}

	LPTSTR data()
	{
		return this->base_type::GetBuffer(this->base_type::GetLength());
	}

	LPTSTR c_str()
	{
		int
			const n = this->base_type::GetLength();
		LPTSTR
			const result = this->base_type::GetBuffer(n + 1);
		if (result[n] != _T('\0'))
		{
			result[n] = _T('\0');
		}

		return result;
	}

	operator LPTSTR()
	{
		return this->c_str();
	}

	friend std::basic_ostream<TCHAR>& operator<<(std::basic_ostream<TCHAR>& ss, this_type& me);	// Do NOT implement this. Turns out DDK's implementation doesn't handle embedded null characters correctly. Just use a basic_string directly instead.
};

extern HMODULE mui_module;

class StringLoader
{
	class SwapModuleResource
	{
		SwapModuleResource(SwapModuleResource
			const&);
		SwapModuleResource& operator=(SwapModuleResource
			const&);
		HINSTANCE prev;
	public:
		~SwapModuleResource()
		{
			InterlockedExchangePointer(reinterpret_cast<void**> (&_Module.m_hInstResource), prev);
		}

		SwapModuleResource(HINSTANCE
			const instance) : prev()
		{
			InterlockedExchangePointer(reinterpret_cast<void**> (&_Module.m_hInstResource), mui_module);
		}
	};

	DWORD thread_id;
	std::vector<RefCountedCString> strings;
public:
	StringLoader() : thread_id(GetCurrentThreadId()) {}

	RefCountedCString& operator()(unsigned short
		const id)
	{
		if (id >= this->strings.size())
		{
			assert(GetCurrentThreadId() && this->thread_id && "cannot expand string table from another thread");
			this->strings.resize(static_cast<size_t>(id) + 1);
		}

		if (this->strings[id].IsEmpty())
		{
			assert(GetCurrentThreadId() && this->thread_id && "cannot modify string table from another thread");
			RefCountedCString& str = this->strings[id];
			bool success = mui_module && (SwapModuleResource(mui_module), !!str.LoadString(id));
			if (!success)
			{
				(void)str.LoadString(id);
			}
		}

		return this->strings[id];
	}
};

class ImageListAdapter
{
	WTL::CImageList me;
public:
	ImageListAdapter(WTL::CImageList
		const me) : me(me) {}

	ImageListAdapter* operator->()
	{
		return this;
	}

	ImageListAdapter
		const* operator->() const
	{
		return this;
	}

	int GetImageCount() const
	{
		return me.GetImageCount();
	}

	bool GetSize(int
		const index, int& width, int& height) const
	{
		IMAGEINFO imginfo;
		bool
			const result = !!me.GetImageInfo(index, &imginfo);
		width = imginfo.rcImage.right - imginfo.rcImage.left;
		height = imginfo.rcImage.bottom - imginfo.rcImage.top;
		return result;
	}

	operator bool() const
	{
		return !!me;
	}
};

class ListViewAdapter
{
	WTL::CListViewCtrl* me;
	WTL::CString temp1;
public:
	typedef std::tvstring String;
	String temp2;
	typedef std::map<String, int> CachedColumnHeaderWidths;
	typedef ptrdiff_t text_getter_type(void* const me, int
		const item, int
		const subitem, String& result);
	CachedColumnHeaderWidths* cached_column_header_widths;
	text_getter_type* text_getter;
	class Freeze
	{
		Freeze(Freeze
			const&);
		Freeze& operator=(Freeze
			const&);
		ListViewAdapter* me;
	public:
		~Freeze() {}

		explicit Freeze(ListViewAdapter& me) : me(&me) {}
	};

	struct Column : HDITEM
	{
		String text;
		void SetMask(int
			const mask)
		{
			this->mask = mask;
		}

		String& GetText()
		{
			return text;
		}
	};

	struct Size : WTL::CSize
	{
		Size(WTL::CSize
			const& size) : WTL::CSize(size) {}

		int GetWidth() const
		{
			return this->cx;
		}

		int GetHeight() const
		{
			return this->cy;
		}
	};

	enum
	{
		LIST_MASK_TEXT = 0x0002
	};

	struct ClientDC : WTL::CClientDC
	{
		HFONT old_font;
		ClientDC(ClientDC
			const&);
		ClientDC& operator=(ClientDC
			const&);
		~ClientDC()
		{
			this->old_font = this->SelectFont(this->old_font);
		}

		explicit ClientDC(HWND
			const wnd) : WTL::CClientDC(wnd), old_font()
		{
			this->old_font = this->SelectFont(static_cast<ATL::CWindow> (wnd).GetFont());
		}

		int GetTextWidth(String
			const& string) const
		{
			WTL::CSize sizeRect;
			this->GetTextExtent(string.data(), static_cast<int> (string.size()), &sizeRect);
			return sizeRect.cx;
		}
	};

	ListViewAdapter(WTL::CListViewCtrl& me, CachedColumnHeaderWidths& cached_column_header_widths, text_getter_type* const text_getter) : me(&me), cached_column_header_widths(&cached_column_header_widths), text_getter(text_getter) {}

	ListViewAdapter* operator->()
	{
		return this;
	}

	ListViewAdapter
		const* operator->() const
	{
		return this;
	}

	typedef ImageListAdapter ImageList;
	bool GetColumn(int
		const j, Column& col)
	{
		bool result = false;
		WTL::CHeaderCtrl header = me->GetHeader();
		if (col.text.empty())
		{
			col.text.resize(16);
		}

		for (;;)
		{
			col.pszText = col.text.empty() ? nullptr : &*col.text.begin();
			col.cchTextMax = static_cast<int> (col.text.size());
			if (!header.GetItem(j, &col))
			{
				break;
			}

			size_t n = 0;
			while (col.pszText && static_cast<int> (n) < col.cchTextMax && col.pszText[n])
			{
				++n;
			}

			if (n < static_cast<size_t> (col.cchTextMax))
			{
				col.text.assign(col.pszText, col.pszText + static_cast<ptrdiff_t> (n));
				result = true;
				break;
			}

			n = n ? n * 2 : 1;
			if (n < col.text.capacity())
			{
				n = col.text.capacity();
			}

			col.text.resize(n);
		}

		if (!result)
		{
			col.text.clear();
		}

		return !!result;
	}

	int GetColumnCount() const
	{
		return me->GetHeader().GetItemCount();
	}

	std::pair<int, int> GetVisibleItems() const
	{
		std::pair<int, int> result(0, me->GetItemCount());
		WTL::CRect rc;
		if (me->GetClientRect(&rc))
		{
			result.first = me->GetTopIndex();
			WTL::CRect rcitem;
			for (int i = result.first; i < result.second; ++i)
			{
				if (me->GetItemRect(i, &rcitem, LVIR_BOUNDS) && !rcitem.IntersectRect(&rcitem, &rc))
				{
					result.second = i;
					break;
				}
			}
		}

		return result;
	}

	bool DeleteColumn(int
		const i)
	{
		return !!me->DeleteColumn(i);
	}

	int InsertColumn(int
		const i, String& text)
	{
		return me->InsertColumn(i, text.c_str());
	}

	int GetColumnWidth(int
		const i) const
	{
		WTL::CRect rc;
		return me->GetHeader().GetItemRect(i, &rc) ? rc.Width() : 0;
	}

	int GetItemCount() const
	{
		return me->GetItemCount();
	}

	String* GetItemText(int
		const item, int
		const subitem)
	{
		bool success = false;
		if (text_getter)
		{
			success = text_getter(me, item, subitem, temp2) >= 0;
		}
		else
		{
			if (me->GetItemText(item, subitem, temp1) >= 0)
			{
				success = true;
				temp2 = temp1;
			}
		}

		if (!success)
		{
			temp2.clear();
		}

		return success ? &temp2 : nullptr;
	}

	Size GetClientSize() const
	{
		WTL::CRect rc;
		return me->GetClientRect(rc) ? rc.Size() : WTL::CSize();
	}

	ImageList GetImageList()
	{
		unsigned long
			const view = me->GetView();
		return ImageList(me->GetImageList(view == LV_VIEW_DETAILS || view == LV_VIEW_LIST || view == LV_VIEW_SMALLICON ? LVSIL_SMALL : LVSIL_NORMAL));
	}

	bool SetColumnWidth(int
		const column, int
		const width /*-1 = autosize, -2 = use header */)
	{
		return !!me->SetColumnWidth(column, width);
	}

	bool SetColumnText(int
		const column, String& text)
	{
		LVCOLUMN col;
		col.mask = LVCF_TEXT;
		col.pszText = const_cast<TCHAR*> (text.c_str());
		return !!me->SetColumn(column, &col);
	}

	operator HWND() const
	{
		return *me;
	}
};

void autosize_columns(ListViewAdapter list)
{
	// Autosize columns
	size_t
		const m = static_cast<size_t> (list->GetColumnCount());
	size_t
		const n = static_cast<size_t> (list->GetItemCount());
	if (m && n)
	{
		{
			ListViewAdapter::String empty, text;
			int itemp1 = -1;
			int itemp2 = -1;
			for (int i = 0; i < static_cast<int> (m); ++i)
			{
				ListViewAdapter::Column col;
				col.SetMask(ListViewAdapter::LIST_MASK_TEXT);
				if (list->GetColumn(static_cast<int> (i), col))
				{
					text = col.GetText();
				}
				else
				{
					text.clear();
				}

				if (list->cached_column_header_widths->find(text) == list->cached_column_header_widths->end())
				{
					if (itemp1 < 0)
					{
						itemp1 = static_cast<int> (list->InsertColumn(static_cast<int> (m), empty));
					}

					if (itemp2 < 0)
					{
						itemp2 = static_cast<int> (list->InsertColumn(static_cast<int> (m), empty));
					}

					int cx = -1;
					if (text.empty())
					{
						cx = 0;
					}
					else
					{
						int
							const old_cx = list->GetColumnWidth(static_cast<int> (i));
						list->SetColumnText(static_cast<int> (m), text);
						if (list->SetColumnWidth(static_cast<int> (itemp2), -2 /*AUTOSIZE_USEHEADER*/))
						{
							cx = list->GetColumnWidth(static_cast<int> (itemp2));
						}

						list->SetColumnWidth(static_cast<int> (i), old_cx);
					}

					if (cx > 0 || (cx == 0 && text.empty()))
					{
						(*list->cached_column_header_widths)[text] = cx;
					}
				}
			}

			if (itemp2 >= 0)
			{
				list->DeleteColumn(itemp2);
			}

			if (itemp1 >= 0)
			{
				list->DeleteColumn(itemp1);
			}
		}

		std::vector< std::pair<std::pair < int /*priority */, int /*width */ >, int /*column */ > > sizes;
		std::vector<int> max_column_widths(m, 0);
		std::vector<int> column_slacks(m, 0);
		std::vector<int> column_widths(m, 0);
		{
			std::pair<int, int> visible = list->GetVisibleItems();
			int
				const feather = 1;
			visible.first = visible.first >= feather ? visible.first - feather : 0;
			visible.second = visible.second + feather <= static_cast<ptrdiff_t> (n) ? visible.second + feather : static_cast<int> (n);
			int cximg = 0;
			if (ListViewAdapter::ImageList imagelist = list->GetImageList())
			{
				int cyimg;
				if (!(imagelist->GetImageCount() > 0 && imagelist->GetSize(visible.first, cximg, cyimg)))
				{
					cximg = 0;
				}
			}

			sizes.reserve(static_cast<size_t> (n * m));
			ListViewAdapter::ClientDC dc(list);
			wchar_t
				const breaks[] = { L'\u200B', L'\u200C', L'\u200D' };

			for (size_t j = 0; j != m; ++j)
			{
				{ 		ListViewAdapter::Column col;
				col.SetMask(ListViewAdapter::LIST_MASK_TEXT);
				int cxheader;
				if (list->GetColumn(static_cast<int> (j), col))
				{
					ListViewAdapter::String text(col.GetText());
					int
						const cx_full = dc.GetTextWidth(text);
					ListViewAdapter::CachedColumnHeaderWidths::const_iterator
						const found = list->cached_column_header_widths->find(text);
					size_t ibreak = std::find_first_of(text.begin(), text.end(), &breaks[0], &breaks[sizeof(breaks) / sizeof(*breaks)]) - text.begin();
					if (ibreak < text.size())
					{
						text.erase(text.begin() + static_cast<ptrdiff_t> (ibreak), text.end());
						for (int k = 0; k != 3; ++k)
						{
							text.push_back('.');
						}
					}

					int cx = dc.GetTextWidth(text);
					if (found != list->cached_column_header_widths->end())
					{
						cxheader = found->second;
						int
							const slack = cx_full - cx;
						column_slacks[j] = cxheader - cx_full;
						using std::max;
						cx = max(cxheader - slack, 0);
					}
					else
					{
						cxheader = 0;
						cx += 22;
					}

					column_widths[j] = cx;
				}
				else
				{
					cxheader = 0;
				}

				sizes.push_back(std::make_pair(std::make_pair(SHRT_MAX, cxheader), static_cast<int> (j)));
				}

				for (size_t i = visible.first >= 0 ? static_cast<size_t> (visible.first) : 0; i != (visible.second >= 0 ? static_cast<size_t> (visible.second) : 0); ++i)
				{
					int cx = 0;
					if (ListViewAdapter::String
						const* const text = list->GetItemText(static_cast<long> (i), static_cast<long> (j)))
					{
						cx = (j == 0 ? cximg : 0) + dc.GetTextWidth(*text) + column_slacks[static_cast<size_t> (j)];
					}

					sizes.push_back(std::make_pair(std::make_pair(0, cx), static_cast<int> (j)));
				}
			}
		}

		for (auto const& entry : sizes)
		{
			size_t
				const c = static_cast<size_t>(entry.second);
			if (entry.first.first < SHRT_MAX)
			{
				using std::max;
				max_column_widths[c] = max(max_column_widths[c], entry.first.second);
			}
		}

		int remaining = list->GetClientSize().GetWidth();
		{
			using std::min;
			remaining -= min(remaining, 0);
		}

		for (const auto& width : column_widths)
		{
			remaining -= width;
		}

		std::sort(sizes.begin(), sizes.end());
		for (size_t i = 0; i != sizes.size() && remaining > 0; ++i)
		{
			int
				const item_width = sizes[i].first.second;
			size_t
				const column = static_cast<size_t> (sizes[i].second);
			int
				const old_column_width = column_widths[column];
			if (old_column_width < item_width)
			{
				using std::min;
				int
					const diff = min(item_width - old_column_width, remaining);
				assert(diff >= 0);
				column_widths[column] += diff;
				remaining -= diff;
			}
		}

		for (size_t i = column_widths.size(); i > 0 && ((void) --i, true);)
		{
			list->SetColumnWidth(static_cast<int> (i), column_widths[i]);
		}
	}
	else if (false)
	{
		for (size_t i = m; i > 0 && ((void) --i, true);)
		{
			list->SetColumnWidth(static_cast<int> (i), -1 /*AUTOSIZE*/);
		}
	}
}


#include "src/util/temp_swap.hpp"

class CProgressDialog : private CModifiedDialogImpl < CProgressDialog>, private WTL::CDialogResize < CProgressDialog>
{
	typedef CProgressDialog This;
	typedef CModifiedDialogImpl<CProgressDialog> Base;
	friend CDialogResize<This>;
	friend CDialogImpl<This>;
	friend CModifiedDialogImpl<This>;
	enum
	{
		IDD = IDD_DIALOGPROGRESS, BACKGROUND_COLOR = COLOR_WINDOW
	};

	class CUnselectableWindow : public ATL::CWindowImpl < CUnselectableWindow>
	{
#pragma warning(suppress: 4555)
		BEGIN_MSG_MAP(CUnselectableWindow)
			MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)
		END_MSG_MAP()
		LRESULT OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL&)
		{
			LRESULT result = this->DefWindowProc(uMsg, wParam, lParam);
			return result == HTCLIENT ? HTTRANSPARENT : result;
		}
	};

	WTL::CButton btnPause, btnStop;
	CUnselectableWindow progressText;
	WTL::CProgressBarCtrl progressBar;
	bool canceled;
	bool invalidated;
	unsigned long long creationTime;
	unsigned long long lastUpdateTime;
	HWND parent;
	std::tstring lastProgressText, lastProgressTitle;
	bool windowCreated;
	bool windowCreateAttempted;
	bool windowShowAttempted;
	TempSwap<ATL::CWindow > setTopmostWindow;
	int lastProgress, lastProgressTotal;
	StringLoader LoadString;

	void OnDestroy()
	{
		setTopmostWindow.reset();
	}

	BOOL OnInitDialog(CWindow /*wndFocus*/, LPARAM /*lInitParam*/)
	{
		this->setTopmostWindow.reset(::topmostWindow, this->m_hWnd);
		(this->progressText.SubclassWindow)(this->GetDlgItem(IDC_RICHEDITPROGRESS));
		// SetClassLongPtr(this->m_hWnd, GCLP_HBRBACKGROUND, reinterpret_cast< LONG_PTR>(GetSysColorBrush(COLOR_3DFACE)));
		this->btnPause.Attach(this->GetDlgItem(IDRETRY));
		this->btnPause.SetWindowText(this->LoadString(IDS_BUTTON_PAUSE));
		this->btnStop.Attach(this->GetDlgItem(IDCANCEL));
		this->btnStop.SetWindowText(this->LoadString(IDS_BUTTON_STOP));
		this->progressBar.Attach(this->GetDlgItem(IDC_PROGRESS1));
		this->DlgResize_Init(false, false, 0);
		ATL::CComBSTR bstr;
		this->progressText.GetWindowText(&bstr);
		this->lastProgressText.assign(static_cast<LPCTSTR> (bstr), bstr ? bstr.Length() : 0);

		return TRUE;
	}

	void OnPause(UINT uNotifyCode, int nID, CWindow wndCtl)
	{
		UNREFERENCED_PARAMETER(uNotifyCode);
		UNREFERENCED_PARAMETER(nID);
		UNREFERENCED_PARAMETER(wndCtl);
		__debugbreak();
	}

	void OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl)
	{
		UNREFERENCED_PARAMETER(uNotifyCode);
		UNREFERENCED_PARAMETER(nID);
		UNREFERENCED_PARAMETER(wndCtl);
		PostQuitMessage(ERROR_CANCELLED);
	}

	BOOL OnEraseBkgnd(WTL::CDCHandle dc)
	{
		RECT rc = {};

		this->GetClientRect(&rc);
		dc.FillRect(&rc, BACKGROUND_COLOR);
		return TRUE;
	}

	HBRUSH OnCtlColorStatic(WTL::CDCHandle dc, WTL::CStatic /*wndStatic*/)
	{
		return GetSysColorBrush(BACKGROUND_COLOR);
	}

#pragma warning(suppress: 4555)
		BEGIN_MSG_MAP_EX(This)
		CHAIN_MSG_MAP(CDialogResize < This>)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_INITDIALOG(OnInitDialog)
		// MSG_WM_ERASEBKGND(OnEraseBkgnd)
		// MSG_WM_CTLCOLORSTATIC(OnCtlColorStatic)
		COMMAND_HANDLER_EX(IDRETRY, BN_CLICKED, OnPause)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancel)
		END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(This)
		DLGRESIZE_CONTROL(IDC_PROGRESS1, DLSZ_MOVE_Y | DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_RICHEDITPROGRESS, DLSZ_SIZE_Y | DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()

	static BOOL EnableWindowRecursive(HWND hWnd, BOOL enable, BOOL includeSelf = true)
	{
		struct Callback
		{
			static BOOL CALLBACK EnableWindowRecursiveEnumProc(HWND hWnd, LPARAM lParam)
			{
				EnableWindowRecursive(hWnd, static_cast<BOOL> (lParam), TRUE);
				return TRUE;
			}
		};

		if (enable)
		{
			EnumChildWindows(hWnd, &Callback::EnableWindowRecursiveEnumProc, enable);
			return includeSelf && ::EnableWindow(hWnd, enable);
		}
		else
		{
			BOOL result = includeSelf && ::EnableWindow(hWnd, enable);
			EnumChildWindows(hWnd, &Callback::EnableWindowRecursiveEnumProc, enable);
			return result;
		}
	}

	unsigned long WaitMessageLoop(uintptr_t
		const handles[], size_t
		const nhandles)
	{
		for (;;)
		{
			unsigned long result;
			if (this->windowCreated)
			{
				result = MsgWaitForMultipleObjectsEx(static_cast<unsigned int> (nhandles), reinterpret_cast<HANDLE
					const*> (handles), UPDATE_INTERVAL, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
			}
			else
			{
				result = WaitForMultipleObjectsEx(static_cast<unsigned int> (nhandles), reinterpret_cast<HANDLE
					const*> (handles), FALSE, UPDATE_INTERVAL, FALSE);
			}

			if (result < WAIT_OBJECT_0 + nhandles || result == WAIT_TIMEOUT)
			{
				return result;
			}
			else if (result == WAIT_OBJECT_0 + static_cast<unsigned int> (nhandles))
			{
				this->ProcessMessages();
			}
			else
			{
				CppRaiseException(result == WAIT_FAILED ? GetLastError() : result);
			}
		}
	}

	DWORD GetMinDelay() const
	{
		return IsDebuggerPresent() ? 0 : 750;
	}

public: using Base::IsWindow;
	  HWND GetHWND() const
	  {
		  return this->m_hWnd;
	  }

	  enum
	  {
		  UPDATE_INTERVAL = 1000 / 64
	  };

	  CProgressDialog(ATL::CWindow parent) : Base(true, !!(parent.GetExStyle()& WS_EX_LAYOUTRTL)), canceled(false), invalidated(false), creationTime(GetTickCount64()), lastUpdateTime(0), parent(parent), windowCreated(false), windowCreateAttempted(false), windowShowAttempted(false), lastProgress(0), lastProgressTotal(1) {}

	  ~CProgressDialog()
	  {
		  if (this->windowCreateAttempted)
		  {
			  EnableWindowRecursive(parent, TRUE);
		  }

		  if (this->windowCreated)
		  {
			  this->DestroyWindow();
		  }
	  }

	  unsigned long long Elapsed(unsigned long long
		  const now = GetTickCount64()) const
	  {
		  return now - this->lastUpdateTime;
	  }

	  bool ShouldUpdate(unsigned long long
		  const now = GetTickCount64()) const
	  {
		  return this->Elapsed(now) >= UPDATE_INTERVAL;
	  }

	  void ProcessMessages()
	  {
		  MSG msg;
		  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		  {
			  if (!this->windowCreated || !this->IsDialogMessage(&msg))
			  {
				  TranslateMessage(&msg);
				  DispatchMessage(&msg);
			  }

			  if (msg.message == WM_QUIT)
			  {
				  this->canceled = true;
			  }
		  }
	  }

	  void refresh_marquee()
	  {
		  bool
			  const marquee = this->lastProgressTotal == 0;
		  this->progressBar.ModifyStyle(marquee ? 0 : PBS_MARQUEE, marquee ? PBS_MARQUEE : 0, 0);
		  this->progressBar.SetMarquee(marquee, UPDATE_INTERVAL);
	  }

	  void ForceShow()
	  {
		  if (!this->windowCreateAttempted)
		  {
			  this->windowCreated = !!this->Create(parent);
			  this->windowCreateAttempted = true;
			  EnableWindowRecursive(parent, FALSE);
			  if (this->windowCreated)
			  {
				  this->windowShowAttempted = !!this->IsWindowVisible();
				  this->refresh_marquee();
			  }

			  this->Flush();
		  }

		  if (!this->windowShowAttempted)
		  {
			  this->ShowWindow(SW_SHOW);
			  this->windowShowAttempted = true;
		  }
	  }

	  bool HasUserCancelled(unsigned long long
		  const now = GetTickCount64())
	  {
		  bool justCreated = false;
		  if (abs(static_cast<int> (now - this->creationTime)) >= static_cast<int> (this->GetMinDelay()))
		  {
			  this->ForceShow();
		  }

		  if (this->windowCreated && (justCreated || this->ShouldUpdate(now)))
		  {
			  this->ProcessMessages();
		  }

		  return this->canceled;
	  }

	  void Flush()
	  {
		  if (this->invalidated)
		  {
			  if (this->windowCreated)
			  {
				  this->invalidated = false;
				  this->SetWindowText(this->lastProgressTitle.c_str());
				  if (this->lastProgressTotal == 0)
				  {
					  this->progressBar.StepIt();
				  }
				  else
				  {
					  this->progressBar.SetRange32(0, this->lastProgressTotal);
					  this->progressBar.SetPos(this->lastProgress);
				  }

				  this->progressText.SetWindowText(this->lastProgressText.c_str());
				  this->progressBar.UpdateWindow();
				  this->progressText.UpdateWindow();
				  this->UpdateWindow();
			  }

			  this->lastUpdateTime = GetTickCount64();
			  }
		  }

	  void SetProgress(long long current, long long total)
	  {
		  if (total > INT_MAX)
		  {
			  current = static_cast<long long> ((static_cast<double> (current) / total) * INT_MAX);
			  total = INT_MAX;
		  }

		  this->invalidated |= this->lastProgress != current || this->lastProgressTotal != total;
		  bool
			  const marquee_invalidated = this->invalidated && ((this->lastProgressTotal == 0) ^ (total == 0));
		  this->lastProgressTotal = static_cast<int> (total);
		  this->lastProgress = static_cast<int> (current);
		  if (marquee_invalidated)
		  {
			  this->refresh_marquee();
		  }
	  }

	  void SetProgressTitle(LPCTSTR title)
	  {
		  this->invalidated |= this->lastProgressTitle != title;
		  this->lastProgressTitle = title;
	  }

	  void SetProgressText(std::tvstring
		  const& text)
	  {
		  this->invalidated |= this->lastProgressText.size() != text.size() || !std::equal(this->lastProgressText.begin(), this->lastProgressText.end(), text.begin());
		  this->lastProgressText.assign(text.begin(), text.end());
	  }

	  void SetProgressText(std::tstring
		  const& text)
	  {
		  this->invalidated |= this->lastProgressText.size() != text.size() || !std::equal(this->lastProgressText.begin(), this->lastProgressText.end(), text.begin());
		  this->lastProgressText.assign(text.begin(), text.end());
	  }
	  };

// IoCompletionPort and OleIoCompletionPort classes extracted to src/io/io_completion_port.hpp


// OverlappedNtfsMftReadPayload class extracted to src/io/mft_reader.hpp

std::vector<std::tvstring > get_volume_path_names()
{
	typedef std::tvstring String;
	std::vector<String> result;
	String buf;
	size_t prev;
	do {
		prev = buf.size();
		buf.resize(std::max(static_cast<size_t> (GetLogicalDriveStrings(static_cast<unsigned long> (buf.size()), buf.empty() ? nullptr : &*buf.begin())), buf.size()));
	} while (prev < buf.size());
	for (size_t i = 0, n; n = std::char_traits<TCHAR>::length(&buf[i]), i < buf.size() && buf[i]; i += n + 1)
	{
		result.push_back(String(&buf[i], n));
	}

	return result;
}

#pragma pack(push, 1)
struct SearchResult
{
	typedef unsigned short index_type;
	typedef unsigned short depth_type;
	explicit SearchResult(NtfsIndex::key_type
		const key, depth_type
		const depth) : _key(key), _depth(depth) {}

	NtfsIndex::key_type key() const
	{
		return this->_key;
	}

	depth_type depth() const
	{
		return static_cast<depth_type> (this->_depth);
	}

	NtfsIndex::key_type::index_type index() const
	{
		return this->_key.index();
	}

	void index(NtfsIndex::key_type::index_type
		const value)
	{
		this->_key.index(value);
	}

private:
	NtfsIndex::key_type _key;
	depth_type _depth;
};
#pragma pack(pop)

#ifdef _MSC_VER
__declspec(align(0x40))
#endif

class Results : memheap_vector < SearchResult>
{
	typedef Results this_type;
	typedef memheap_vector<value_type, allocator_type> base_type;
	typedef std::vector< intrusive_ptr < NtfsIndex volatile
		const > > Indexes;
	typedef std::vector<std::pair<Indexes::value_type::value_type*, SearchResult::index_type >> IndicesInUse;
	Indexes indexes /*to keep alive */;
	IndicesInUse indices_in_use /*to keep alive */;
public: typedef base_type::allocator_type allocator_type;
	  typedef base_type::value_type value_type;
	  typedef base_type::reverse_iterator iterator;
	  typedef base_type::const_reverse_iterator const_iterator;
	  typedef base_type::size_type size_type;
	  Results() : base_type() {}

	  explicit Results(allocator_type
		  const& alloc) : base_type(alloc) {}

	  iterator begin()
	  {
		  return this->base_type::rbegin();
	  }

	  const_iterator begin() const
	  {
		  return this->base_type::rbegin();
	  }

	  iterator end()
	  {
		  return this->base_type::rend();
	  }

	  const_iterator end() const
	  {
		  return this->base_type::rend();
	  }

	  using base_type::capacity;
	  size_type size() const
	  {
		  return this->end() - this->begin();
	  }

	  SearchResult::index_type save_index(Indexes::value_type::element_type* const index)
	  {
		  auto j = std::lower_bound(this->indices_in_use.begin(), this->indices_in_use.end(), std::make_pair(index, IndicesInUse::value_type::second_type()));
		  if (j == this->indices_in_use.end() || j->first != index)
		  {
			  this->indexes.push_back(index);
			  j = this->indices_in_use.insert(j, IndicesInUse::value_type(index, static_cast<IndicesInUse::value_type::second_type> (this->indexes.size() - 1)));
		  }

		  return j->second;
	  }

	  Indexes::value_type::element_type* item_index(size_t
		  const i) const
	  {
		  return this->ith_index((*this)[i].index());
	  }

	  Indexes::value_type::element_type* ith_index(value_type::index_type
		  const i) const
	  {
		  return this->indexes[i];
	  }

	  value_type const& operator[](size_t
		  const i) const
	  {
		  return this->begin()[static_cast<ptrdiff_t> (i)];
	  }

	  void reserve(size_t
		  const n)
	  {
		  (void)n;
		  this->base_type::reserve(n);
	  }

	  void push_back(Indexes::value_type::element_type* const index, base_type::const_reference value)
	  {
		  this->base_type::push_back(value);
		  (*(this->base_type::end() - 1)).index(this->save_index(index));
	  }

	  void clear()
	  {
		  this->base_type::clear();
		  this->indexes.clear();
		  this->indices_in_use.clear();
	  }

	  void swap(this_type& other)
	  {
		  this->base_type::swap(static_cast<base_type&> (other));
		  this->indexes.swap(other.indexes);
		  this->indices_in_use.swap(other.indices_in_use);
	  }

	  friend void swap(this_type& a, this_type& b)
	  {
		  return a.swap(b);
	  }
};

long long get_time_zone_bias()
{
	long long ft = 0;
	GetSystemTimeAsFileTime(&reinterpret_cast<FILETIME&> (ft));
	long long ft_local = 0;
	if (!FileTimeToLocalFileTime(&reinterpret_cast<FILETIME&> (ft), &reinterpret_cast<FILETIME&> (ft_local)))
	{
		ft_local = 0;
	}

	return ft_local - ft;
}

template < class Container > struct NFormatBase
{
	typedef basic_iterator_ios<std::back_insert_iterator < Container>, typename Container::traits_type > type;
};

class NFormat : public NFormatBase<std::tstring>::type, public NFormatBase<std::tvstring>::type
{
	typedef NFormat this_type;
public:
	explicit NFormat(std::locale
		const& loc) : NFormatBase<std::tstring>::type(loc), NFormatBase<std::tvstring>::type(loc) {}

	template < class T>
	struct lazy
	{
		this_type
			const* me;
		T
			const* value;
		explicit lazy(this_type
			const* const me, T
			const& value) : me(me), value(&value) {}

		operator std::tstring() const
		{
			std::tstring result;
			me->NFormatBase<std::tstring>::type::put(std::back_inserter(result), *value);
			return result;
		}

		operator std::tvstring() const
		{
			std::tvstring result;
			me->NFormatBase<std::tvstring>::type::put(std::back_inserter(result), *value);
			return result;
		}

		template < class String>
		friend String& operator+=(String& out, lazy
			const& this_)
		{
			this_.me->NFormatBase<String>::type::put(std::back_inserter(out), *this_.value);
			return out;
		}
	};

	template < class T>
	lazy<T> operator()(T
		const& value) const
	{
		return lazy<T>(this, value);
	}
};

extern "C"
IMAGE_DOS_HEADER __ImageBase;

unsigned short get_subsystem(IMAGE_DOS_HEADER
	const* const image_base)
{
	return reinterpret_cast<IMAGE_NT_HEADERS
		const*> (reinterpret_cast<unsigned char
			const*> (image_base) + image_base->e_lfanew)->OptionalHeader.Subsystem;
}

unsigned long long get_version(IMAGE_DOS_HEADER
	const* const image_base)
{
	return reinterpret_cast<IMAGE_NT_HEADERS
		const*> (reinterpret_cast<unsigned char
			const*> (image_base) + image_base->e_lfanew)->FileHeader.TimeDateStamp * 10000000ULL + 0x019db1ded53e8000ULL;
}

struct HookedNtUserProps
{
	typedef HookedNtUserProps hook_type;
	HWND prev_hwnd;
	ATOM prev_atom;
	HANDLE prev_result;
	struct : hook_detail::thread_hook_swap < HOOK_TYPE(NtUserGetProp) >
	{
		HANDLE operator()(HWND hWnd, ATOM PropId) override
		{
			hook_type* const self = CONTAINING_RECORD(this, hook_type, HOOK_CONCAT(hook_, NtUserGetProp));
			if (self->prev_hwnd != hWnd || self->prev_atom != PropId)
			{
				self->prev_result = this->hook_base_type::operator()(hWnd, PropId);
				self->prev_hwnd = hWnd;
				self->prev_atom = PropId;
			}

			return self->prev_result;
		}
	}

	HOOK_CONCAT(hook_, NtUserGetProp);
	struct : hook_detail::thread_hook_swap < HOOK_TYPE(NtUserSetProp) >
	{
		BOOL operator()(HWND hWnd, ATOM PropId, HANDLE value) override
		{
			hook_type* const self = CONTAINING_RECORD(this, hook_type, HOOK_CONCAT(hook_, NtUserSetProp));
			BOOL
				const result = this->hook_base_type::operator()(hWnd, PropId, value);
			if (result && self->prev_hwnd == hWnd && self->prev_atom == PropId)
			{
				self->prev_result = value;
			}

			return result;
		}
	}

	HOOK_CONCAT(hook_, NtUserSetProp);
	HookedNtUserProps() : prev_hwnd(), prev_atom(), prev_result() {}
	~HookedNtUserProps() {}
};


#include "src/gui/main_dialog.hpp"

HMODULE mui_module = nullptr;
WTL::CAppModule _Module;


#if defined(_CPPLIB_VER) && 610 <= _CPPLIB_VER && _CPPLIB_VER < 650
template < class _BidIt,
	class _Diff,
	class _Ty,
	class _Pr > inline
	void My_Stable_sort_unchecked1(_BidIt _First, _BidIt _Last, _Diff _Count,
		std::_Temp_iterator<_Ty>& _Tempbuf, _Pr& _Pred)
{
	// sort preserving order of equivalents, using _Pred
	{
		// sort halves and merge
		_Diff _Count2 = (_Count + 1) / 2;
		_BidIt _Mid = _First;
		_STD advance(_Mid, _Count2);

		if (_Count2 <= _Tempbuf._Maxlen())
		{
			// temp buffer big enough, sort each half using buffer
			std::_Buffered_merge_sort_unchecked(_First, _Mid, _Count2, _Tempbuf, _Pred);
			std::_Buffered_merge_sort_unchecked(_Mid, _Last, _Count - _Count2,
				_Tempbuf, _Pred);
		}
		else
		{
			// temp buffer not big enough, divide and conquer
			My_Stable_sort_unchecked1(_First, _Mid, _Count2, _Tempbuf, _Pred);
			My_Stable_sort_unchecked1(_Mid, _Last, _Count - _Count2, _Tempbuf, _Pred);
		}

		std::_Buffered_merge_unchecked(_First, _Mid, _Last,
			_Count2, _Count - _Count2, _Tempbuf, _Pred);	// merge halves
	}
}
#endif

RefCountedCString get_app_guid()
{
	RefCountedCString guid_str;
	guid_str.Insert(guid_str.GetLength(), _T("{40D41A33-D1FF-4759-9551-0A7201E9F829}"));
	return guid_str;
}

std::pair<int, std::tstring > extract_and_run_if_needed(HINSTANCE hInstance, int argc, TCHAR* const argv[])
{
	int result = -1;
	std::tstring module_path;
	if (argc > 0)
	{
		module_path = argv[0];
	}
	else
	{
		module_path.resize(USHRT_MAX, _T('\0'));
		module_path.resize(GetModuleFileName(hInstance, &module_path[0], static_cast<unsigned int> (module_path.size())));
	}

	if (Wow64::is_wow64() && !string_matcher(string_matcher::pattern_regex, string_matcher::pattern_option_case_insensitive, _T("^.*(?:(?:\\.|_)(?:x86|I386|IA32)|32)(?:\\.[^:/\\\\\\.]+)(?::.*)?$")).is_match(module_path.data(), module_path.size()))
	{
		if (!IsDebuggerPresent())
		{
			HRSRC hRsrs = nullptr;
			WORD langs[] = { GetUserDefaultUILanguage(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT), MAKELANGID(LANG_INVARIANT, SUBLANG_NEUTRAL)
			};

			for (size_t ilang = 0; ilang < sizeof(langs) / sizeof(*langs) && !hRsrs; ++ilang)
			{
				hRsrs = FindResourceEx(nullptr, _T("BINARY"), _T("AMD64"), langs[ilang]);
			}

			HGLOBAL hResource = LoadResource(nullptr, hRsrs);
			LPVOID pBinary = hResource ? LockResource(hResource) : nullptr;
			if (pBinary)
			{
				std::tstring tempDir(32 * 1024, _T('\0'));
				tempDir.resize(GetTempPath(static_cast<DWORD> (tempDir.size()), &tempDir[0]));
				if (!tempDir.empty())
				{
					std::tstring fileName;
					struct Deleter
					{
						std::tstring file;
						~Deleter()
						{
							if (!this->file.empty())
							{
								_tunlink(this->file.c_str());
							}
						}
					}

					deleter;
					bool success = false;
					for (int pass = 0; !success && pass < 2; ++pass)
					{
						if (pass)
						{
							std::tstring
								const module_file_name(basename(module_path.begin(), module_path.end()), module_path.end());
							fileName.assign(module_file_name.begin(), fileext(module_file_name.begin(), module_file_name.end()));
							fileName.insert(fileName.begin(), tempDir.begin(), tempDir.end());
							TCHAR tempbuf[10];
							fileName.append(_itot(64, tempbuf, 10));
							fileName.append(_T("_"));
							fileName.append(get_app_guid());
							fileName.append(fileext(module_file_name.begin(), module_file_name.end()), module_file_name.end());
						}
						else
						{
							fileName = module_path;
							fileName.append(_T(":"));
							TCHAR tempbuf[10];
							fileName.append(_itot(64, tempbuf, 10));
							fileName.append(_T("_"));
							fileName.append(get_app_guid());
						}

						std::filebuf file;
						std::ios_base::openmode
							const openmode = std::ios_base::out | std::ios_base::trunc | std::ios_base::binary; 
#if defined(_CPPLIB_VER)
								success = !!file.open(fileName.c_str(), openmode); 
#else
								std::string fileNameChars;
						std::copy(fileName.begin(), fileName.end(), std::inserter(fileNameChars, fileNameChars.end()));
						success = !!file.open(fileNameChars.c_str(), openmode); 
#endif
							if (success)
							{
								deleter.file = fileName;
								file.sputn(static_cast<char
									const*> (pBinary), static_cast<std::streamsize> (SizeofResource(nullptr, hRsrs)));
								file.close();
							}
					}

					if (success)
					{
						STARTUPINFO si = { sizeof(si)
						};

						GetStartupInfo(&si);
						PROCESS_INFORMATION pi;
						HANDLE hJob = CreateJobObject(nullptr, nullptr);
						JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobLimits = {
		{ 					{ 						0
								},
								{ 0 }, JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
							}
						};

						if (hJob != nullptr &&
							SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobLimits, sizeof(jobLimits)) &&
							AssignProcessToJobObject(hJob, GetCurrentProcess()))
						{
							if (CreateProcess(fileName.c_str(), GetCommandLine(), nullptr, nullptr, FALSE, CREATE_PRESERVE_CODE_AUTHZ_LEVEL | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi))
							{
								jobLimits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
								SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jobLimits, sizeof(jobLimits));
								if (ResumeThread(pi.hThread) != -1)
								{
									WaitForSingleObject(pi.hProcess, INFINITE);
									DWORD exitCode = 0;
									GetExitCodeProcess(pi.hProcess, &exitCode);
									result = static_cast<int> (exitCode);
								}
								else
								{
									TerminateProcess(pi.hProcess, GetLastError());
								}
							}
						}
					}
				}
			}

			/*continue running in x86 mode... */
		}
	}

	return std::make_pair(result, module_path);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

std::wstring_convert<std::codecvt_utf8_utf16 < wchar_t>> converter;
//std::string narrow = converter.to_bytes(wide_utf16_source_string);
//std::wstring wide = converter.from_bytes(narrow_utf8_source_string);

std::string PACKAGE_VERSION = "0.9.6";

// PrintVersion for CLI11 (called from CommandLineParser)
void PrintVersion()
{
	std::cout << "Ultra Fast File Search \t https://github.com/githubrobbi/Ultra-Fast-File-Search\n\nbased on SwiftSearch \t https://sourceforge.net/projects/swiftsearch/\n\n";
	std::cout << "\tUFFS" << " version:\t" << PACKAGE_VERSION << '\n';
	std::cout << "\tBuild for:\t" << "x86_64-pc-windows-msvc" << '\n';
	std::cout << "\n";
	std::cout << "\tOptimized build";
	std::cout << "\n\n";
}

// Buffer length
DWORD maxdrives = 250;
// Buffer for drive string storage
char lpBuffer[100];
std::string drivenames(void)
{
	std::string drives = "";

	DWORD test;

	int i;

	test = GetLogicalDriveStringsW(maxdrives, (LPWSTR)lpBuffer);

	if (test != 0)

	{
		//printf("GetLogicalDriveStrings() return value: %d, Error (if any): %d \n", test, GetLastError());

		//printf("The logical drives of this machine are:\n");

		//if (GetLastError() != 0) printf("Trying to find all physical DISKS failed!!! Error code: %d\n", GetLastError());

		// Check up to 100 drives...

		for (i = 0; i < 100; i++)

			drives += lpBuffer[i];
	}
	else

		//printf("GetLogicalDriveStrings() is failed lor!!! Error code: %d\n", GetLastError());
		printf("Trying to find all physical DISKS failed!!! Error code: %lu\n", GetLastError());

	return drives;

}


void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();	// In case 'to' contains 'from', like replacing 'x' with 'yx' 
	}

}

// Function to remove all spaces from a given string 
std::string removeSpaces(std::string str)
{
	str.erase(remove(str.begin(), str.end(), '\0'), str.end());
	return str;
}

//bool tvreplace(std::tvstring& str, const std::tvstring& from, const std::tvstring& to) {
//	size_t start_pos = str.find(from);
//	if (start_pos == std::tvstring::npos)
//		return false;
//	str.replace(start_pos, from.length(), to);
//	return true;
//}

std::wstring s2ws(const std::string & s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	std::wstring r(len, L'\0');
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
	return r;
}

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString()
{
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return std::string();	//No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ============================================================================
// Raw MFT Dump Implementation (UFFS-MFT format)
// See: mft-reader-rs/CPP_RAW_MFT_DUMP_TOOL_SPEC.md
// ============================================================================

// UFFS-MFT Header structure (64 bytes)
#pragma pack(push, 1)
struct UffsMftHeader {
    char magic[8];           // "UFFS-MFT"
    uint32_t version;        // 1
    uint32_t flags;          // 0 = no compression
    uint32_t record_size;    // e.g., 1024
    uint64_t record_count;   // number of MFT records
    uint64_t original_size;  // total bytes = record_size * record_count
    uint64_t compressed_size;// 0 for uncompressed
    uint8_t reserved[20];    // padding to 64 bytes
};
#pragma pack(pop)

static_assert(sizeof(UffsMftHeader) == 64, "UffsMftHeader must be exactly 64 bytes");

// Dump raw MFT to file in UFFS-MFT format
// Returns 0 on success, error code on failure
int dump_raw_mft(char drive_letter, const char* output_path, std::ostream& OS)
{
    OS << "\n=== Raw MFT Dump Tool ===\n";
    OS << "Drive: " << drive_letter << ":\n";
    OS << "Output: " << output_path << "\n\n";

    // Build volume path: \\.\X:
    std::wstring volume_path = L"\\\\.\\";
    volume_path += static_cast<wchar_t>(toupper(drive_letter));
    volume_path += L":";

    // Open volume handle
    HANDLE volume_handle = CreateFileW(
        volume_path.c_str(),
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        nullptr
    );

    if (volume_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        OS << "ERROR: Failed to open volume " << drive_letter << ": (error " << err << ")\n";
        OS << "Make sure you are running as Administrator.\n";
        return static_cast<int>(err);
    }

    // Get NTFS volume data
    NTFS_VOLUME_DATA_BUFFER volume_data = {};
    DWORD bytes_returned = 0;

    if (!DeviceIoControl(
        volume_handle,
        FSCTL_GET_NTFS_VOLUME_DATA,
        nullptr, 0,
        &volume_data, sizeof(volume_data),
        &bytes_returned,
        nullptr
    )) {
        DWORD err = GetLastError();
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to get NTFS volume data (error " << err << ")\n";
        return static_cast<int>(err);
    }

    OS << "Volume Information:\n";
    OS << "  BytesPerSector: " << volume_data.BytesPerSector << "\n";
    OS << "  BytesPerCluster: " << volume_data.BytesPerCluster << "\n";
    OS << "  BytesPerFileRecordSegment: " << volume_data.BytesPerFileRecordSegment << "\n";
    OS << "  MftValidDataLength: " << volume_data.MftValidDataLength.QuadPart << "\n";
    OS << "  MftStartLcn: " << volume_data.MftStartLcn.QuadPart << "\n\n";

    // Build $MFT path for retrieval pointers
    std::wstring mft_path = L"\\\\.\\";
    mft_path += static_cast<wchar_t>(toupper(drive_letter));
    mft_path += L":\\$MFT";

    // Get MFT extents using get_retrieval_pointers
    long long mft_size = 0;
    std::vector<std::pair<unsigned long long, long long>> ret_ptrs;

    try {
        std::tstring mft_path_t;
        mft_path_t = drive_letter;
        mft_path_t += _T(":\\$MFT");
        ret_ptrs = get_retrieval_pointers(mft_path_t.c_str(), &mft_size,
            volume_data.MftStartLcn.QuadPart, volume_data.BytesPerFileRecordSegment);
    } catch (...) {
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to get MFT retrieval pointers\n";
        return ERROR_READ_FAULT;
    }

    if (ret_ptrs.empty()) {
        CloseHandle(volume_handle);
        OS << "ERROR: No MFT extents found\n";
        return ERROR_READ_FAULT;
    }

    OS << "MFT Extents: " << ret_ptrs.size() << "\n";
    OS << "MFT Size: " << mft_size << " bytes\n";

    // Calculate record count
    uint32_t record_size = volume_data.BytesPerFileRecordSegment;
    uint64_t record_count = static_cast<uint64_t>(mft_size) / record_size;
    uint64_t total_bytes = record_count * record_size;

    OS << "Record Size: " << record_size << " bytes\n";
    OS << "Record Count: " << record_count << "\n";
    OS << "Total Bytes to Write: " << total_bytes << "\n\n";

    // Open output file
    HANDLE out_handle = CreateFileA(
        output_path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (out_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to create output file (error " << err << ")\n";
        return static_cast<int>(err);
    }

    // Write UFFS-MFT header
    UffsMftHeader header = {};
    memcpy(header.magic, "UFFS-MFT", 8);
    header.version = 1;
    header.flags = 0;  // No compression
    header.record_size = record_size;
    header.record_count = record_count;
    header.original_size = total_bytes;
    header.compressed_size = 0;
    memset(header.reserved, 0, sizeof(header.reserved));

    DWORD written = 0;
    if (!WriteFile(out_handle, &header, sizeof(header), &written, nullptr) || written != sizeof(header)) {
        DWORD err = GetLastError();
        CloseHandle(out_handle);
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to write header (error " << err << ")\n";
        return static_cast<int>(err);
    }

    OS << "Reading MFT data...\n";

    // Read MFT data extent by extent
    uint64_t bytes_written = 0;
    uint64_t cluster_size = volume_data.BytesPerCluster;

    // Allocate aligned read buffer (must be sector-aligned for FILE_FLAG_NO_BUFFERING)
    size_t buffer_size = 1024 * 1024;  // 1MB buffer
    // Align to sector size
    buffer_size = (buffer_size / volume_data.BytesPerSector) * volume_data.BytesPerSector;
    std::vector<unsigned char> read_buffer(buffer_size);

    unsigned long long prev_vcn = 0;
    for (size_t i = 0; i < ret_ptrs.size(); ++i) {
        unsigned long long next_vcn = ret_ptrs[i].first;
        long long lcn = ret_ptrs[i].second;

        // Calculate cluster count for this extent
        unsigned long long cluster_count = next_vcn - prev_vcn;
        if (cluster_count == 0) continue;

        // Calculate byte offset and length
        long long byte_offset = lcn * static_cast<long long>(cluster_size);
        unsigned long long byte_length = cluster_count * cluster_size;

        // Read in chunks
        unsigned long long extent_bytes_read = 0;
        while (extent_bytes_read < byte_length && bytes_written < total_bytes) {
            // Seek to position
            LARGE_INTEGER seek_pos;
            seek_pos.QuadPart = byte_offset + static_cast<long long>(extent_bytes_read);
            if (!SetFilePointerEx(volume_handle, seek_pos, nullptr, FILE_BEGIN)) {
                DWORD err = GetLastError();
                CloseHandle(out_handle);
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to seek (error " << err << ")\n";
                return static_cast<int>(err);
            }

            // Calculate how much to read
            unsigned long long remaining_in_extent = byte_length - extent_bytes_read;
            unsigned long long remaining_total = total_bytes - bytes_written;
            size_t to_read = static_cast<size_t>(std::min({
                static_cast<unsigned long long>(buffer_size),
                remaining_in_extent,
                remaining_total
            }));
            // Align to sector size for reading
            to_read = (to_read / volume_data.BytesPerSector) * volume_data.BytesPerSector;
            if (to_read == 0) to_read = volume_data.BytesPerSector;

            DWORD bytes_read = 0;
            if (!ReadFile(volume_handle, read_buffer.data(), static_cast<DWORD>(to_read), &bytes_read, nullptr)) {
                DWORD err = GetLastError();
                CloseHandle(out_handle);
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to read from volume (error " << err << ")\n";
                return static_cast<int>(err);
            }

            // Write to output (may need to trim to not exceed total_bytes)
            size_t to_write = static_cast<size_t>(std::min(
                static_cast<unsigned long long>(bytes_read),
                total_bytes - bytes_written
            ));

            DWORD out_written = 0;
            if (!WriteFile(out_handle, read_buffer.data(), static_cast<DWORD>(to_write), &out_written, nullptr)) {
                DWORD err = GetLastError();
                CloseHandle(out_handle);
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to write to output (error " << err << ")\n";
                return static_cast<int>(err);
            }

            bytes_written += out_written;
            extent_bytes_read += bytes_read;

            // Progress indicator
            if ((bytes_written % (100 * 1024 * 1024)) == 0) {
                OS << "  Progress: " << (bytes_written / (1024 * 1024)) << " MB / "
                   << (total_bytes / (1024 * 1024)) << " MB\n";
            }
        }

        prev_vcn = next_vcn;
    }

    CloseHandle(out_handle);
    CloseHandle(volume_handle);

    OS << "\n=== Dump Complete ===\n";
    OS << "Total extents: " << ret_ptrs.size() << "\n";
    OS << "Total bytes written: " << bytes_written << "\n";
    OS << "Record count: " << record_count << "\n";
    OS << "Output file: " << output_path << "\n";

    return 0;
}

// ============================================================================
// MFT Extent Diagnostic Tool Implementation
// ============================================================================

// Dump MFT extents as JSON for diagnostic purposes
// Returns 0 on success, error code on failure
int dump_mft_extents(char drive_letter, const char* output_path, bool verify_extents, std::ostream& OS)
{
    // Get current timestamp in ISO 8601 format
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm tm_now;
    gmtime_s(&tm_now, &time_t_now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &tm_now);

    // Build volume path: \\.\X:
    std::wstring volume_path = L"\\\\.\\";
    volume_path += static_cast<wchar_t>(toupper(drive_letter));
    volume_path += L":";

    // Open volume handle
    HANDLE volume_handle = CreateFileW(
        volume_path.c_str(),
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        nullptr
    );

    if (volume_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        OS << "{\"error\": \"Failed to open volume " << drive_letter << ": (error " << err << ")\"}\n";
        return static_cast<int>(err);
    }

    // Get NTFS volume data
    NTFS_VOLUME_DATA_BUFFER volume_data = {};
    DWORD bytes_returned = 0;

    if (!DeviceIoControl(
        volume_handle,
        FSCTL_GET_NTFS_VOLUME_DATA,
        nullptr, 0,
        &volume_data, sizeof(volume_data),
        &bytes_returned,
        nullptr
    )) {
        DWORD err = GetLastError();
        CloseHandle(volume_handle);
        OS << "{\"error\": \"Failed to get NTFS volume data (error " << err << ")\"}\n";
        return static_cast<int>(err);
    }

    // Get MFT extents using get_retrieval_pointers
    long long mft_size = 0;
    std::vector<std::pair<unsigned long long, long long>> ret_ptrs;

    try {
        std::tstring mft_path_t;
        mft_path_t = drive_letter;
        mft_path_t += _T(":\\$MFT");
        ret_ptrs = get_retrieval_pointers(mft_path_t.c_str(), &mft_size,
            volume_data.MftStartLcn.QuadPart, volume_data.BytesPerFileRecordSegment);
    } catch (...) {
        CloseHandle(volume_handle);
        OS << "{\"error\": \"Failed to get MFT retrieval pointers\"}\n";
        return ERROR_READ_FAULT;
    }

    if (ret_ptrs.empty()) {
        CloseHandle(volume_handle);
        OS << "{\"error\": \"No MFT extents found\"}\n";
        return ERROR_READ_FAULT;
    }

    uint64_t bytes_per_cluster = volume_data.BytesPerCluster;
    uint32_t record_size = volume_data.BytesPerFileRecordSegment;
    uint64_t records_per_cluster = bytes_per_cluster / record_size;

    // Build JSON output
    std::ostringstream json;
    json << "{\n";
    json << "  \"drive\": \"" << static_cast<char>(toupper(drive_letter)) << "\",\n";
    json << "  \"timestamp\": \"" << timestamp << "\",\n";
    json << "  \"volume_info\": {\n";
    json << "    \"bytes_per_sector\": " << volume_data.BytesPerSector << ",\n";
    json << "    \"bytes_per_cluster\": " << bytes_per_cluster << ",\n";
    json << "    \"bytes_per_file_record\": " << record_size << ",\n";
    json << "    \"mft_start_lcn\": " << volume_data.MftStartLcn.QuadPart << ",\n";
    json << "    \"mft_valid_data_length\": " << volume_data.MftValidDataLength.QuadPart << ",\n";
    json << "    \"total_clusters\": " << volume_data.TotalClusters.QuadPart << "\n";
    json << "  },\n";
    json << "  \"mft_extents\": [\n";

    uint64_t total_clusters = 0;
    uint64_t total_records = 0;
    uint64_t prev_vcn = 0;

    // Allocate buffer for verification reads (one cluster)
    std::vector<uint8_t> verify_buffer;
    if (verify_extents) {
        verify_buffer.resize(static_cast<size_t>(bytes_per_cluster));
    }

    for (size_t i = 0; i < ret_ptrs.size(); ++i) {
        uint64_t next_vcn = ret_ptrs[i].first;
        int64_t lcn = ret_ptrs[i].second;
        uint64_t cluster_count = next_vcn - prev_vcn;
        uint64_t start_frs = prev_vcn * records_per_cluster;
        uint64_t end_frs = start_frs + (cluster_count * records_per_cluster) - 1;
        uint64_t byte_offset = static_cast<uint64_t>(lcn) * bytes_per_cluster;
        uint64_t byte_length = cluster_count * bytes_per_cluster;

        total_clusters += cluster_count;
        total_records = end_frs + 1;

        json << "    {\n";
        json << "      \"index\": " << i << ",\n";
        json << "      \"vcn\": " << prev_vcn << ",\n";
        json << "      \"lcn\": " << lcn << ",\n";
        json << "      \"cluster_count\": " << cluster_count << ",\n";
        json << "      \"start_frs\": " << start_frs << ",\n";
        json << "      \"end_frs\": " << end_frs << ",\n";
        json << "      \"byte_offset\": " << byte_offset << ",\n";
        json << "      \"byte_length\": " << byte_length;

        // Verification: read first record from this extent and check FRS number
        if (verify_extents && lcn >= 0) {
            LARGE_INTEGER seek_pos;
            seek_pos.QuadPart = static_cast<LONGLONG>(byte_offset);

            if (SetFilePointerEx(volume_handle, seek_pos, nullptr, FILE_BEGIN)) {
                DWORD bytes_read = 0;
                if (ReadFile(volume_handle, verify_buffer.data(),
                    static_cast<DWORD>(bytes_per_cluster), &bytes_read, nullptr) && bytes_read >= record_size) {
                    // Check FILE signature and extract FRS number from record header
                    // MFT record header: offset 0x2C (44) contains the FRS number (48-bit)
                    bool valid_signature = (verify_buffer[0] == 'F' && verify_buffer[1] == 'I' &&
                                           verify_buffer[2] == 'L' && verify_buffer[3] == 'E');
                    uint64_t header_frs = 0;
                    if (record_size >= 48) {
                        // FRS is at offset 44 (0x2C), 6 bytes (48-bit) in little-endian
                        header_frs = static_cast<uint64_t>(verify_buffer[44]) |
                                    (static_cast<uint64_t>(verify_buffer[45]) << 8) |
                                    (static_cast<uint64_t>(verify_buffer[46]) << 16) |
                                    (static_cast<uint64_t>(verify_buffer[47]) << 24) |
                                    (static_cast<uint64_t>(verify_buffer[48]) << 32) |
                                    (static_cast<uint64_t>(verify_buffer[49]) << 40);
                    }
                    json << ",\n      \"verify\": {\n";
                    json << "        \"valid_signature\": " << (valid_signature ? "true" : "false") << ",\n";
                    json << "        \"header_frs\": " << header_frs << ",\n";
                    json << "        \"expected_frs\": " << start_frs << ",\n";
                    json << "        \"match\": " << ((header_frs == start_frs) ? "true" : "false") << "\n";
                    json << "      }";
                } else {
                    json << ",\n      \"verify\": {\"error\": \"read_failed\"}";
                }
            } else {
                json << ",\n      \"verify\": {\"error\": \"seek_failed\"}";
            }
        }

        json << "\n    }";
        if (i < ret_ptrs.size() - 1) {
            json << ",";
        }
        json << "\n";

        prev_vcn = next_vcn;
    }

    json << "  ],\n";
    json << "  \"summary\": {\n";
    json << "    \"extent_count\": " << ret_ptrs.size() << ",\n";
    json << "    \"total_clusters\": " << total_clusters << ",\n";
    json << "    \"total_records\": " << total_records << ",\n";
    json << "    \"total_bytes\": " << (total_clusters * bytes_per_cluster) << ",\n";
    json << "    \"is_fragmented\": " << (ret_ptrs.size() > 1 ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}\n";

    CloseHandle(volume_handle);

    // Output to file or stdout
    std::string json_str = json.str();

    if (output_path && strlen(output_path) > 0) {
        std::ofstream out_file(output_path, std::ios::binary);
        if (!out_file) {
            OS << "{\"error\": \"Failed to create output file: " << output_path << "\"}\n";
            return ERROR_CANNOT_MAKE;
        }
        out_file.write(json_str.c_str(), json_str.size());
        out_file.close();
        OS << "MFT extent data written to: " << output_path << "\n";
        OS << "Extents: " << ret_ptrs.size() << ", Total records: " << total_records << "\n";
    } else {
        // Output to stdout
        OS << json_str;
    }

    return 0;
}

// ============================================================================
// End MFT Extent Diagnostic Tool Implementation
// ============================================================================

// ============================================================================
// MFT Read Benchmark Tool Implementation
// ============================================================================

// Benchmark raw MFT reading speed (read-only, no output file)
// Returns 0 on success, error code on failure
int benchmark_mft_read(char drive_letter, std::ostream& OS)
{
    OS << "\n=== MFT Read Benchmark Tool ===\n";
    OS << "Drive: " << drive_letter << ":\n\n";

    // Build volume path: \\.\X:
    std::wstring volume_path = L"\\\\.\\";
    volume_path += static_cast<wchar_t>(toupper(drive_letter));
    volume_path += L":";

    // Open volume handle
    HANDLE volume_handle = CreateFileW(
        volume_path.c_str(),
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        nullptr
    );

    if (volume_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        OS << "ERROR: Failed to open volume " << drive_letter << ": (error " << err << ")\n";
        OS << "Make sure you are running as Administrator.\n";
        return static_cast<int>(err);
    }

    // Get NTFS volume data
    NTFS_VOLUME_DATA_BUFFER volume_data = {};
    DWORD bytes_returned = 0;

    if (!DeviceIoControl(
        volume_handle,
        FSCTL_GET_NTFS_VOLUME_DATA,
        nullptr, 0,
        &volume_data, sizeof(volume_data),
        &bytes_returned,
        nullptr
    )) {
        DWORD err = GetLastError();
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to get NTFS volume data (error " << err << ")\n";
        return static_cast<int>(err);
    }

    OS << "Volume Information:\n";
    OS << "  BytesPerSector: " << volume_data.BytesPerSector << "\n";
    OS << "  BytesPerCluster: " << volume_data.BytesPerCluster << "\n";
    OS << "  BytesPerFileRecordSegment: " << volume_data.BytesPerFileRecordSegment << "\n";
    OS << "  MftValidDataLength: " << volume_data.MftValidDataLength.QuadPart << "\n";
    OS << "  MftStartLcn: " << volume_data.MftStartLcn.QuadPart << "\n\n";

    // Get MFT extents using get_retrieval_pointers
    long long mft_size = 0;
    std::vector<std::pair<unsigned long long, long long>> ret_ptrs;

    try {
        std::tstring mft_path_t;
        mft_path_t = drive_letter;
        mft_path_t += _T(":\\$MFT");
        ret_ptrs = get_retrieval_pointers(mft_path_t.c_str(), &mft_size,
            volume_data.MftStartLcn.QuadPart, volume_data.BytesPerFileRecordSegment);
    } catch (...) {
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to get MFT retrieval pointers\n";
        return ERROR_READ_FAULT;
    }

    if (ret_ptrs.empty()) {
        CloseHandle(volume_handle);
        OS << "ERROR: No MFT extents found\n";
        return ERROR_READ_FAULT;
    }

    // Calculate sizes
    uint32_t record_size = volume_data.BytesPerFileRecordSegment;
    uint64_t record_count = static_cast<uint64_t>(mft_size) / record_size;
    uint64_t total_bytes = record_count * record_size;
    uint64_t cluster_size = volume_data.BytesPerCluster;

    OS << "MFT Information:\n";
    OS << "  Extents: " << ret_ptrs.size() << "\n";
    OS << "  MFT Size: " << mft_size << " bytes (" << (mft_size / (1024 * 1024)) << " MB)\n";
    OS << "  Record Size: " << record_size << " bytes\n";
    OS << "  Record Count: " << record_count << "\n";
    OS << "  Total Bytes to Read: " << total_bytes << "\n\n";

    // Allocate aligned read buffer (must be sector-aligned for FILE_FLAG_NO_BUFFERING)
    size_t buffer_size = 1024 * 1024;  // 1MB buffer
    buffer_size = (buffer_size / volume_data.BytesPerSector) * volume_data.BytesPerSector;
    std::vector<unsigned char> read_buffer(buffer_size);

    // Variables to capture first and last 4 bytes
    unsigned char first_4_bytes[4] = {0, 0, 0, 0};
    unsigned char last_4_bytes[4] = {0, 0, 0, 0};
    bool captured_first = false;

    OS << "Starting MFT read benchmark...\n";
    OS.flush();

    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();

    // Read MFT data extent by extent
    uint64_t bytes_read_total = 0;
    unsigned long long prev_vcn = 0;

    for (size_t i = 0; i < ret_ptrs.size(); ++i) {
        unsigned long long next_vcn = ret_ptrs[i].first;
        long long lcn = ret_ptrs[i].second;

        // Calculate cluster count for this extent
        unsigned long long cluster_count = next_vcn - prev_vcn;
        if (cluster_count == 0) continue;

        // Calculate byte offset and length
        long long byte_offset = lcn * static_cast<long long>(cluster_size);
        unsigned long long byte_length = cluster_count * cluster_size;

        // Read in chunks
        unsigned long long extent_bytes_read = 0;
        while (extent_bytes_read < byte_length && bytes_read_total < total_bytes) {
            // Seek to position
            LARGE_INTEGER seek_pos;
            seek_pos.QuadPart = byte_offset + static_cast<long long>(extent_bytes_read);
            if (!SetFilePointerEx(volume_handle, seek_pos, nullptr, FILE_BEGIN)) {
                DWORD err = GetLastError();
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to seek (error " << err << ")\n";
                return static_cast<int>(err);
            }

            // Calculate how much to read
            unsigned long long remaining_in_extent = byte_length - extent_bytes_read;
            unsigned long long remaining_total = total_bytes - bytes_read_total;
            size_t to_read = static_cast<size_t>(std::min({
                static_cast<unsigned long long>(buffer_size),
                remaining_in_extent,
                remaining_total
            }));
            // Align to sector size for reading
            to_read = (to_read / volume_data.BytesPerSector) * volume_data.BytesPerSector;
            if (to_read == 0) to_read = volume_data.BytesPerSector;

            DWORD bytes_read = 0;
            if (!ReadFile(volume_handle, read_buffer.data(), static_cast<DWORD>(to_read), &bytes_read, nullptr)) {
                DWORD err = GetLastError();
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to read from volume (error " << err << ")\n";
                return static_cast<int>(err);
            }

            // Capture first 4 bytes (from very first read)
            if (!captured_first && bytes_read >= 4) {
                memcpy(first_4_bytes, read_buffer.data(), 4);
                captured_first = true;
            }

            // Always update last 4 bytes (from the actual MFT data portion)
            size_t actual_data_in_buffer = static_cast<size_t>(std::min(
                static_cast<unsigned long long>(bytes_read),
                total_bytes - bytes_read_total
            ));
            if (actual_data_in_buffer >= 4) {
                memcpy(last_4_bytes, read_buffer.data() + actual_data_in_buffer - 4, 4);
            }

            bytes_read_total += actual_data_in_buffer;
            extent_bytes_read += bytes_read;
        }

        prev_vcn = next_vcn;
    }

    // Stop timing
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double seconds = static_cast<double>(duration.count()) / 1000.0;
    double mb_per_sec = (seconds > 0) ? (bytes_read_total / (1024.0 * 1024.0)) / seconds : 0;

    CloseHandle(volume_handle);

    // Output results
    OS << "\n=== Benchmark Results ===\n";
    OS << "Total bytes read: " << bytes_read_total << " (" << (bytes_read_total / (1024 * 1024)) << " MB)\n";
    OS << "Total records: " << record_count << "\n";
    OS << "Time elapsed: " << duration.count() << " ms (" << std::fixed << std::setprecision(3) << seconds << " seconds)\n";
    OS << "Read speed: " << std::fixed << std::setprecision(2) << mb_per_sec << " MB/s\n\n";

    // Proof of reading - first and last 4 bytes
    OS << "=== Proof of Complete Read ===\n";
    OS << "First 4 bytes (hex): ";
    for (int i = 0; i < 4; ++i) {
        OS << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(first_4_bytes[i]);
        if (i < 3) OS << " ";
    }
    OS << std::dec;  // Reset to decimal
    OS << "  (ASCII: ";
    for (int i = 0; i < 4; ++i) {
        char c = static_cast<char>(first_4_bytes[i]);
        OS << (isprint(c) ? c : '.');
    }
    OS << ")\n";

    OS << "Last 4 bytes (hex):  ";
    for (int i = 0; i < 4; ++i) {
        OS << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(last_4_bytes[i]);
        if (i < 3) OS << " ";
    }
    OS << std::dec;  // Reset to decimal
    OS << "  (ASCII: ";
    for (int i = 0; i < 4; ++i) {
        char c = static_cast<char>(last_4_bytes[i]);
        OS << (isprint(c) ? c : '.');
    }
    OS << ")\n";

    // Note about expected values
    OS << "\nNote: First 4 bytes should be 'FILE' (46 49 4C 45) - the MFT record signature.\n";

    return 0;
}

// ============================================================================
// End MFT Read Benchmark Tool Implementation
// ============================================================================

// ============================================================================
// Index Build Benchmark Tool Implementation
// ============================================================================

// Benchmark full index building (read + parse + build) using the real UFFS async pipeline
// Returns 0 on success, error code on failure
int benchmark_index_build(char drive_letter, std::ostream& OS)
{
    OS << "\n=== Index Build Benchmark Tool ===\n";
    OS << "Drive: " << drive_letter << ":\n";
    OS << "This measures the full UFFS indexing pipeline (async I/O + parsing + index building)\n\n";

    // Build path name (e.g., "C:\")
    TCHAR path_buf[4] = { static_cast<TCHAR>(toupper(drive_letter)), _T(':'), _T('\\'), _T('\0') };
    std::tvstring path_name(path_buf);

    OS << "Creating index for " << static_cast<char>(toupper(drive_letter)) << ":\\ ...\n";
    OS.flush();

    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();
    clock_t tbegin = clock();

    // Create the index
    intrusive_ptr<NtfsIndex> index(new NtfsIndex(path_name), true);

    // Create IOCP and closing event
    IoCompletionPort iocp;
    Handle closing_event;

    // Post the read payload to start async indexing
    // Note: Volume is opened by the payload's operator() when IOCP processes it
    typedef OverlappedNtfsMftReadPayload T;
    intrusive_ptr<T> payload(new T(iocp, index, closing_event));
    iocp.post(0, 0, payload);

    // Wait for indexing to complete
    OS << "Indexing in progress...\n";
    OS.flush();

    HANDLE wait_handle = reinterpret_cast<HANDLE>(index->finished_event());
    DWORD wait_result = WaitForSingleObject(wait_handle, INFINITE);

    if (wait_result != WAIT_OBJECT_0) {
        OS << "ERROR: Wait failed (result=" << wait_result << ")\n";
        return ERROR_WAIT_1;
    }

    // Stop timing
    auto end_time = std::chrono::high_resolution_clock::now();
    clock_t tend = clock();

    // Check for indexing errors
    unsigned int task_result = index->get_finished();
    if (task_result != 0) {
        OS << "ERROR: Indexing failed with error code " << task_result << "\n";
        if (task_result == ERROR_ACCESS_DENIED) {
            OS << "Make sure you are running as Administrator.\n";
        } else if (task_result == ERROR_UNRECOGNIZED_VOLUME) {
            OS << "The volume is not NTFS formatted.\n";
        }
        return static_cast<int>(task_result);
    }

    // Calculate timing
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double seconds = static_cast<double>(duration.count()) / 1000.0;
    double clock_seconds = static_cast<double>(tend - tbegin) / CLOCKS_PER_SEC;

    // Get index statistics from public accessors
    size_t total_records = index->records_so_far();
    size_t total_names = index->total_names();
    size_t total_names_and_streams = index->total_names_and_streams();
    unsigned int mft_capacity = index->mft_capacity;
    unsigned int mft_record_size = index->mft_record_size;
    unsigned long long mft_bytes = static_cast<unsigned long long>(mft_capacity) * mft_record_size;

    // Calculate throughput
    double mb_per_sec = (seconds > 0) ? (mft_bytes / (1024.0 * 1024.0)) / seconds : 0;
    double records_per_sec = (seconds > 0) ? static_cast<double>(total_records) / seconds : 0;
    double names_per_sec = (seconds > 0) ? static_cast<double>(total_names) / seconds : 0;

    // Output results
    OS << "\n=== Volume Information ===\n";
    OS << "MFT Capacity: " << mft_capacity << " records\n";
    OS << "MFT Record Size: " << mft_record_size << " bytes\n";
    OS << "MFT Total Size: " << mft_bytes << " bytes (" << (mft_bytes / (1024 * 1024)) << " MB)\n";

    OS << "\n=== Index Statistics ===\n";
    OS << "Records Processed: " << total_records << "\n";
    OS << "Name Entries: " << total_names << "\n";
    OS << "Names + Streams: " << total_names_and_streams << "\n";

    OS << "\n=== Benchmark Results ===\n";
    OS << "Time Elapsed: " << duration.count() << " ms (" << std::fixed << std::setprecision(3) << seconds << " seconds)\n";
    OS << "CPU Time: " << std::fixed << std::setprecision(3) << clock_seconds << " seconds\n";
    OS << "MFT Read Speed: " << std::fixed << std::setprecision(2) << mb_per_sec << " MB/s\n";
    OS << "Record Processing: " << std::fixed << std::setprecision(0) << records_per_sec << " records/sec\n";
    OS << "Name Indexing: " << std::fixed << std::setprecision(0) << names_per_sec << " names/sec\n";

    // Summary line for easy comparison
    OS << "\n=== Summary ===\n";
    OS << "Indexed " << total_names << " names in "
       << std::fixed << std::setprecision(3) << seconds << " seconds\n";

    return 0;
}

// ============================================================================
// End Index Build Benchmark Tool Implementation
// ============================================================================

// ============================================================================
// End Raw MFT Dump Implementation
// ============================================================================

// Command Line Version
//int _tmain(int argc, TCHAR *argv[])
#include "src/cli/cli_main.hpp"

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

//Windows Version
#include "src/gui/gui_main.hpp"