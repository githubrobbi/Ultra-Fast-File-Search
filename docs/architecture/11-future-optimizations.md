# Future Performance Optimizations

**Purpose**: Document potential optimizations to push UFFS performance beyond current levels.  
**Date**: 2026-01-23  
**Status**: Research / Planning  
**Current Baseline**: 40.759s for 12GB MFT (281.73 MB/s, 288K records/sec)

---

## 1. Executive Summary

The current C++ implementation is already highly optimized:
- Vec indexed by FRS (no HashMap overhead)
- String pool for file names (minimal allocations)
- Lazy path resolution (defer work to search time)
- Sequential buffered I/O with `FILE_FLAG_SEQUENTIAL_SCAN`
- Single-threaded parsing (appropriate for I/O-bound HDD workloads)

This document explores optimizations that could provide additional 10-50% improvement depending on hardware (HDD vs SSD).

---

## 2. Memory Layout Optimizations

### 2.1 Shrink FileEntry Struct

**Current**: 28 bytes per entry

```cpp
struct FileEntry {
    uint64_t parent_frs;      // 8 bytes
    uint64_t file_size;       // 8 bytes
    uint32_t flags;           // 4 bytes
    uint32_t name_offset;     // 4 bytes
    uint16_t name_length;     // 2 bytes
    uint16_t padding;         // 2 bytes
};  // 28 bytes
```

**Proposed**: 24 bytes per entry

```cpp
struct FileEntry {
    uint32_t parent_frs;      // 4 bytes (MFT rarely exceeds 4B records)
    uint32_t name_offset;     // 4 bytes
    uint64_t file_size;       // 8 bytes
    uint16_t name_length;     // 2 bytes
    uint16_t flags;           // 2 bytes
    uint32_t reserved;        // 4 bytes (alignment)
};  // 24 bytes, naturally aligned
```

**Impact**: 
- 11.7M records × 4 bytes saved = **47MB less memory**
- Better cache line utilization (2.67 entries per 64-byte cache line → 2.67)
- Expected improvement: **5-10%** on parsing phase

**Risk**: Limits MFT to 4 billion records (not a practical concern)

---

### 2.2 Huge Pages for Large Allocations

Use 2MB huge pages for:
- MFT read buffer (~12GB)
- FileEntry vector (~280-330MB)
- String pool (~300-500MB)

```cpp
void* buffer = VirtualAlloc(
    NULL,
    size,
    MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
    PAGE_READWRITE
);
```

**Impact**:
- Reduces TLB (Translation Lookaside Buffer) misses
- Fewer page table walks during sequential access
- Expected improvement: **5-15%** on parsing and search phases

**Requirements**:
- Process must have `SeLockMemoryPrivilege`
- System must have contiguous 2MB physical pages available
- May require running as Administrator

---

## 3. CPU Optimizations

### 3.1 Software Prefetching During Parsing

Prefetch MFT records ahead of current parsing position:

```cpp
for (size_t i = 0; i < record_count; i++) {
    // Prefetch 16 records ahead (16KB)
    _mm_prefetch((char*)&buffer[(i + 16) * 1024], _MM_HINT_T0);
    parse_record(&buffer[i * 1024]);
}
```

**Impact**:
- Keeps L1/L2 cache warm with upcoming data
- Hides memory latency
- Expected improvement: **2-10%** depending on memory speed

**Tuning**: Prefetch distance depends on:
- Memory latency (~60-100ns for DDR4)
- Parse time per record (~1-2μs)
- Optimal distance: 8-32 records ahead

---

### 3.2 SIMD String Search (AVX2/AVX-512)

For substring search across 11.7M file names:

```cpp
// AVX2: Compare 32 bytes at once
__m256i pattern = _mm256_set1_epi8(first_char);
__m256i data = _mm256_loadu_si256((__m256i*)name);
__m256i cmp = _mm256_cmpeq_epi8(data, pattern);
int mask = _mm256_movemask_epi8(cmp);
// Check mask for potential matches
```

**Impact**:
- Search 32 characters simultaneously (AVX2) or 64 (AVX-512)
- Expected improvement: **3-5x faster search** (not indexing)

**Implementation Options**:
1. Hand-written SIMD intrinsics
2. Use library: `hyperscan`, `vectorscan`, or `simdutf`
3. Compiler auto-vectorization (less reliable)

---

### 3.3 Skip Unused MFT Attributes

During parsing, only extract required attributes:

| Attribute | Type | Required for Indexing |
|-----------|------|----------------------|
| `$FILE_NAME` (0x30) | Name, parent FRS | ✅ Yes |
| `$STANDARD_INFORMATION` (0x10) | Flags, timestamps | ✅ Yes (flags only) |
| `$DATA` (0x80) | File size | ✅ Yes |
| `$ATTRIBUTE_LIST` (0x20) | Extension records | ⚠️ Only for large files |
| `$REPARSE_POINT` (0xC0) | Symlinks | ❌ Skip during index |
| Others | Various | ❌ Skip |

**Impact**:
- Less parsing work per record
- Expected improvement: **2-5%**

---

## 4. I/O Optimizations

### 4.1 ReadFileScatter for Fragmented MFT

For MFT with many extents (e.g., 62 extents on test drive):

```cpp
FILE_SEGMENT_ELEMENT segments[MAX_EXTENTS];
// Fill segments with extent addresses
ReadFileScatter(handle, segments, total_size, NULL, &overlapped);
```

**Impact**:
- Single syscall instead of N seeks + reads
- Expected improvement: **2-5%** for highly fragmented MFT

**Limitation**: Requires aligned buffers and sizes.

---

### 4.2 Lock Pages in Working Set

Prevent OS from paging out buffers during processing:

