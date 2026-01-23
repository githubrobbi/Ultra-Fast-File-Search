# Ultra Fast File Search - Architecture Overview

## Executive Summary

Ultra Fast File Search (UFFS) is a high-performance Windows file search utility that achieves its speed by directly reading the NTFS Master File Table (MFT) rather than using standard Windows file enumeration APIs. This document provides a comprehensive architectural overview for developers who wish to understand, maintain, or recreate this system.

## System Context

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              User Interface                                  │
│  ┌─────────────────────────────┐    ┌─────────────────────────────────────┐ │
│  │   GUI (WTL/ATL Dialog)      │    │   CLI (LLVM CommandLine Parser)    │ │
│  │   uffs.exe                  │    │   uffs.com                         │ │
│  └──────────────┬──────────────┘    └──────────────────┬──────────────────┘ │
└─────────────────┼──────────────────────────────────────┼────────────────────┘
                  │                                      │
                  ▼                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Core Search Engine                                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  │
│  │   NtfsIndex     │  │  MatchOperation │  │     BackgroundWorker        │  │
│  │   (MFT Parser)  │  │  (Pattern Match)│  │     (Thread Pool)           │  │
│  └────────┬────────┘  └────────┬────────┘  └──────────────┬──────────────┘  │
└───────────┼────────────────────┼─────────────────────────┼──────────────────┘
            │                    │                         │
            ▼                    ▼                         ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Low-Level Components                                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  │
│  │ NTFS Structures │  │  string_matcher │  │    IoCompletionPort         │  │
│  │ (Boot, MFT, FRS)│  │  (Regex/Glob)   │  │    (Async I/O)              │  │
│  └────────┬────────┘  └─────────────────┘  └──────────────┬──────────────┘  │
└───────────┼────────────────────────────────────────────────┼────────────────┘
            │                                                │
            ▼                                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Windows Kernel / NTFS                                │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │  Direct Volume Access (CreateFile on \\.\C:)                            ││
│  │  NT Native APIs (NtQueryVolumeInformationFile, etc.)                    ││
│  │  Raw MFT Reading via ReadFile with OVERLAPPED                           ││
│  └─────────────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Entry Points

The application has two entry points depending on the subsystem:

| Entry Point | File | Purpose |
|-------------|------|---------|
| `_tWinMain` | UltraFastFileSearch.cpp:8019 / file.cpp:8019 | GUI mode (Windows subsystem) |
| `main` / `_tmain` | UltraFastFileSearch.cpp:12068 / file.cpp:7740 | CLI mode (Console subsystem) |

**Startup Sequence:**
1. `extract_and_run_if_needed()` - Handles WOW64 (32-bit on 64-bit) by extracting embedded 64-bit binary
2. Check subsystem type via `get_subsystem()`
3. Initialize WTL/ATL module (`_Module.Init()`)
4. Create single-instance mutex to prevent multiple instances
5. Load MUI (Multilingual User Interface) resources
6. Create main dialog (`CMainDlg`) or run CLI search

### 2. NtfsIndex Class (The Core Engine)

**Location:** `UltraFastFileSearch.cpp` lines 3578-5435

The `NtfsIndex` class is the heart of the application. It:
- Opens a volume handle with direct access
- Reads and parses the MFT
- Builds an in-memory index of all files
- Provides pattern matching against the index

**Key Data Structures:**

```cpp
class NtfsIndex : public RefCounted<NtfsIndex> {
    // Thread synchronization
    atomic_namespace::recursive_mutex _mutex;
    
    // Volume information
    std::tvstring _root_path;      // e.g., "C:\"
    Handle _volume;                 // Volume handle
    
    // MFT data storage
    std::tvstring names;            // All filenames concatenated
    Records records_data;           // File record data
    RecordsLookup records_lookup;   // FRS -> records_data index
    LinkInfos nameinfos;            // Hard link information
    StreamInfos streaminfos;        // NTFS stream information
    ChildInfos childinfos;          // Parent-child relationships
    
    // Progress tracking
    atomic<unsigned int> _records_so_far;
    atomic<unsigned int> _preprocessed_so_far;
    Handle _finished_event;
};
```

### 3. Pattern Matching System

**Location:** `string_matcher.cpp`, `string_matcher.hpp`

The pattern matching system supports multiple strategies:

| Pattern Kind | Description | Example |
|--------------|-------------|---------|
| `pattern_anything` | Matches everything | (empty pattern) |
| `pattern_verbatim` | Exact substring match | `readme` |
| `pattern_glob` | Shell-style wildcards | `*.txt` |
| `pattern_globstar` | Extended glob with `**` | `**/*.cpp` |
| `pattern_regex` | Full regex (prefix with `>`) | `>.*\.txt$` |

**Optimization Techniques:**
- Boyer-Moore-Horspool algorithm for substring search
- Case-insensitive matching via custom iterators
- High-water mark tracking for early termination

### 4. Threading Model

**Location:** `BackgroundWorker.hpp`

```cpp
class BackgroundWorker {
    // Semaphore-based task queue
    HANDLE hSemaphore;
    std::deque<std::pair<long, Thunk*>> todo;
    
    // Worker thread
    HANDLE hThread;
    unsigned int tid;
};
```

**Thread Architecture:**
- Main UI thread handles user interaction
- One `BackgroundWorker` per drive for MFT reading
- `IoCompletionPort` for async I/O coordination
- Critical sections protect shared state

### 5. I/O Completion Port System

**Purpose:** Coordinate asynchronous MFT reading across multiple drives

```cpp
class IoCompletionPort {
    Handle handle;  // CreateIoCompletionPort handle
    
    void post(size_t size, uintptr_t key, intrusive_ptr<Overlapped> payload);
    bool get(size_t& size, uintptr_t& key, Overlapped*& payload, DWORD timeout);
};
```

## File Structure

```
UltraFastFileSearch-code/
├── UltraFastFileSearch.cpp   # Main source (~13,400 lines) - GUI + Core
├── file.cpp                   # CLI version (~8,100 lines) - Similar core
├── stdafx.h                   # Precompiled header
├── BackgroundWorker.hpp       # Threading infrastructure
├── string_matcher.cpp/.hpp    # Pattern matching engine
├── path.hpp                   # Path manipulation utilities
├── nformat.hpp                # Number formatting
├── ShellItemIDList.hpp        # Shell integration
├── CModifiedDialogImpl.hpp    # Custom dialog base class
├── NtUserCallHook.hpp         # NT user call hooks
├── resource.h                 # Resource IDs
├── UltraFastFileSearch.rc     # Resources (dialogs, strings, icons)
└── Project.props              # MSBuild properties
```

## Dependencies

| Dependency | Purpose | Version |
|------------|---------|---------|
| **Boost** | Xpressive (regex), Algorithm (Boyer-Moore-Horspool) | 1.90.0 |
| **WTL** | Windows Template Library for GUI | r636 |
| **ATL** | Active Template Library | VS 2017+ |
| **LLVM** | CommandLine parser for CLI | Support library |
| **Windows SDK** | Windows APIs | 10.0+ |

## Build Configurations

| Configuration | Output | Subsystem |
|---------------|--------|-----------|
| COM (Release/Debug) | uffs.com | Console |
| EXE (Release/Debug) | uffs.exe | Windows GUI |

Both configurations share the same source code; the subsystem determines which entry point is used.

## Data Flow

### Search Operation Flow

```
1. User Input
   └─► Pattern entered (e.g., "*.cpp" or ">.*\.h$")
       │
2. Pattern Parsing (MatchOperation::init)
   ├─► Detect pattern type (regex prefix ">", glob, verbatim)
   ├─► Extract root path optimization (e.g., "C:\Users\*.txt" → root="C:\Users\")
   └─► Compile pattern into string_matcher
       │
3. Drive Enumeration
   ├─► get_volume_path_names() → List all NTFS volumes
   └─► Filter by root path optimization
       │
4. Parallel MFT Reading (per drive)
   ├─► NtfsIndex::init() → Open volume handle
   ├─► Read NTFS boot sector → Get MFT location
   ├─► IoCompletionPort posts async read requests
   ├─► OverlappedNtfsMftReadPayload handles completion
   └─► NtfsIndex::load() parses FILE_RECORD_SEGMENT_HEADER
       │
5. Index Building
   ├─► Parse $STANDARD_INFORMATION → timestamps, attributes
   ├─► Parse $FILE_NAME → filename, parent reference
   ├─► Parse $DATA, $INDEX_ROOT, etc. → sizes, streams
   └─► Build parent-child relationships
       │
6. Pattern Matching (NtfsIndex::matches)
   ├─► Traverse index depth-first
   ├─► Build full path for each entry
   ├─► Apply string_matcher::is_match()
   └─► Yield matching results
       │
7. Output
   ├─► GUI: Populate ListView with results
   └─► CLI: Write to stdout (CSV, plain text, etc.)
```

