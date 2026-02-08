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
static inline BOOL ILIsEmpty(LPCITEMIDLIST pidl) { return ((pidl == nullptr) || (pidl->mkid.cb == 0)); }
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

// Sort utilities extracted to src/util/sort_utils.hpp
#include "src/util/sort_utils.hpp"
using uffs::is_sorted_ex;
using uffs::stable_sort_by_key_comparator;
using uffs::stable_sort_by_key;


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


// Type traits compatibility extracted to src/util/type_traits_compat.hpp
#include "src/util/type_traits_compat.hpp"

// File RAII wrapper extracted to src/util/file_handle.hpp
#include "src/util/file_handle.hpp"
using uffs::File;

// Hook type declarations are in src/util/nt_user_hooks.hpp
// Here we only define the static _instance members
#include "src/util/nt_user_hooks.hpp"

#define X(Class)(GetProcAddress(GetModuleHandle(TEXT("win32u.dll")), HOOK_TYPE(Class)::static_name()))
template<> HOOK_TYPE(NtUserGetProp) HOOK_IMPLEMENT(NtUserGetProp, HANDLE __stdcall(HWND hWnd, ATOM PropId), X);
template<> HOOK_TYPE(NtUserSetProp) HOOK_IMPLEMENT(NtUserSetProp, BOOL __stdcall(HWND hWnd, ATOM PropId, HANDLE value), X);
template<> HOOK_TYPE(NtUserMessageCall) HOOK_IMPLEMENT(NtUserMessageCall, LRESULT __stdcall(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, ULONG_PTR xParam, DWORD xpfnProc, BOOL bAnsi), X);
template<> HOOK_TYPE(NtUserRedrawWindow) HOOK_IMPLEMENT(NtUserRedrawWindow, BOOL __stdcall(HWND hWnd, CONST RECT * lprcUpdate, HRGN hrgnUpdate, UINT flags), X);
#undef X

// atomic_namespace moved to src/util/atomic_compat.hpp




// ============================================================================
// Exception Handler (extracted to src/util/exception_handler.hpp)
// ============================================================================
// The topmostWindow global is defined here (used by exception_handler.hpp)
ATL::CWindow topmostWindow;

// Include the exception handler implementation
// Note: This provides global_exception_handler() and global_exception_mutex
#include "src/util/exception_handler.hpp"


// is_ascii and DisplayError extracted to src/util/error_utils.hpp

// append_directional extracted to src/util/append_directional.hpp
#include "src/util/append_directional.hpp"

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


// isdevnull functions extracted to src/util/devnull_check.hpp
#include "src/util/devnull_check.hpp"
using uffs::isdevnull;

// NTFS types are now defined in src/core/ntfs_types.hpp
// The ntfs:: namespace is exposed at global scope via type aliases in that header.

// Path utilities extracted to src/util/path_utils.hpp
#include "src/util/path_utils.hpp"
using uffs::remove_path_stream_and_trailing_sep;
using uffs::NormalizePath;
using uffs::GetDisplayName;

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

// get_cluster_size() extracted to src/util/volume_utils.hpp
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

// HookedNtUserProps extracted to src/util/hooked_nt_user_props.hpp
#include "src/util/hooked_nt_user_props.hpp"

#include "src/gui/main_dialog.hpp"

HMODULE mui_module = nullptr;
WTL::CAppModule _Module;


// My_Stable_sort_unchecked1 extracted to src/util/sort_utils.hpp
#if defined(_CPPLIB_VER) && 610 <= _CPPLIB_VER && _CPPLIB_VER < 650
using uffs::My_Stable_sort_unchecked1;
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
// benchmark_index_build extracted to:
//   src/cli/benchmark.hpp
// ============================================================================
#include "src/cli/benchmark.hpp"
// dump_raw_mft, dump_mft_extents, benchmark_mft_read are in mft_diagnostics.cpp

// ============================================================================
// Entry Points - Conditionally compiled based on build configuration
// ============================================================================
// UFFS_CLI_BUILD: Defined for COM configurations (console subsystem)
// UFFS_GUI_BUILD: Defined for EXE configurations (Windows subsystem)
// ============================================================================

#ifdef UFFS_CLI_BUILD
// Command Line Version - main()
#include "src/cli/cli_main.hpp"
#endif

#ifdef UFFS_GUI_BUILD
// Windows Version - _tWinMain()
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include "src/gui/gui_main.hpp"
#endif
