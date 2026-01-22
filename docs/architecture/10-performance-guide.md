# Performance Guide

## Introduction

This document provides exhaustive detail on the performance optimization techniques used in Ultra Fast File Search. After reading this document, you should be able to:

1. Understand why UFFS is 5000x faster than standard file enumeration
2. Identify and apply the key optimization patterns
3. Profile and benchmark the application
4. Extend the codebase without degrading performance

---

## Overview: Performance Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Performance Optimization Layers                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Layer 1: I/O Optimization                                              │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ • Direct MFT reading (bypass file system APIs)                  │   │
│  │ • Bitmap-based skip optimization                                │   │
│  │ • Async I/O with completion ports                               │   │
│  │ • Buffer recycling (no allocation per read)                     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Layer 2: Memory Optimization                                           │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ • Compact data structures (bit-packing)                         │   │
│  │ • ASCII optimization for filenames                              │   │
│  │ • Contiguous storage (cache-friendly)                           │   │
│  │ • Lazy path resolution                                          │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Layer 3: Algorithm Optimization                                        │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ • Boyer-Moore-Horspool for substring search                     │   │
│  │ • High-water mark for early termination                         │   │
│  │ • Pattern type detection for optimal matcher                    │   │
│  │ • Volume pre-matching                                           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Layer 4: Concurrency Optimization                                      │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ • Parallel volume indexing                                      │   │
│  │ • I/O completion ports for async reads                          │   │
│  │ • Lock-free progress counters                                   │   │
│  │ • UI thread isolation                                           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Benchmark: UFFS vs Standard APIs

### Test Environment

- **System**: Windows 10, NVMe SSD
- **Files**: 2 million files across 200,000 directories
- **Pattern**: `*.cpp` (simple glob)

### Results

| Method | Time | Speedup |
|--------|------|---------|
| FindFirstFile/FindNextFile | ~30 minutes | 1x |
| std::filesystem::recursive_directory_iterator | ~25 minutes | 1.2x |
| Everything (voidtools) | ~1 second | 1800x |
| **UFFS** | **~350ms** | **5000x** |

### Why Standard APIs Are Slow

```
Standard API (per file):
┌─────────────────────────────────────────────────────────────────────────┐
│ 1. CreateFile() → Open handle                                          │
│ 2. GetFileInformationByHandle() → Query metadata                       │
│ 3. Security check → Evaluate ACLs                                      │
│ 4. CloseHandle() → Release handle                                      │
│ 5. Repeat 2,000,000 times...                                           │
└─────────────────────────────────────────────────────────────────────────┘
Total syscalls: ~8,000,000

UFFS (entire volume):
┌─────────────────────────────────────────────────────────────────────────┐
│ 1. Open volume handle (1 syscall)                                      │
│ 2. Read MFT bitmap (~10 reads)                                         │
│ 3. Read MFT data (~2000 reads of 1MB each)                             │
│ 4. Parse in memory (no syscalls)                                       │
└─────────────────────────────────────────────────────────────────────────┘
Total syscalls: ~2,000
```

---

## I/O Optimization

### Direct MFT Reading

Instead of using file system APIs, UFFS reads the raw MFT:

```cpp
// Open volume for raw access
Handle volume = CreateFile(
    _T("\\\\.\\C:"),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
    NULL
);

// Read MFT directly
DeviceIoControl(volume, FSCTL_GET_NTFS_VOLUME_DATA, ...);
DeviceIoControl(volume, FSCTL_GET_RETRIEVAL_POINTERS, ...);
ReadFile(volume, buffer, cluster_size * clusters_per_read, ...);
```

**Benefits**:
- Single sequential read vs millions of random accesses
- No per-file handle overhead
- No security check overhead
- Kernel-level buffering bypassed

### Bitmap-Based Skip Optimization

The MFT bitmap indicates which records are in use:

```cpp
// Calculate skip regions from bitmap
void calculate_skips(unsigned char const* bitmap, size_t bitmap_size) {
    size_t current_skip_start = 0;
    bool in_skip = false;

    for (size_t byte = 0; byte < bitmap_size; ++byte) {
        unsigned char bits = bitmap[byte];
        for (int bit = 0; bit < 8; ++bit) {
            bool in_use = (bits >> bit) & 1;
            size_t record = byte * 8 + bit;

            if (!in_use && !in_skip) {
                current_skip_start = record;
                in_skip = true;
            } else if (in_use && in_skip) {
                add_skip_region(current_skip_start, record);
                in_skip = false;
            }
        }
    }
}
```

**Impact**: Reduces I/O by 60-80% on typical volumes.

### Async I/O with Completion Ports