### Memory Layout

The index uses a compact, cache-friendly memory layout:

```
┌─────────────────────────────────────────────────────────────────┐
│ names (std::tvstring)                                           │
│ ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐  │
│ │file1│.txt │file2│.cpp │dir1 │file3│.h   │...  │     │     │  │
│ └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘  │
│   ▲                                                             │
│   │ offset                                                      │
└───┼─────────────────────────────────────────────────────────────┘
    │
┌───┴─────────────────────────────────────────────────────────────┐
│ records_data (vector<Record>)                                   │
│ ┌──────────────────────────────────────────────────────────────┐│
│ │ Record[0]: stdinfo, name_count, stream_count, first_child,  ││
│ │            first_name (LinkInfo), first_stream (StreamInfo) ││
│ ├──────────────────────────────────────────────────────────────┤│
│ │ Record[1]: ...                                               ││
│ └──────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
    │
    │ FRS (File Record Segment number)
    ▼
┌─────────────────────────────────────────────────────────────────┐
│ records_lookup (vector<unsigned int>)                           │
│ ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐  │
│ │  0  │  1  │ -1  │  2  │  3  │ -1  │  4  │...  │     │     │  │
│ └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘  │
│ (Maps FRS → index in records_data; -1 = not present)           │
└─────────────────────────────────────────────────────────────────┘
```

## Key Algorithms

### 1. MFT Record Parsing

```cpp
// Pseudocode for MFT record processing
void NtfsIndex::load(virtual_offset, buffer, size) {
    for each mft_record in buffer {
        FILE_RECORD_SEGMENT_HEADER* frsh = get_record(buffer, offset);

        // Verify and unfixup Update Sequence Array
        if (frsh->Magic == 'ELIF' && frsh->unfixup(record_size)) {

            // Get base record (for extended records)
            frs_base = frsh->BaseFileRecordSegment ?: current_frs;

            // Parse each attribute
            for each ATTRIBUTE_RECORD_HEADER in frsh {
                switch (attribute->Type) {
                    case AttributeStandardInformation:
                        // Extract timestamps, file attributes
                        break;
                    case AttributeFileName:
                        // Extract filename, parent directory reference
                        // Build parent-child relationship
                        break;
                    case AttributeData:
                    case AttributeIndexRoot:
                    case AttributeIndexAllocation:
                        // Extract size information
                        // Handle sparse, compressed, encrypted flags
                        break;
                }
            }
        }
    }
}
```

### 2. Path Reconstruction

The `ParentIterator` class reconstructs full paths by walking up the parent chain:

```cpp
class ParentIterator {
    // Yields path components from leaf to root
    // Example: "C:\Users\John\file.txt"
    //   Iteration 0: "file.txt"
    //   Iteration 1: "\"
    //   Iteration 2: "John"
    //   Iteration 3: "\"
    //   Iteration 4: "Users"
    //   Iteration 5: "\"
    //   (root reached)

    // Path is built in reverse, then std::reverse() is called
};
```

### 3. Pattern Matching Strategy Selection

```cpp
void MatchOperation::init(pattern) {
    // Regex detection (prefix with ">")
    is_regex = pattern.starts_with('>');

    // Path pattern detection
    is_path_pattern = is_regex ||
                      pattern.contains('\\') ||
                      pattern.contains("**");

    // Stream pattern detection (NTFS alternate data streams)
    is_stream_pattern = is_regex || pattern.contains(':');

    // Root path optimization
    if (is_path_pattern && !is_regex && pattern[0] != '*') {
        root_path_optimized_away = extract_root_prefix(pattern);
    }

    // Auto-wrap simple patterns with **
    if (!is_path_pattern && !has_wildcards(pattern)) {
        pattern = "**" + pattern + "**";  // Substring match
    }

    // Compile pattern
    matcher = string_matcher(kind, options, pattern);
}
```

## Error Handling

The codebase uses a mix of error handling strategies:

| Strategy | Usage |
|----------|-------|
| Win32 Error Codes | Low-level Windows API calls |
| C++ Exceptions | `std::invalid_argument`, `std::runtime_error` |
| SEH (Structured Exception Handling) | Critical sections, hardware exceptions |
| HRESULT | COM interop |

