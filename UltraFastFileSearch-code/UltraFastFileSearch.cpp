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
// Utility Headers (extracted from monolith)
// ============================================================================
#include "src/util/string_utils.hpp"
#include "src/util/volume_utils.hpp"
#include "src/util/time_utils.hpp"
#include "src/util/nformat_ext.hpp"
#include "src/util/utf_convert.hpp"
#include "src/search/match_operation.hpp"
#include "src/core/ntfs_types.hpp"

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
#include "src/util/core_types.hpp"
#include "src/util/error_utils.hpp"
using uffs::DynamicAllocator;
using uffs::dynamic_allocator;
using uffs::SingleMovableGlobalAllocator;
using uffs::error::safe_stprintf;
using uffs::error::CppRaiseException;
using uffs::error::CheckAndThrow;
using uffs::error::GetAnyErrorText;
using uffs::error::GetLastErrorAsString;

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




// safe_stprintf() extracted to src/util/error_utils.hpp

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


// CppRaiseException() extracted to src/util/error_utils.hpp

// CheckAndThrow() extracted to src/util/error_utils.hpp

// GetAnyErrorText() extracted to src/util/error_utils.hpp

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

// NTFS types are now defined in src/core/ntfs_types.hpp
// The ntfs:: namespace is exposed at global scope via type aliases in that header.

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
			TCHAR value_name[16] = {};
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

// get_retrieval_pointers() extracted to src/util/volume_utils.hpp

// propagate_const and fast_subscript templates extracted to src/index/ntfs_index.hpp

#include "src/util/buffer.hpp"

#include "src/util/containers.hpp"
#include "src/index/ntfs_index.hpp"

// NtfsIndex class has been extracted to src/index/ntfs_index.hpp
// MatchOperation struct has been extracted to src/search/match_operation.hpp

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
		LVCOLUMN col = {};
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
			visible.second = visible.second + static_cast<ptrdiff_t>(feather) <= static_cast<ptrdiff_t> (n) ? visible.second + feather : static_cast<int> (n);
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


// CProgressDialog extracted to src/gui/progress_dialog.hpp
#include "src/util/temp_swap.hpp"
#include "src/gui/progress_dialog.hpp"

// IoCompletionPort and OleIoCompletionPort classes extracted to src/io/io_completion_port.hpp


// OverlappedNtfsMftReadPayload class extracted to src/io/mft_reader.hpp

// get_volume_path_names() extracted to src/util/volume_utils.hpp

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

// get_time_zone_bias() extracted to src/util/time_utils.hpp
// NFormatBase and NFormat extracted to src/util/nformat_ext.hpp

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

// UTF-8/UTF-16 converter extracted to src/util/utf_convert.hpp
// Use: converter.to_bytes(wide) or converter.from_bytes(narrow)

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

// String utilities (drivenames, replaceAll, removeSpaces) moved to src/util/string_utils.cpp

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

// GetLastErrorAsString() extracted to src/util/error_utils.hpp

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ============================================================================
// MFT Diagnostic Tools
// ============================================================================
// dump_raw_mft, dump_mft_extents, benchmark_mft_read extracted to:
//   src/cli/mft_diagnostics.cpp
// benchmark_index_build remains here (depends on NtfsIndex, IoCompletionPort)
// ============================================================================

namespace uffs {

// benchmark_index_build - Benchmark full index building
// This function depends on NtfsIndex and IoCompletionPort which are still
// defined via textual inclusion, so it must remain in the monolith for now.
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
// End MFT Diagnostic Tools
// ============================================================================

} // namespace uffs

// Expose MFT diagnostic functions at global scope for backward compatibility
using uffs::benchmark_index_build;
// dump_raw_mft, dump_mft_extents, benchmark_mft_read are in mft_diagnostics.cpp

// Command Line Version
#include "src/cli/cli_main.hpp"

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

//Windows Version
#include "src/gui/gui_main.hpp"