```cpp
// Create completion port
HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
CreateIoCompletionPort(volume_handle, iocp, volume_key, 0);

// Queue multiple reads
for (int i = 0; i < CONCURRENT_READS; ++i) {
    OVERLAPPED* ovl = get_overlapped(i);
    ReadFile(volume, buffers[i], READ_SIZE, NULL, ovl);
}

// Process completions
while (GetQueuedCompletionStatus(iocp, &bytes, &key, &ovl, INFINITE)) {
    process_completed_read(ovl);
    queue_next_read();  // Maintain concurrency level
}
```

**Benefits**:
- CPU processes data while next read is in flight
- Kernel manages thread scheduling
- Maximum disk utilization

### Buffer Recycling

```cpp
// Buffer pool to avoid allocation overhead
class BufferPool {
    std::vector<void*> free_buffers;
    CRITICAL_SECTION lock;

public:
    void* acquire() {
        EnterCriticalSection(&lock);
        if (free_buffers.empty()) {
            LeaveCriticalSection(&lock);
            return VirtualAlloc(NULL, BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);
        }
        void* buffer = free_buffers.back();
        free_buffers.pop_back();
        LeaveCriticalSection(&lock);
        return buffer;
    }

    void release(void* buffer) {
        EnterCriticalSection(&lock);
        free_buffers.push_back(buffer);
        LeaveCriticalSection(&lock);
    }
};
```

**Benefits**:
- No malloc/free per read operation
- Reduced memory fragmentation
- Better cache utilization

---

## Memory Optimization

### Compact Data Structures

UFFS uses aggressive bit-packing to minimize memory footprint:

```cpp
// small_t: Use smallest integer type that fits
template<class = void> struct small_t {
    typedef unsigned int type;  // 4 bytes instead of 8
};

// file_size_type: 6 bytes instead of 8
class file_size_type {
    unsigned int low;      // 4 bytes
    unsigned short high;   // 2 bytes
    // Total: 6 bytes for 48-bit file sizes (256 TB max)
};

// StandardInfo: Bit-packed flags
struct StandardInfo {
    unsigned int attributes : 14;  // File attributes
    unsigned int is_directory : 1;
    unsigned int is_reparse_point : 1;
    // ... more flags packed into single word
};
```

**Memory Savings**:

| Structure | Naive Size | Optimized Size | Savings |
|-----------|------------|----------------|---------|
| file_size_type | 8 bytes | 6 bytes | 25% |
| StandardInfo | 16 bytes | 4 bytes | 75% |
| NameInfo | 12 bytes | 5 bytes | 58% |
| Record | 64 bytes | 32 bytes | 50% |

### ASCII Optimization

Most filenames are ASCII-only. UFFS stores these as single bytes:

```cpp
struct NameInfo {
    small_t<size_t>::type _offset;  // Bit 0 = ASCII flag
    unsigned char length;

    bool ascii() const { return !!(this->_offset & 1U); }

    small_t<size_t>::type offset() const {
        return this->_offset >> 1;
    }
};

// Storage in names buffer:
// ASCII:   "readme.txt" → 10 bytes
// Unicode: "readme.txt" → 20 bytes (UTF-16)
```

**Impact**: ~40% reduction in name storage for typical volumes.

### Contiguous Storage

All records stored in contiguous vectors for cache efficiency:

```cpp
class NtfsIndex {
    std::vector<Record> records;           // All file records
    std::vector<unsigned char> names;      // All filenames
    std::vector<NameInfo> nameinfos;       // Hard link names
    std::vector<StreamInfo> streaminfos;   // Alternate streams

    // O(1) lookup by FRS number
    Record& get_record(unsigned int frs) {
        return records[frs];
    }
};
```

**Benefits**:
- Sequential memory access patterns
- CPU prefetching works effectively
- Minimal cache misses during traversal

### Lazy Path Resolution

Full paths are built only when needed:

```cpp
// During indexing: Store only parent reference
struct Record {
    small_t<size_t>::type parent;  // Parent FRS, not full path
    NameInfo first_name;           // Just the filename
};

// During display: Build path on demand
std::wstring build_path(Record const& record) {
    std::vector<std::wstring> components;
    Record const* current = &record;

    while (current->parent != ROOT_FRS) {
        components.push_back(get_name(*current));
        current = &records[current->parent];
    }

    // Reverse and join
    std::wstring path;
    for (auto it = components.rbegin(); it != components.rend(); ++it) {
        path += L'\\';
        path += *it;
    }
    return path;
}
```

**Impact**: Paths average 50 characters. For 2M files, this saves ~200MB.

---

## Algorithm Optimization

### Boyer-Moore-Horspool for Substring Search

For substring patterns, UFFS uses Boyer-Moore-Horspool:

```cpp
// Preprocessing: Build skip table
template<typename CharT>
class BMHSearcher {
    size_t skip_table[256];
    std::basic_string<CharT> pattern;

public:
    BMHSearcher(std::basic_string<CharT> const& pat) : pattern(pat) {
        size_t m = pattern.size();

        // Default skip: pattern length
        std::fill(skip_table, skip_table + 256, m);

        // Character-specific skips
        for (size_t i = 0; i < m - 1; ++i) {
            skip_table[static_cast<unsigned char>(pattern[i])] = m - 1 - i;
        }
    }

    // Search: O(n/m) average case
    bool search(CharT const* text, size_t n) {
        size_t m = pattern.size();
        size_t i = m - 1;

        while (i < n) {
            size_t j = m - 1;
            while (j > 0 && text[i] == pattern[j]) {
                --i; --j;
            }
            if (j == 0 && text[i] == pattern[0]) {
                return true;  // Match found
            }
            i += skip_table[static_cast<unsigned char>(text[i])];
        }
        return false;
    }
};
```

**Performance**:

| Pattern Length | Average Case | Worst Case |
|----------------|--------------|------------|
| m characters | O(n/m) | O(n*m) |

### High-Water Mark Tracking

Track the furthest position examined for early termination:

```cpp
template<typename Iterator>
class HighWaterMarkIterator {
    Iterator current;
    Iterator& high_water_mark;

public:
    HighWaterMarkIterator(Iterator it, Iterator& hwm)
        : current(it), high_water_mark(hwm) {}

    auto operator*() const {
        if (current > high_water_mark) {
            high_water_mark = current;
        }
        return *current;
    }

    HighWaterMarkIterator& operator++() {
        ++current;
        return *this;
    }
};
```

**Use Case**: When matching `*.cpp` against `readme.txt`, the matcher can stop after examining `.txt` without checking the full filename.

### Pattern Type Detection

UFFS analyzes patterns to choose optimal matching strategy:

```cpp
enum PatternType {
    LITERAL,      // "readme.txt" → direct comparison
    SUFFIX,       // "*.cpp" → check end only
    PREFIX,       // "test*" → check start only
    SUBSTRING,    // "*test*" → Boyer-Moore-Horspool
    GLOB,         // "test*.cpp" → glob matching
    REGEX         // "test[0-9]+\.cpp" → regex engine
};

PatternType analyze_pattern(std::wstring const& pattern) {
    bool has_star = pattern.find(L'*') != std::wstring::npos;
    bool has_question = pattern.find(L'?') != std::wstring::npos;
    bool has_bracket = pattern.find(L'[') != std::wstring::npos;

    if (!has_star && !has_question && !has_bracket) {
        return LITERAL;
    }
    if (pattern.front() == L'*' && pattern.back() != L'*' &&
        pattern.find(L'*', 1) == std::wstring::npos) {
        return SUFFIX;
    }
    // ... more analysis
}
```

### Volume Pre-Matching

Skip volumes that cannot match the pattern:

```cpp
bool volume_can_match(NtfsIndex const& index, Pattern const& pattern) {
    // Check if pattern specifies a drive letter
    if (pattern.has_drive_letter()) {
        return pattern.drive_letter() == index.drive_letter();
    }

    // Check if pattern requires specific path prefix
    if (pattern.has_path_prefix()) {
        return index.root_path().starts_with(pattern.path_prefix());
    }

    return true;  // Could match any volume
}
```

---

## Concurrency Optimization

### Parallel Volume Indexing

Multiple volumes indexed simultaneously:

```cpp
void index_all_volumes() {
    std::vector<std::thread> threads;

    for (auto& volume : volumes) {
        threads.emplace_back([&volume]() {
            volume.index();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}
```

**Note**: Each volume has independent I/O, so parallelism is effective.

### Lock-Free Progress Counters

```cpp
class ProgressTracker {
    std::atomic<size_t> files_processed{0};
    std::atomic<size_t> bytes_read{0};

public:
    void add_files(size_t count) {
        files_processed.fetch_add(count, std::memory_order_relaxed);
    }

    void add_bytes(size_t count) {
        bytes_read.fetch_add(count, std::memory_order_relaxed);
    }

    size_t get_files() const {
        return files_processed.load(std::memory_order_relaxed);
    }
};
```

**Note**: `memory_order_relaxed` is sufficient for counters - no ordering guarantees needed.

### UI Thread Isolation

Background work never blocks the UI:

```cpp
// Background thread posts results
void search_thread() {
    while (searching) {
        auto results = find_next_batch();
        PostMessage(hwnd, WM_SEARCH_RESULTS, 0, (LPARAM)results);
    }
}

// UI thread processes messages
LRESULT OnSearchResults(UINT, WPARAM, LPARAM lParam, BOOL&) {
    auto* results = (Results*)lParam;
    update_list_view(*results);
    return 0;
}
```