**Key Error Handling Functions:**
- `CheckAndThrow(condition)` - Throws if condition is false
- `CppRaiseException(error_code)` - Converts Win32 error to exception
- `GetAnyErrorText(code)` - Formats error message

## Thread Safety

### Protected Resources

| Resource | Protection Mechanism |
|----------|---------------------|
| `NtfsIndex` internal state | `atomic_namespace::recursive_mutex` |
| `BackgroundWorker` task queue | `CRITICAL_SECTION` |
| Progress counters | `atomic<unsigned int>` |
| Shared UI state | Message passing via `PostMessage` |

### Lock Acquisition Pattern

```cpp
// Using lock_ptr RAII wrapper
{
    lock_ptr<NtfsIndex> locked = lock(index);
    // Safe to access index members
    locked->some_method();
}  // Lock released automatically
```

## Performance Considerations

### Why Direct MFT Reading is Fast

1. **Single Sequential Read**: MFT is read sequentially, not random access
2. **No Handle Overhead**: No `CreateFile`/`CloseHandle` per file
3. **No Security Checks**: Bypasses per-file ACL evaluation
4. **Minimal Syscalls**: Bulk reads instead of per-file queries
5. **Parallel I/O**: Multiple drives read simultaneously

### Memory Optimization Techniques

1. **Compact Structures**: Bit-packed fields (e.g., `StandardInfo` uses bitfields)
2. **String Deduplication**: All names stored in single contiguous buffer
3. **ASCII Optimization**: Single-byte storage for ASCII-only names
4. **Lazy Allocation**: Vectors pre-reserved based on expected record count

### Typical Performance

| Metric | Value |
|--------|-------|
| Index build time | 2-10 seconds per million files |
| Memory usage | ~100-200 bytes per file |
| Search time | Sub-second for most patterns |

## Security Considerations

1. **Requires Administrator Privileges**: Direct volume access needs elevation
2. **Bypasses File Permissions**: Can see all files regardless of ACLs
3. **Read-Only Access**: Only reads MFT, never modifies
4. **No Network Access**: Local drives only

## Limitations

1. **NTFS Only**: Does not support FAT32, exFAT, ReFS, or network shares
2. **Windows Only**: Uses Windows-specific APIs throughout
3. **No Real-time Updates**: Index is a snapshot; changes require re-scan
4. **Large Memory Footprint**: Entire index held in RAM
5. **No Content Search**: Searches filenames only, not file contents

## Extension Points

### Adding New Output Formats (CLI)

Modify the `Writer` class in `file.cpp` around line 7867:

```cpp
class Writer {
    void operator()(std::tvstring &line_buffer, bool force) {
        // Add new format handling here
    }
};
```

### Adding New Pattern Types

Extend `string_matcher::pattern_kind` enum and implement in `string_matcher.cpp`:

```cpp
enum pattern_kind {
    pattern_anything,
    pattern_verbatim,
    pattern_glob,
    pattern_globstar,
    pattern_regex,
    // Add new pattern type here
};
```

### Adding New File Attributes

Extend `StandardInfo` structure in `NtfsIndex` class:

```cpp
struct StandardInfo {
    unsigned long long
        created,
        written,
        accessed : 0x40 - 6,
        // Add new attribute flags here
        is_new_attribute : 1;
};
```

## Glossary

| Term | Definition |
|------|------------|
| **MFT** | Master File Table - NTFS database containing all file metadata |
| **FRS** | File Record Segment - Individual record in the MFT |
| **VCN** | Virtual Cluster Number - Logical cluster offset within a file |
| **LCN** | Logical Cluster Number - Physical cluster offset on disk |
| **USA** | Update Sequence Array - NTFS integrity protection mechanism |
| **ADS** | Alternate Data Stream - Additional named data streams on NTFS files |

## Related Documents

- [02-MFT Reading Deep Dive](02-mft-reading-deep-dive.md)
- [03-Threading Model](03-threading-model.md)
- [04-Pattern Matching](04-pattern-matching.md)
- [05-Build Guide](05-build-guide.md)
- [06-CLI Reference](06-cli-reference.md)

---

*Document Version: 1.0*
*Last Updated: 2026-01-20*
*Author: Architecture Documentation*

