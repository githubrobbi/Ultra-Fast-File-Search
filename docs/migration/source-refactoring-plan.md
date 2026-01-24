# Source Code Refactoring Plan

> **Target Audience**: Junior Software Engineer
> **Skill Level Required**: Basic C++ knowledge, familiarity with Visual Studio
> **Time Estimate**: 25-40 hours total
> **Risk Level**: Low to Medium (all changes preserve functionality)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current State Analysis](#current-state-analysis)
3. [Pre-Requisites](#pre-requisites)
4. [Phase 1: Cleanup](#phase-1-cleanup-2-hours)
5. [Phase 2: Extract NTFS Types](#phase-2-extract-ntfs-types-3-hours)
6. [Phase 3: Extract Utilities](#phase-3-extract-utilities-3-hours)
7. [Phase 4: Extract I/O Layer](#phase-4-extract-io-layer-6-hours)
8. [Phase 5: Extract NtfsIndex](#phase-5-extract-ntfsindex-6-hours)
9. [Phase 6: Separate GUI/CLI](#phase-6-separate-guicli-8-hours)
10. [Phase 7: Modernize C++](#phase-7-modernize-c-6-hours)
11. [Testing Checklist](#testing-checklist)
12. [Troubleshooting](#troubleshooting)

---

## Executive Summary

This document outlines a comprehensive plan to modernize the UFFS C++ codebase while **preserving 100% functionality**. The goal is to transform a legacy monolithic codebase into a clean, maintainable, modern C++ project.

**What we ARE doing:**
- Splitting large files into smaller, focused modules
- Removing duplicate code
- Organizing includes
- Adding namespaces
- Modernizing C++ style

**What we are NOT doing:**
- Changing any algorithms
- Modifying any functionality
- Changing the build output (uffs.com, uffs.exe)

---

## Implementation Status (as of 2026-01-24)

- Phases 1–5 from this plan have been executed; Phase 6 is partially complete; Phase 7 (Modernize C++) is in progress.
- The main monolithic source file `UltraFastFileSearch.cpp` has been reduced from 14,155 to 11,932 lines (≈2,200 lines migrated into headers under `src/`).
- Key utility and I/O abstractions now live in dedicated headers:
  - `src/util/atomic_compat.hpp` — atomic_namespace (spin locks, atomics, lightweight sync primitives)
  - `src/util/intrusive_ptr.hpp` — `RefCounted` + `intrusive_ptr`
  - `src/util/lock_ptr.hpp` — `lock_ptr` RAII lock wrapper
  - `src/io/overlapped.hpp` — `Overlapped` base class for async I/O
  - `src/io/io_completion_port.hpp` — `IoCompletionPort` / `OleIoCompletionPort`
  - `src/io/mft_reader.hpp` — `OverlappedNtfsMftReadPayload` and related MFT read operations
  - `src/util/containers.hpp` — `vector_with_fast_size` and `Speed` helper
  - `src/util/buffer.hpp` — resizable buffer abstraction used by the indexer
  - `src/util/com_init.hpp` — `CoInit` and `OleInit` RAII COM initialization helpers
  - `src/util/temp_swap.hpp` — `TempSwap<T>` RAII helper for temporary value swaps
  - `src/util/wow64.hpp` — `Wow64` and `Wow64Disable` WOW64 file system redirection helpers
- Phase 7 modernization work applied so far:
  - All `NULL` pointer literals have been replaced with `nullptr`.
  - `auto` is used for several complex iterator types where it improves readability.
  - `[[nodiscard]]` has been added to 5 functions where ignoring the return value would likely be a bug.

For detailed per-phase status and verification notes, see **`refactoring-milestones.md`**.

---

## Current State Analysis

### File Size Issues

| File | Lines | Problem |
|------|-------|---------|
| `UltraFastFileSearch.cpp` | 14,155 | **Massive monolith** - contains everything |
| `file.cpp` | 8,101 | **Duplicate/variant** of main file |
| `CLI11.hpp` | 10,998 | Third-party (OK as-is) |
| `WinDDKFixes.hpp` | 1,616 | Windows compatibility layer |
| Other files | < 800 | Reasonably sized |

### Critical Issues Found

#### Issue 1: Duplicate Includes (UltraFastFileSearch.cpp)

**Location**: Lines 46-69

```cpp
// Lines 46-52 (FIRST occurrence)
#include "nformat.hpp"
#include "path.hpp"
#include "BackgroundWorker.hpp"
#include "ShellItemIDList.hpp"
#include "CModifiedDialogImpl.hpp"
#include "NtUserCallHook.hpp"
#include "string_matcher.hpp"

// ... other includes ...

// Lines 61-70 (DUPLICATE!)
#include "BackgroundWorker.hpp"      // DUPLICATE of line 48
#include "CModifiedDialogImpl.hpp"   // DUPLICATE of line 50
#include "CommandLineParser.hpp"     // OK - only here
#include "nformat.hpp"               // DUPLICATE of line 46
#include "NtUserCallHook.hpp"        // DUPLICATE of line 51
#include "path.hpp"                  // DUPLICATE of line 47
#include "resource.h"                // OK - only here
#include "ShellItemIDList.hpp"       // DUPLICATE of line 49
#include "string_matcher.hpp"        // DUPLICATE of line 52
#include "targetver.h"               // DUPLICATE of line 11
```

#### Issue 2: Hardcoded Path (file.cpp)

**Location**: Line 91

```cpp
#include "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Tools/MSVC/14.16.27023/include/xutility"
```

**Problem**: This will break on any machine without this exact VS version installed.

#### Issue 3: Two Near-Identical Files

`UltraFastFileSearch.cpp` (14,155 lines) and `file.cpp` (8,101 lines) share approximately 80% of their code. Only one should exist.

#### Issue 4: Mixed Concerns in One File

`UltraFastFileSearch.cpp` contains:
- NTFS structures and parsing (lines 1886-2430)
- MFT reading with IOCP (lines 6740-7560)
- Index building (lines 3587-5565)
- Pattern matching (lines 5448-5565)
- GUI/WTL dialogs (lines 7842-11960)
- CLI parsing (lines 12890-14156)
- Utility functions (scattered throughout)

### Classes Found in UltraFastFileSearch.cpp

| Class Name | Line | Purpose | Extract To |
|------------|------|---------|------------|
| `Handle` | 2432 | RAII handle wrapper | `src/util/handle.hpp` |
| `IoPriority` | 2493 | I/O priority management | `src/io/io_priority.hpp` |
| `RefCounted` | 3052 | Reference counting base | `src/util/ref_counted.hpp` |
| `Overlapped` | 3084 | Async I/O base class | `src/io/overlapped.hpp` |
| `NtfsIndex` | 3587 | Core index class | `src/index/ntfs_index.hpp` |
| `IoCompletionPort` | 6740 | IOCP wrapper | `src/io/iocp.hpp` |
| `OverlappedNtfsMftReadPayload` | 7078 | MFT reader | `src/core/mft_reader.hpp` |
| `Results` | 7603 | Search results | `src/search/results.hpp` |
| `CMainDlg` | 7842 | Main GUI dialog | `src/gui/main_dialog.hpp` |

### Structs Found in UltraFastFileSearch.cpp

| Struct Name | Line | Purpose | Extract To |
|-------------|------|---------|------------|
| `NTFS_BOOT_SECTOR` | 1889 | NTFS boot sector | `src/core/ntfs_types.hpp` |
| `MULTI_SECTOR_HEADER` | 1926 | Multi-sector header | `src/core/ntfs_types.hpp` |
| `intrusive_ptr` | 2872 | Smart pointer | `src/util/intrusive_ptr.hpp` |
| `SearchResult` | 7564 | Search result item | `src/search/search_result.hpp` |
| `UffsMftHeader` | 12081 | MFT dump header | `src/core/mft_dump.hpp` |

---

## Pre-Requisites

Before starting any refactoring work, complete these steps:

### 1. Set Up Your Environment

```bash
# Clone the repository
git clone https://github.com/githubrobbi/Ultra-Fast-File-Search.git
cd Ultra-Fast-File-Search

# Create a refactoring branch
git checkout -b refactoring/phase-1-cleanup
```

### 2. Verify the Build Works

Open `UltraFastFileSearch-code/UltraFastFileSearch.sln` in Visual Studio 2019 or later.

```
Build → Build Solution (Ctrl+Shift+B)
```

**Expected output:**
- `uffs.com` (CLI tool)
- `uffs.exe` (GUI tool)

### 3. Create Baseline Test Results

Run these commands and save the output:

```cmd
# Test 1: Benchmark on C: drive
uffs.com --benchmark-index=C > baseline_benchmark.txt

# Test 2: Search test
uffs.com "*.txt" --drives=C > baseline_search.txt

# Test 3: Help output
uffs.com --help > baseline_help.txt
```

**IMPORTANT**: After EVERY change, re-run these tests and compare output. If anything differs, you broke something!

### 4. Understand the Build Configuration

The project builds two executables from the same source:

| Output | Subsystem | Entry Point | Purpose |
|--------|-----------|-------------|---------|
| `uffs.com` | Console | `main()` | CLI tool |
| `uffs.exe` | Windows | `_tWinMain()` | GUI tool |

---

## Proposed Module Structure

```
UltraFastFileSearch-code/
├── src/
│   ├── core/                    # Core NTFS/MFT functionality
│   │   ├── ntfs_types.hpp       # NTFS structures (FILE_RECORD_SEGMENT_HEADER, etc.)
│   │   ├── ntfs_types.cpp
│   │   ├── mft_reader.hpp       # MFT reading with IOCP
│   │   ├── mft_reader.cpp
│   │   ├── mft_parser.hpp       # MFT record parsing
│   │   ├── mft_parser.cpp
│   │   ├── volume.hpp           # Volume handle management
│   │   └── volume.cpp
│   │
│   ├── index/                   # Indexing functionality
│   │   ├── ntfs_index.hpp       # NtfsIndex class
│   │   ├── ntfs_index.cpp
│   │   ├── index_builder.hpp    # Index building logic
│   │   └── index_builder.cpp
│   │
│   ├── search/                  # Search functionality
│   │   ├── pattern_matcher.hpp  # Regex/pattern matching
│   │   ├── pattern_matcher.cpp
│   │   ├── search_engine.hpp    # Search orchestration
│   │   └── search_engine.cpp
│   │
│   ├── io/                      # I/O abstraction
│   │   ├── iocp.hpp             # IoCompletionPort wrapper
│   │   ├── iocp.cpp
│   │   ├── overlapped.hpp       # Overlapped I/O helpers
│   │   └── overlapped.cpp
│   │
│   ├── util/                    # Utilities
│   │   ├── intrusive_ptr.hpp    # Smart pointer
│   │   ├── handle.hpp           # RAII handle wrapper
│   │   ├── string_utils.hpp     # String utilities
│   │   ├── time_utils.hpp       # Time conversion
│   │   └── error_handling.hpp   # Exception handling
│   │
│   ├── cli/                     # Command-line interface
│   │   ├── cli_main.cpp         # CLI entry point
│   │   ├── CommandLineParser.hpp
│   │   └── CommandLineParser.cpp
│   │
│   └── gui/                     # GUI (WTL)
│       ├── gui_main.cpp         # GUI entry point
│       ├── main_dialog.hpp      # CMainDlg
│       ├── main_dialog.cpp
│       ├── list_view.hpp        # ListView handling
│       └── list_view.cpp
│
├── include/                     # Public headers
│   └── uffs/
│       ├── uffs.hpp             # Main public API
│       └── types.hpp            # Public types
│
├── third_party/                 # Third-party code
│   ├── CLI11.hpp
│   └── WinDDKFixes.hpp
│
└── resources/                   # Resources
    ├── resource.h
    └── UltraFastFileSearch.rc
```

---

## Phase 1: Cleanup (2 hours)

**Goal**: Fix obvious issues without changing structure
**Risk**: Very Low
**Branch**: `refactoring/phase-1-cleanup`

### Step 1.1: Remove Duplicate Includes

**File**: `UltraFastFileSearch-code/UltraFastFileSearch.cpp`

**What to do**: Delete lines 61-70 (the duplicate includes)

**Before** (lines 46-70):
```cpp
#include "nformat.hpp"
#include "path.hpp"
#include "BackgroundWorker.hpp"
#include "ShellItemIDList.hpp"
#include "CModifiedDialogImpl.hpp"
#include "NtUserCallHook.hpp"
#include "string_matcher.hpp"
#include <boost/algorithm/string.hpp>
#include <chrono>
#include <codecvt>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <strsafe.h>
#include "BackgroundWorker.hpp"      // DELETE THIS LINE
#include "CModifiedDialogImpl.hpp"   // DELETE THIS LINE
#include "CommandLineParser.hpp"     // KEEP THIS LINE
#include "nformat.hpp"               // DELETE THIS LINE
#include "NtUserCallHook.hpp"        // DELETE THIS LINE
#include "path.hpp"                  // DELETE THIS LINE
#include "resource.h"                // KEEP THIS LINE
#include "ShellItemIDList.hpp"       // DELETE THIS LINE
#include "string_matcher.hpp"        // DELETE THIS LINE
#include "targetver.h"               // DELETE THIS LINE
```

**After** (lines 46-62):
```cpp
#include "nformat.hpp"
#include "path.hpp"
#include "BackgroundWorker.hpp"
#include "ShellItemIDList.hpp"
#include "CModifiedDialogImpl.hpp"
#include "NtUserCallHook.hpp"
#include "string_matcher.hpp"
#include <boost/algorithm/string.hpp>
#include <chrono>
#include <codecvt>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <strsafe.h>
#include "CommandLineParser.hpp"
#include "resource.h"
```

**Verification**:
```cmd
# Build the project
msbuild UltraFastFileSearch.sln /p:Configuration=Release

# Run tests
uffs.com --benchmark-index=C
```

### Step 1.2: Fix Syntax Error on Line 73

**File**: `UltraFastFileSearch-code/UltraFastFileSearch.cpp`

**Current** (line 73):
```cpp
#pragma clang diagnostic push# pragma clang diagnostic ignored "-Wignored-attributes"
```

**Problem**: Missing newline between two pragmas

**Fixed**:
```cpp
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
```

### Step 1.3: Organize Includes into Groups

**File**: `UltraFastFileSearch-code/UltraFastFileSearch.cpp`

Replace lines 11-70 with this organized version:

```cpp
// ============================================================================
// Standard C headers
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
// Standard C++ headers
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
// Compiler-specific headers
// ============================================================================
#if defined(_MSC_VER) && _MSC_VER >= 1800
#define is_trivially_copyable_v sizeof is_trivially_copyable
#include <atomic>
#undef is_trivially_copyable_v
#endif

#if defined(_CPPLIB_VER) && _CPPLIB_VER >= 610
#include <mutex>
#endif

#ifdef _OPENMP
#include <omp.h>
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
// Windows headers
// ============================================================================
#include <Windows.h>
#include <Dbt.h>
#include <muiload.h>
#include <ProvExce.h>
#include <ShlObj.h>
#include <strsafe.h>
#include <WinNLS.h>

// ============================================================================
// ATL/WTL headers
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
// Third-party headers
// ============================================================================
#include <boost/algorithm/string.hpp>

// ============================================================================
// Project headers
// ============================================================================
#include "targetver.h"
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
```

### Step 1.4: Delete file.cpp

**Why**: `file.cpp` is a duplicate of `UltraFastFileSearch.cpp` and is not used in the build.

**Steps**:

1. Open `UltraFastFileSearch.vcxproj` in a text editor
2. Search for `file.cpp`
3. If it's listed, remove it from the project
4. Delete the file:

```cmd
cd UltraFastFileSearch-code
del file.cpp
```

**Verification**:
```cmd
msbuild UltraFastFileSearch.sln /p:Configuration=Release
```

### Step 1.5: Add #pragma once to All Headers

Check each header file and add `#pragma once` at the top if missing:

| File | Has #pragma once? | Action |
|------|-------------------|--------|
| `BackgroundWorker.hpp` | Check | Add if missing |
| `CDlgTemplate.hpp` | Check | Add if missing |
| `CModifiedDialogImpl.hpp` | Check | Add if missing |
| `CommandLineParser.hpp` | Check | Add if missing |
| `nformat.hpp` | Check | Add if missing |
| `NtUserCallHook.hpp` | Check | Add if missing |
| `path.hpp` | Check | Add if missing |
| `ShellItemIDList.hpp` | Check | Add if missing |
| `string_matcher.hpp` | Check | Add if missing |
| `WinDDKFixes.hpp` | Check | Add if missing |

**Example** - if a file starts with:
```cpp
#ifndef BACKGROUND_WORKER_HPP
#define BACKGROUND_WORKER_HPP
```

Change to:
```cpp
#pragma once

#ifndef BACKGROUND_WORKER_HPP
#define BACKGROUND_WORKER_HPP
```

### Step 1.6: Commit Phase 1

```cmd
git add -A
git commit -m "Phase 1: Cleanup - remove duplicates, organize includes"
git push origin refactoring/phase-1-cleanup
```

---

## Phase 2: Extract NTFS Types (3 hours)

**Goal**: Move NTFS structures to dedicated header
**Risk**: Low
**Branch**: `refactoring/phase-2-ntfs-types`

### Step 2.1: Create Directory Structure

```cmd
cd UltraFastFileSearch-code
mkdir src
mkdir src\core
```

### Step 2.2: Create ntfs_types.hpp

Create file: `UltraFastFileSearch-code/src/core/ntfs_types.hpp`

```cpp
#pragma once

// ============================================================================
// NTFS Type Definitions
// ============================================================================
// This file contains all NTFS-related structures extracted from the main
// source file for better organization and reusability.
// ============================================================================

#include <Windows.h>
#include <cstdint>

namespace uffs {
namespace ntfs {

// ============================================================================
// NTFS Boot Sector (lines 1889-1924 in original)
// ============================================================================
#pragma pack(push, 1)
struct BootSector {
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

    unsigned int file_record_size() const {
        return this->ClustersPerFileRecordSegment >= 0
            ? this->ClustersPerFileRecordSegment * this->SectorsPerCluster * this->BytesPerSector
            : 1U << static_cast<int>(-this->ClustersPerFileRecordSegment);
    }

    unsigned int index_block_size() const {
        return this->ClustersPerIndexBlock * this->SectorsPerCluster * this->BytesPerSector;
    }
};
#pragma pack(pop)

// Verify size at compile time
static_assert(sizeof(BootSector) == 512, "BootSector must be 512 bytes");

// ============================================================================
// Multi-Sector Header (lines 1926-1955 in original)
// ============================================================================
struct MultiSectorHeader {
    unsigned long Magic;
    unsigned short USAOffset;
    unsigned short USACount;

    bool unfixup(size_t max_size) {
        unsigned short* usa = reinterpret_cast<unsigned short*>(
            &reinterpret_cast<unsigned char*>(this)[this->USAOffset]);
        unsigned short const usa0 = usa[0];
        bool result = true;
        for (unsigned short i = 1; i < this->USACount; i++) {
            const size_t offset = i * 512 - sizeof(unsigned short);
            unsigned short* const check = (unsigned short*)((unsigned char*)this + offset);
            if (offset < max_size) {
                result &= *check == usa0;
                *check = usa[i];
            } else {
                break;
            }
        }
        return result;
    }
};

// ============================================================================
// Attribute Type Codes
// ============================================================================
enum class AttributeType : uint32_t {
    StandardInformation = 0x10,
    AttributeList = 0x20,
    FileName = 0x30,
    ObjectId = 0x40,
    SecurityDescriptor = 0x50,
    VolumeName = 0x60,
    VolumeInformation = 0x70,
    Data = 0x80,
    IndexRoot = 0x90,
    IndexAllocation = 0xA0,
    Bitmap = 0xB0,
    ReparsePoint = 0xC0,
    EaInformation = 0xD0,
    Ea = 0xE0,
    End = 0xFFFFFFFF
};

// ============================================================================
// File Name Types
// ============================================================================
enum class FileNameType : uint8_t {
    Posix = 0,      // Case-sensitive, allows most Unicode
    Win32 = 1,      // Case-insensitive, Windows naming rules
    Dos = 2,        // 8.3 format
    Win32AndDos = 3 // Both Win32 and DOS names
};

} // namespace ntfs
} // namespace uffs
```

### Step 2.3: Add to Project

1. Open `UltraFastFileSearch.vcxproj` in Visual Studio
2. Right-click on "Header Files" → Add → Existing Item
3. Navigate to `src/core/ntfs_types.hpp` and add it

### Step 2.4: Include in Main File

Add this include near the top of `UltraFastFileSearch.cpp` (after other project headers):

```cpp
#include "src/core/ntfs_types.hpp"
```

### Step 2.5: Verify Build

```cmd
msbuild UltraFastFileSearch.sln /p:Configuration=Release
uffs.com --benchmark-index=C
```

### Step 2.6: Commit Phase 2

```cmd
git add -A
git commit -m "Phase 2: Extract NTFS types to src/core/ntfs_types.hpp"
git push origin refactoring/phase-2-ntfs-types
```

---

## Phase 3: Extract Utilities (3 hours)

**Goal**: Move utility classes to dedicated files
**Risk**: Low
**Branch**: `refactoring/phase-3-utilities`

### Step 3.1: Create Utility Directory

```cmd
mkdir src\util
```

### Step 3.2: Create handle.hpp

Create file: `UltraFastFileSearch-code/src/util/handle.hpp`

Extract the `Handle` class from lines 2432-2491 of `UltraFastFileSearch.cpp`:

```cpp
#pragma once

// ============================================================================
// RAII Handle Wrapper
// ============================================================================
// Extracted from UltraFastFileSearch.cpp lines 2432-2491
// ============================================================================

#include <Windows.h>
#include <utility>

namespace uffs {

class Handle {
public:
    Handle() noexcept : h_(nullptr) {}

    explicit Handle(HANDLE h) noexcept : h_(h) {}

    ~Handle() noexcept {
        reset();
    }

    // Move constructor
    Handle(Handle&& other) noexcept : h_(other.release()) {}

    // Move assignment
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    // No copying
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    // Swap
    void swap(Handle& other) noexcept {
        std::swap(h_, other.h_);
    }

    // Access
    HANDLE get() const noexcept { return h_; }

    operator HANDLE() const noexcept { return h_; }

    explicit operator bool() const noexcept {
        return valid(h_);
    }

    // Release ownership
    HANDLE release() noexcept {
        HANDLE h = h_;
        h_ = nullptr;
        return h;
    }

    // Reset with new handle
    void reset(HANDLE h = nullptr) noexcept {
        if (valid(h_)) {
            ::CloseHandle(h_);
        }
        h_ = h;
    }

private:
    HANDLE h_;

    static bool valid(HANDLE h) noexcept {
        return h != nullptr && h != INVALID_HANDLE_VALUE;
    }
};

// Free function swap
inline void swap(Handle& a, Handle& b) noexcept {
    a.swap(b);
}

} // namespace uffs
```

### Step 3.3: Create intrusive_ptr.hpp

Create file: `UltraFastFileSearch-code/src/util/intrusive_ptr.hpp`

Extract from lines 2872-3050 of `UltraFastFileSearch.cpp`.

(This is a longer extraction - copy the entire `intrusive_ptr` struct and `RefCounted` class)

### Step 3.4: Update Main File

At the top of `UltraFastFileSearch.cpp`, add:

```cpp
#include "src/util/handle.hpp"
#include "src/util/intrusive_ptr.hpp"
```

**IMPORTANT**: Do NOT delete the original code yet! First verify the build works with both the include and the original code. Then gradually replace usages.

### Step 3.5: Verify and Commit

```cmd
msbuild UltraFastFileSearch.sln /p:Configuration=Release
uffs.com --benchmark-index=C
git add -A
git commit -m "Phase 3: Extract utility classes to src/util/"
```

---

## Phase 4: Extract I/O Layer (6 hours)

**Goal**: Separate IOCP and async I/O code
**Risk**: Medium
**Branch**: `refactoring/phase-4-io-layer`

### Step 4.1: Create I/O Directory

```cmd
mkdir src\io
```

### Step 4.2: Create iocp.hpp

Extract `IoCompletionPort` class from lines 6740-7050.

### Step 4.3: Create overlapped.hpp

Extract `Overlapped` class from lines 3084-3175.

### Step 4.4: Create mft_reader.hpp

Extract `OverlappedNtfsMftReadPayload` from lines 7078-7560.

**WARNING**: This is the most sensitive extraction. The async I/O code is complex and interdependent. Test thoroughly after each change.

---

## Phase 5: Extract NtfsIndex (6 hours)

**Goal**: Separate the core index class
**Risk**: Medium
**Branch**: `refactoring/phase-5-ntfs-index`

### Step 5.1: Create Index Directory

```cmd
mkdir src\index
```

### Step 5.2: Identify NtfsIndex Dependencies

The `NtfsIndex` class (lines 3587-5565) depends on:
- `Handle` (extract in Phase 3)
- `intrusive_ptr` (extract in Phase 3)
- `RefCounted` (extract in Phase 3)
- NTFS types (extract in Phase 2)
- Various internal structs

### Step 5.3: Create ntfs_index.hpp

Create file: `UltraFastFileSearch-code/src/index/ntfs_index.hpp`

This is a large extraction (~2000 lines). Key sections:

| Lines | Content |
|-------|---------|
| 3587-3593 | Class declaration start |
| 3594-3852 | Internal packed structs |
| 3853-4160 | Member variables |
| 4161-5565 | Member functions |

### Step 5.4: Gradual Migration

1. First, just create the header with forward declarations
2. Include it in the main file
3. Verify build
4. Gradually move code, testing after each move

---

## Phase 6: Separate GUI from CLI (8 hours)

**Goal**: Clean separation of entry points
**Risk**: Medium
**Branch**: `refactoring/phase-6-gui-cli-split`

### Step 6.1: Understand Entry Points

The project has TWO entry points in the same file:

| Entry Point | Line | Purpose |
|-------------|------|---------|
| `main()` | 12892 | CLI tool (uffs.com) |
| `_tWinMain()` | 14073 | GUI tool (uffs.exe) |

### Step 6.2: Create CLI Directory

```cmd
mkdir src\cli
```

### Step 6.3: Create cli_main.cpp

Create file: `UltraFastFileSearch-code/src/cli/cli_main.cpp`

Extract `main()` function (lines 12892-14068):

```cpp
// ============================================================================
// CLI Entry Point
// ============================================================================
// Extracted from UltraFastFileSearch.cpp lines 12892-14068
// ============================================================================

#include "../core/ntfs_types.hpp"
#include "../index/ntfs_index.hpp"
#include "../io/iocp.hpp"
#include "CommandLineParser.hpp"

// ... rest of main() function
```

### Step 6.4: Create GUI Directory

```cmd
mkdir src\gui
```

### Step 6.5: Create gui_main.cpp

Create file: `UltraFastFileSearch-code/src/gui/gui_main.cpp`

Extract `_tWinMain()` function (lines 14073-14156).

### Step 6.6: Update Project File

Modify `UltraFastFileSearch.vcxproj` to:
1. Add new source files
2. Configure different entry points for .com vs .exe

---

## Phase 7: Modernize C++ Style (6 hours)

**Goal**: Update to modern C++ idioms
**Risk**: Low
**Branch**: `refactoring/phase-7-modernize`

### Step 7.1: Replace NULL with nullptr

**Find**: `NULL`
**Replace**: `nullptr`

**Example**:
```cpp
// Before
HANDLE h = NULL;
if (ptr == NULL) { ... }

// After
HANDLE h = nullptr;
if (ptr == nullptr) { ... }
```

**Command** (in Visual Studio):
- Ctrl+H (Find and Replace)
- Find: `\bNULL\b` (use regex)
- Replace: `nullptr`
- Replace All

**WARNING**: Do NOT replace in Windows API calls that expect `NULL` specifically.

### Step 7.2: Use auto for Complex Types

**Before**:
```cpp
std::vector<std::pair<unsigned long long, long long>>::const_iterator i = ret_ptrs.begin();
```

**After**:
```cpp
auto i = ret_ptrs.begin();
```

### Step 7.3: Use Range-Based For Loops

**Before**:
```cpp
for (size_t i = 0; i != path_names.size(); ++i) {
    process(path_names[i]);
}
```

**After**:
```cpp
for (const auto& path_name : path_names) {
    process(path_name);
}
```

**WARNING**: Only use range-for when you don't need the index!

### Step 7.4: Use enum class

**Before**:
```cpp
enum AttributeType {
    AT_STANDARD_INFORMATION = 0x10,
    AT_ATTRIBUTE_LIST = 0x20,
    // ...
};
```

**After**:
```cpp
enum class AttributeType : uint32_t {
    StandardInformation = 0x10,
    AttributeList = 0x20,
    // ...
};
```

### Step 7.5: Add [[nodiscard]]

Add `[[nodiscard]]` to functions where ignoring the return value is likely a bug:

```cpp
// Before
bool is_valid() const { return valid_; }

// After
[[nodiscard]] bool is_valid() const { return valid_; }
```

---

## Testing Checklist

Use this checklist after EVERY change:

### Build Tests

- [ ] `msbuild UltraFastFileSearch.sln /p:Configuration=Release` succeeds
- [ ] `msbuild UltraFastFileSearch.sln /p:Configuration=Debug` succeeds
- [ ] No new warnings introduced

### Functional Tests

- [ ] `uffs.com --help` shows help text
- [ ] `uffs.com --version` shows version
- [ ] `uffs.com --benchmark-index=C` completes successfully
- [ ] `uffs.com "*.txt" --drives=C` returns results
- [ ] `uffs.exe` launches GUI successfully
- [ ] GUI can search and display results

### Regression Tests

- [ ] Benchmark time is within 5% of baseline
- [ ] Search results match baseline exactly
- [ ] No memory leaks (run with Debug build)

---

## Troubleshooting

### Problem: Build fails after extracting code

**Solution**:
1. Check for missing includes in the new file
2. Check for missing forward declarations
3. Verify namespace usage is correct
4. Check for circular dependencies

### Problem: Linker errors (unresolved external symbol)

**Solution**:
1. Make sure the new .cpp file is added to the project
2. Check that functions are not accidentally marked `static`
3. Verify the function signature matches between .hpp and .cpp

### Problem: Runtime crash after refactoring

**Solution**:
1. Run in Debug mode to get a stack trace
2. Check for uninitialized variables
3. Verify object lifetimes haven't changed
4. Check for threading issues if async code was modified

### Problem: Performance regression

**Solution**:
1. Use profiler to identify bottleneck
2. Check if any code was accidentally duplicated
3. Verify inline functions are still being inlined
4. Check for unnecessary copies (use `const&` where appropriate)

---

## Quick Reference: Line Numbers

Use this table to quickly find code in `UltraFastFileSearch.cpp`:

| Component | Start Line | End Line | Extract To |
|-----------|------------|----------|------------|
| Includes | 11 | 70 | (cleanup only) |
| NTFS namespace | 1886 | 2430 | `src/core/ntfs_types.hpp` |
| Handle class | 2432 | 2491 | `src/util/handle.hpp` |
| IoPriority class | 2493 | 2870 | `src/io/io_priority.hpp` |
| intrusive_ptr | 2872 | 3050 | `src/util/intrusive_ptr.hpp` |
| RefCounted | 3052 | 3082 | `src/util/ref_counted.hpp` |
| Overlapped | 3084 | 3175 | `src/io/overlapped.hpp` |
| NtfsIndex | 3587 | 5565 | `src/index/ntfs_index.hpp` |
| MatchOperation | 5448 | 5565 | `src/search/match_operation.hpp` |
| IoCompletionPort | 6740 | 7050 | `src/io/iocp.hpp` |
| OverlappedNtfsMftReadPayload | 7078 | 7560 | `src/core/mft_reader.hpp` |
| SearchResult | 7564 | 7596 | `src/search/search_result.hpp` |
| Results | 7603 | 7726 | `src/search/results.hpp` |
| CMainDlg | 7842 | 11960 | `src/gui/main_dialog.hpp` |
| main() | 12892 | 14068 | `src/cli/cli_main.cpp` |
| _tWinMain() | 14073 | 14156 | `src/gui/gui_main.cpp` |

---

## Build System Updates (Optional)

### CMakeLists.txt (Recommended)

Consider migrating from `.vcxproj` to CMake for better maintainability:

```cmake
cmake_minimum_required(VERSION 3.20)
project(UltraFastFileSearch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core library
add_library(uffs_core STATIC
    src/core/ntfs_types.cpp
    src/core/mft_reader.cpp
    src/core/mft_parser.cpp
    src/core/volume.cpp
    src/index/ntfs_index.cpp
    src/io/iocp.cpp
    src/util/string_utils.cpp
)

target_include_directories(uffs_core PUBLIC include src)

# CLI executable
add_executable(uffs_cli
    src/cli/cli_main.cpp
    src/cli/CommandLineParser.cpp
)
target_link_libraries(uffs_cli PRIVATE uffs_core)

# GUI executable
add_executable(uffs_gui WIN32
    src/gui/gui_main.cpp
    src/gui/main_dialog.cpp
    resources/UltraFastFileSearch.rc
)
target_link_libraries(uffs_gui PRIVATE uffs_core)
```

---

## Testing Strategy

### Before Each Phase

1. **Build** the project
2. **Run** `uffs.com --benchmark-index=C` and record timing
3. **Run** `uffs.com test --drives=C` and record results
4. **Save** these as baseline

### After Each Phase

1. **Build** and verify no errors
2. **Run** same benchmarks
3. **Compare** results - must be identical
4. **Commit** if passing

---

## Risk Mitigation

1. **Git branches**: Create a branch for each phase
2. **Small commits**: Commit after each successful extraction
3. **Automated tests**: Run benchmarks after each change
4. **Code review**: Review diffs carefully before merging
5. **Rollback plan**: Keep main branch clean until phase is complete

---

## Priority Order

| Priority | Phase | Effort | Impact |
|----------|-------|--------|--------|
| 1 | Phase 1: Cleanup | 1-2h | High (removes bugs) |
| 2 | Phase 2: NTFS Types | 2-3h | High (foundation) |
| 3 | Phase 3: Utilities | 2-3h | Medium |
| 4 | Phase 7: Modernize | 4-6h | Medium (readability) |
| 5 | Phase 4: I/O Layer | 4-6h | High (core logic) |
| 6 | Phase 5: NtfsIndex | 4-6h | High (core logic) |
| 7 | Phase 6: GUI/CLI Split | 6-8h | Medium |

**Total estimated effort**: 25-40 hours

---

## Quick Wins (Do First)

These can be done immediately with minimal risk:

1. **Delete `file.cpp`** - it's unused/duplicate
2. **Remove duplicate includes** in UltraFastFileSearch.cpp
3. **Remove hardcoded VS path** in file.cpp (if keeping it)
4. **Add `#pragma once`** to all headers
5. **Organize includes** into logical groups with comments

---

## Conclusion

This refactoring plan transforms a 14,000+ line monolith into a well-organized, maintainable codebase. Each phase is designed to be:

- **Incremental**: Can stop at any phase
- **Testable**: Verify functionality after each phase
- **Reversible**: Easy to rollback if issues arise
- **Non-breaking**: 100% functionality preserved

The end result will be a modern C++ codebase that's easier to understand, maintain, and extend.
