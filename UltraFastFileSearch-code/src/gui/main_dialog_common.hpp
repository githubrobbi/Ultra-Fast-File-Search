#pragma once
// ============================================================================
// main_dialog_common.hpp - Common includes for main_dialog
// ============================================================================
// This header provides all the dependencies needed by main_dialog.hpp and
// main_dialog.cpp. It mirrors the include structure of UltraFastFileSearch.cpp
// up to the point where main_dialog.hpp is included.
//
// IMPORTANT: Include order matters! Headers that depend on namespace aliases
// or using declarations must come AFTER those declarations.
// ============================================================================

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
// Basic Utility Headers (no winnt:: dependency)
// ============================================================================
#include "../util/string_utils.hpp"
#include "../util/volume_utils.hpp"
#include "../util/time_utils.hpp"
#include "../util/nformat_ext.hpp"
#include "../util/utf_convert.hpp"
#include "../search/match_operation.hpp"
#include "../core/ntfs_types.hpp"
#include "../util/atomic_compat.hpp"
#include "../util/intrusive_ptr.hpp"
#include "../util/lock_ptr.hpp"
#include "../io/overlapped.hpp"
#include "../../resource.h"
#include "../util/background_worker.hpp"
#include "modified_dialog_impl.hpp"
#include "../cli/command_line_parser.hpp"
#include "../util/nformat.hpp"
#include "../util/path.hpp"
#include "../util/shell_item_id_list.hpp"
#include "../search/string_matcher.hpp"
#include "../util/handle.hpp"
#include "../io/winnt_types.hpp"
#include "../io/io_priority.hpp"
#include "../util/wow64.hpp"
#include "../util/locale_utils.hpp"
#include "../util/memheap_vector.hpp"
#include "../util/buffer.hpp"
#include "../util/containers.hpp"
#include "../util/temp_swap.hpp"
#include "../util/pe_utils.hpp"
#include "../util/nt_user_call_hook.hpp"
#include "../util/com_init.hpp"
#include "../util/path_utils.hpp"
#include "../util/sort_utils.hpp"
#include "../util/allocators.hpp"
#include "../util/file_handle.hpp"

// ============================================================================
// Namespace aliases (MUST come before headers that use winnt:: prefix)
// ============================================================================
using namespace uffs::winnt;
namespace winnt = uffs::winnt;

// ============================================================================
// Using declarations (MUST come before headers that use these types)
// ============================================================================
using uffs::Handle;
using uffs::IoPriority;
using uffs::get_subsystem;
using uffs::get_version;
using uffs::LCIDToLocaleName_XPCompatible;
using uffs::get_ui_locale_name;

// Path utilities
using uffs::NormalizePath;
using uffs::remove_path_stream_and_trailing_sep;
using uffs::GetDisplayName;

// Sort utilities
using uffs::is_sorted_ex;
using uffs::stable_sort_by_key;

// Allocators
using uffs::SingleMovableGlobalAllocator;

// File handle
using uffs::File;

template <class T, class Alloc = uffs::default_memheap_alloc<T>>
using memheap_vector = uffs::memheap_vector<T, Alloc>;

// ============================================================================
// External declarations (MUST come before headers that use them)
// ============================================================================
extern HMODULE mui_module;
extern WTL::CAppModule _Module;
extern ATL::CWindow topmostWindow;

// Forward declaration for global_exception_handler (used by io_completion_port.hpp)
long global_exception_handler(struct _EXCEPTION_POINTERS* ExceptionInfo);

// ============================================================================
// Headers that depend on namespace aliases and using declarations
// ============================================================================
#include "../util/nt_user_hooks.hpp"
#include "../util/hooked_nt_user_props.hpp"
#include "../index/ntfs_index.hpp"
#include "../io/io_completion_port.hpp"
#include "../io/mft_reader.hpp"
#include "listview_hooks.hpp"
#include "string_loader.hpp"

// Using declarations needed BEFORE progress_dialog.hpp
using uffs::gui::RefCountedCString;
using uffs::gui::StringLoader;

#include "listview_adapter.hpp"
#include "progress_dialog.hpp"
#include "../search/search_results.hpp"

// ============================================================================
// Additional using declarations for GUI components
// ============================================================================
using uffs::gui::CDisableListViewUnnecessaryMessages;
#ifdef WM_SETREDRAW
using uffs::gui::CSetRedraw;
#endif
using uffs::gui::ImageListAdapter;
using uffs::gui::ListViewAdapter;
using uffs::gui::autosize_columns;