---

## Compiler Optimization

### Optimization Flags

```xml
<ClCompile>
  <Optimization>Full</Optimization>           <!-- /O2 -->
  <InlineFunctionExpansion>AnySuitable</InlineFunctionExpansion>  <!-- /Ob2 -->
  <IntrinsicFunctions>true</IntrinsicFunctions>  <!-- /Oi -->
  <FavorSizeOrSpeed>Speed</FavorSizeOrSpeed>     <!-- /Ot -->
  <BufferSecurityCheck>false</BufferSecurityCheck>  <!-- /GS- -->
  <WholeProgramOptimization>true</WholeProgramOptimization>  <!-- /GL -->
</ClCompile>
<Link>
  <LinkTimeCodeGeneration>UseLinkTimeCodeGeneration</LinkTimeCodeGeneration>  <!-- /LTCG -->
  <EnableCOMDATFolding>true</EnableCOMDATFolding>  <!-- /OPT:ICF -->
  <OptimizeReferences>true</OptimizeReferences>    <!-- /OPT:REF -->
</Link>
```

### Impact of Optimizations

| Optimization | Impact |
|--------------|--------|
| /O2 | 2-3x faster than /Od |
| /Ob2 (inlining) | 10-20% faster for small functions |
| /Oi (intrinsics) | 5-10% faster for math/memory ops |
| /LTCG | 5-15% faster (cross-module inlining) |
| /GS- | 1-2% faster (no stack checks) |

---

## Profiling Techniques

### Windows Performance Analyzer

```batch
:: Start ETW trace
xperf -on PROC_THREAD+LOADER+PROFILE -stackwalk Profile

:: Run UFFS
uffs.exe

:: Stop trace
xperf -d trace.etl

:: Analyze
wpa trace.etl
```

### Visual Studio Profiler

1. Debug → Performance Profiler
2. Select "CPU Usage"
3. Run application
4. Analyze hot paths

### Key Metrics to Monitor

| Metric | Target | Concern |
|--------|--------|---------|
| MFT read time | <500ms | I/O bottleneck |
| Parse time | <200ms | CPU bottleneck |
| Memory usage | <500MB | Memory pressure |
| Search latency | <100ms | Algorithm issue |

---

## Performance Anti-Patterns

### Avoid These Patterns

```cpp
// BAD: String concatenation in loop
std::wstring path;
for (auto& component : components) {
    path = path + L"\\" + component;  // O(n²) allocations
}

// GOOD: Reserve and append
std::wstring path;
path.reserve(estimated_length);
for (auto& component : components) {
    path += L'\\';
    path += component;
}

// BAD: Virtual function in hot loop
for (auto& record : records) {
    matcher->match(record);  // Virtual call overhead
}

// GOOD: Template-based dispatch
template<typename Matcher>
void search(Matcher& matcher) {
    for (auto& record : records) {
        matcher.match(record);  // Inlined
    }
}

// BAD: Lock per operation
for (auto& item : items) {
    lock.acquire();
    results.push_back(item);
    lock.release();
}

// GOOD: Batch under single lock
std::vector<Item> batch;
batch.reserve(BATCH_SIZE);
for (auto& item : items) {
    batch.push_back(item);
    if (batch.size() >= BATCH_SIZE) {
        lock.acquire();
        results.insert(results.end(), batch.begin(), batch.end());
        lock.release();
        batch.clear();
    }
}
```

---

## Summary

UFFS achieves 5000x speedup through:

| Layer | Technique | Impact |
|-------|-----------|--------|
| I/O | Direct MFT reading | 100x fewer syscalls |
| I/O | Bitmap skip optimization | 60-80% less I/O |
| I/O | Async completion ports | 2-3x throughput |
| Memory | Bit-packing | 50% less memory |
| Memory | ASCII optimization | 40% less name storage |
| Memory | Lazy path resolution | 200MB saved |
| Algorithm | Boyer-Moore-Horspool | O(n/m) search |
| Algorithm | Pattern type detection | Optimal matcher |
| Concurrency | Parallel indexing | Linear scaling |
| Compiler | LTCG + full optimization | 20-30% faster |

---

## See Also

- [01-architecture-overview.md](01-architecture-overview.md) - System architecture
- [02-mft-reading-deep-dive.md](02-mft-reading-deep-dive.md) - I/O optimization details
- [03-concurrency-deep-dive.md](03-concurrency-deep-dive.md) - Threading patterns
- [05-pattern-matching-engine.md](05-pattern-matching-engine.md) - Algorithm details