```cpp
VirtualLock(mft_buffer, mft_size);
VirtualLock(entries.data(), entries.size() * sizeof(FileEntry));
```

**Impact**:
- Guarantees no page faults during parsing
- Expected improvement: **1-3%** (more on memory-constrained systems)

**Requirement**: Process needs `SeLockMemoryPrivilege`.

---

## 5. SSD-Specific Optimizations

These optimizations only benefit SSD/NVMe where I/O is not the bottleneck.

### 5.1 Parallel Parsing

On NVMe SSD (3-7 GB/s read speed), single-threaded parsing becomes the bottleneck.

```cpp
// Read entire MFT first
ReadFile(handle, mft_buffer, mft_size, ...);

// Parse in parallel
#pragma omp parallel for schedule(static)
for (size_t i = 0; i < record_count; i++) {
    FileEntry entry = parse_record(&mft_buffer[i * 1024]);
    entries[i] = entry;  // Direct index, no synchronization needed
}
```

**Impact**:
- Scales with core count
- Expected improvement: **20-40%** on SSD with 4+ cores

**Why it works**:
- Each record is independent (no data dependencies)
- Direct Vec indexing avoids synchronization
- String pool needs thread-local buffers + merge

---

### 5.2 IOCP with Sequential Ordering (SSD Only)

For SSD, overlapped I/O can help if ordered correctly:

```cpp
// Issue multiple reads, but process completions in order
for (int i = 0; i < QUEUE_DEPTH; i++) {
    ReadFile(handle, buffers[i], CHUNK_SIZE, NULL, &overlapped[i]);
}
// Process completions in submission order
```

**Impact**:
- Keeps NVMe queue full
- Expected improvement: **10-20%** on NVMe

**Note**: This hurts HDD performance (seek thrashing). Only enable for SSD.

---

## 6. Post-Index Optimizations

### 6.1 Compress String Pool

After indexing, compress the string pool for better cache utilization:

```cpp
// LZ4 is fast enough for real-time decompression
compressed_pool = LZ4_compress_default(string_pool, ...);
```

**Impact**:
- 2-4x smaller string pool
- Better cache hit rate during search
- Expected improvement: **5-10%** on search phase

**Trade-off**: Adds decompression latency when displaying results.

---

### 6.2 Build Search Index

For repeated searches, build an inverted index or suffix array:

```cpp
// Suffix array for substring search
std::vector<uint32_t> suffix_array = build_suffix_array(string_pool);
```

**Impact**:
- O(log n) search instead of O(n) linear scan
- Expected improvement: **10-100x** for repeated searches

**Trade-off**: Adds index build time and memory.

---

## 7. Summary: Priority Matrix

| Optimization | HDD Impact | SSD Impact | Effort | Priority |
|--------------|------------|------------|--------|----------|
| Shrink FileEntry | 5-10% | 5-10% | Low | **High** |
| Huge pages | 5-10% | 10-15% | Low | **High** |
| Software prefetch | 2-5% | 5-10% | Low | Medium |
| SIMD string search | N/A | N/A | Medium | Medium (search) |
| Skip unused attributes | 2-5% | 2-5% | Low | Medium |
| Parallel parsing | 0% | 20-40% | Medium | **High (SSD)** |
| ReadFileScatter | 2-5% | 0% | Medium | Low |
| Lock pages | 1-3% | 1-3% | Low | Low |
| Compress string pool | 5-10% | 5-10% | Medium | Low |
| Search index | N/A | N/A | High | Low (one-time search) |

---

## 8. Recommended Implementation Order

### Phase 1: Quick Wins (1-2 days)
1. Shrink FileEntry to 24 bytes
2. Add software prefetching
3. Skip unused attributes during parsing

### Phase 2: Memory Optimizations (2-3 days)
4. Implement huge page support (with fallback)
5. Add VirtualLock for critical buffers

### Phase 3: SSD Optimization (3-5 days)
6. Implement parallel parsing with OpenMP
7. Add SSD detection and mode switching

### Phase 4: Search Optimization (5-7 days)
8. SIMD string search implementation
9. Optional: String pool compression

---

## 9. Theoretical Limits

### HDD Limit
- Sequential read: 150-280 MB/s (physical limit)
- Current: 281.73 MB/s (already at limit!)
- **Conclusion**: HDD performance is maxed out. Only reduce CPU overhead.

### SSD Limit
- NVMe sequential read: 3,000-7,000 MB/s
- Current bottleneck: Single-threaded parsing
- **Conclusion**: Parallel parsing is the key unlock for SSD.

### Memory Bandwidth Limit
- DDR4-3200: ~50 GB/s
- Parsing 12GB MFT: ~12GB read + ~0.5GB write = 12.5GB
- At 50 GB/s: theoretical minimum = 0.25 seconds
- **Conclusion**: Memory is not the bottleneck.

---

## 10. Benchmarking Plan

Before implementing, establish baselines:

```
Metric                  | HDD Baseline | SSD Baseline | Target
------------------------|--------------|--------------|--------
Total time              | 40.759s      | TBD          | -20%
MFT read time           | ~38s         | TBD          | -10%
Parse time              | ~2s          | TBD          | -50%
Index build time        | <1s          | TBD          | -
Search time (cold)      | TBD          | TBD          | -50%
Search time (warm)      | TBD          | TBD          | -50%
Peak memory             | TBD          | TBD          | -20%
```

---

## See Also

- [06-rust-mft-implementation-comparison.md](06-rust-mft-implementation-comparison.md) - Rust vs C++ analysis
- [10-performance-guide.md](10-performance-guide.md) - Current performance documentation
- [02-mft-reading-deep-dive.md](02-mft-reading-deep-dive.md) - MFT reading internals

