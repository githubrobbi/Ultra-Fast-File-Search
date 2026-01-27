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




// safe_stprintf() extracted to src/util/error_utils.hpp

ATL::CWindow topmostWindow;
atomic_namespace::recursive_mutex global_exception_mutex;

// Note: Not static - needs external linkage for io_completion_port.hpp
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
