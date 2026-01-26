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
#include "src/util/background_worker.hpp"
#include "src/gui/modified_dialog_impl.hpp"
#include "src/cli/command_line_parser.hpp"
#include "src/util/nformat.hpp"
#include "src/util/nt_user_call_hook.hpp"
#include "src/util/path.hpp"
#include "src/util/shell_item_id_list.hpp"
#include "src/search/string_matcher.hpp"
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

// ============================================================================
// WTL Compatibility
// ============================================================================
namespace WTL
{
	using std::min;
	using std::max;
}

#ifndef ILIsEmpty
inline BOOL ILIsEmpty(LPCITEMIDLIST pidl) { return ((pidl == nullptr) || (pidl->mkid.cb == 0)); }
#endif

extern WTL::CAppModule _Module;

// ============================================================================
// Clang Compatibility
// ============================================================================
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


// is_ascii and DisplayError extracted to src/util/error_utils.hpp

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

// Locale utilities extracted to src/util/locale_utils.hpp
#include "src/util/locale_utils.hpp"
using uffs::LCIDToLocaleName_XPCompatible;
using uffs::get_ui_locale_name;

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

// CDisableListViewUnnecessaryMessages and CSetRedraw extracted to src/gui/listview_hooks.hpp
#include "src/gui/listview_hooks.hpp"
using uffs::gui::CDisableListViewUnnecessaryMessages;
#ifdef WM_SETREDRAW
using uffs::gui::CSetRedraw;
#endif

// RefCountedCString and StringLoader extracted to src/gui/string_loader.hpp
#include "src/gui/string_loader.hpp"
using uffs::gui::RefCountedCString;
using uffs::gui::StringLoader;

extern HMODULE mui_module;

// ImageListAdapter, ListViewAdapter, autosize_columns extracted to src/gui/listview_adapter.hpp
#include "src/gui/listview_adapter.hpp"
using uffs::gui::ImageListAdapter;
using uffs::gui::ListViewAdapter;
using uffs::gui::autosize_columns;

// CProgressDialog extracted to src/gui/progress_dialog.hpp
#include "src/util/temp_swap.hpp"
#include "src/gui/progress_dialog.hpp"

// IoCompletionPort and OleIoCompletionPort classes extracted to src/io/io_completion_port.hpp


// OverlappedNtfsMftReadPayload class extracted to src/io/mft_reader.hpp

// get_volume_path_names() extracted to src/util/volume_utils.hpp

// SearchResult and Results extracted to src/search/search_results.hpp
#include "src/search/search_results.hpp"

// get_time_zone_bias() extracted to src/util/time_utils.hpp
// NFormatBase and NFormat extracted to src/util/nformat_ext.hpp

extern "C"
IMAGE_DOS_HEADER __ImageBase;

// PE utilities extracted to src/util/pe_utils.hpp
#include "src/util/pe_utils.hpp"
using uffs::get_subsystem;
using uffs::get_version;

// HookedNtUserProps moved to src/util/nt_user_call_hook.hpp

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

// get_app_guid and extract_and_run_if_needed extracted to src/util/x64_launcher.hpp
#include "src/util/x64_launcher.hpp"
using uffs::get_app_guid;
using uffs::extract_and_run_if_needed;

///////////////////////////////////////////////////////////////////////////////////////////////////////

// UTF-8/UTF-16 converter extracted to src/util/utf_convert.hpp
// Use: converter.to_bytes(wide) or converter.from_bytes(narrow)

// Version info extracted to src/util/version_info.hpp
#include "src/util/version_info.hpp"
std::string PACKAGE_VERSION = uffs::get_package_version();

// PrintVersion for CLI11 (called from CommandLineParser)
void PrintVersion()
{
	uffs::print_version();
}

// String utilities (drivenames, replaceAll, removeSpaces) moved to src/util/string_utils.cpp

// s2ws extracted to src/util/version_info.hpp
using uffs::s2ws;

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
