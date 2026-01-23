# Rust MFT Implementation: Settings & Assumptions

**Purpose**: Document all configuration settings, assumptions, and implementation details of the Rust MFT reader for comparison with the C++ implementation.

**Date**: 2026-01-23  
**Benchmark Drive**: S: (HDD, ~12GB MFT, 11.7M records, 62 extents)

---

## 1. Performance Comparison Summary

| Implementation | Time | MFT Read Speed | Records/sec |
|----------------|------|----------------|-------------|
| **C++ (reference)** | **40.759s** | **281.73 MB/s** | 288,491 |
| Rust pipelined | 76.157s | 150.78 MB/s | 154,400 |
| Rust iocp-parallel | 79.984s | 143.57 MB/s | 147,012 |
| Rust pipelined-parallel | 88.281s | 130.07 MB/s | 133,195 |

**Gap**: Rust is ~1.9x slower than C++ on this HDD workload.

---

## 2. Windows API Flags

### File Handle Creation (`CreateFileW`)

| Flag | Rust | C++ | Notes |
|------|------|-----|-------|
| `FILE_READ_DATA` | ✅ 0x0001 | ? | Read access |
| `FILE_READ_ATTRIBUTES` | ✅ | ? | Attribute access |
| `SYNCHRONIZE` | ✅ | ? | Sync access |
| `FILE_SHARE_READ` | ✅ | ? | Share mode |
| `FILE_SHARE_WRITE` | ✅ | ? | Share mode |
| `FILE_SHARE_DELETE` | ✅ | ? | Share mode |
| `FILE_FLAG_BACKUP_SEMANTICS` | ✅ | ? | Required for volume access |
| `FILE_FLAG_NO_BUFFERING` | ✅ | ? | Direct I/O, bypasses cache |
| `FILE_FLAG_OVERLAPPED` | ✅ (IOCP only) | ? | Async I/O for IOCP |
| `FILE_FLAG_SEQUENTIAL_SCAN` | ❌ | ? | Hint for sequential access |

**Question for C++ team**: What flags do you use? Do you use `FILE_FLAG_SEQUENTIAL_SCAN`?

---

## 3. I/O Configuration Constants

### Chunk Sizes (per read operation)

| Setting | Rust Value | C++ Value | Notes |
|---------|------------|-----------|-------|
| SSD chunk size | 8 MB | ? | Large sequential reads |
| HDD chunk size | 4 MB | ? | Balance seek vs throughput |
| Max merged chunk | 1 GB | ? | Cap to avoid u32 overflow |

### Buffer Alignment

| Setting | Rust Value | C++ Value | Notes |
|---------|------------|-----------|-------|
| Sector size | 512 bytes | ? | Standard NTFS sector |
| Buffer alignment | 512 bytes | ? | Required for `FILE_FLAG_NO_BUFFERING` |

### Concurrency Settings

| Setting | Rust Value | C++ Value | Notes |
|---------|------------|-----------|-------|
| IOCP concurrent reads | 8 | ? | Overlapped reads in flight |
| Pipeline depth | 3 | ? | Buffers in channel |
| Rayon thread pool | System default (24 on test machine) | ? | Parallel parsing |

**Question for C++ team**: How many concurrent reads do you issue? What's your async I/O depth?

---

## 4. Read Modes Implemented

### Rust Read Modes

| Mode | Description | I/O Pattern | Parsing |
|------|-------------|-------------|---------|
| `streaming` | Sequential read, immediate parse | Sync, sequential | Single-threaded |
| `prefetch` | Double-buffered | Sync, sequential | Single-threaded |
| `parallel` | Read all, then parse | Sync, sequential | Rayon parallel |
| `pipelined` | Reader thread + parser thread | Sync in reader thread | Single-threaded |
| `pipelined-parallel` | Reader thread + Rayon | Sync in reader thread | Rayon parallel |
| `iocp-parallel` | IOCP overlapped I/O | Async, 8 in flight | Single-threaded |

**Question for C++ team**: What's your I/O strategy? Async with IOCP? How many reads in flight?

---

## 5. MFT Bitmap Usage

| Feature | Rust | C++ | Notes |
|---------|------|-----|-------|
| Load $MFT bitmap | ✅ | ? | `$Bitmap` attribute of $MFT |
| Skip unused records | ✅ | ? | Don't read records marked as free |
| Skip percentage | ~29.5% on test drive | ? | Significant I/O savings |

**Question for C++ team**: Do you use the MFT bitmap to skip unused records?

---

## 6. Extent Handling (Fragmented MFT)

| Feature | Rust | C++ | Notes |
|---------|------|-----|-------|
| `FSCTL_GET_RETRIEVAL_POINTERS` | ✅ | ? | Get MFT extent map |
| VCN-to-LCN mapping | ✅ | ? | Handle fragmented MFT |
| Merge adjacent chunks | ✅ | ? | Reduce syscall count |
| Physical adjacency check | ✅ | ? | Only merge if disk-adjacent |

Test drive has 62 extents (fragmented MFT).

---

## 7. Record Parsing

### USA Fixup (Update Sequence Array)

| Feature | Rust | C++ | Notes |
|---------|------|-----|-------|
| In-place fixup | ✅ | ? | Zero-copy, modify buffer directly |
| Sector size | 512 bytes | ? | Standard |
| Validate check values | ✅ | ? | Detect torn writes |

### Attribute Parsing

| Attribute | Rust | C++ | Notes |
|-----------|------|-----|-------|
| `$STANDARD_INFORMATION` (0x10) | ✅ | ? | Timestamps, flags |
| `$FILE_NAME` (0x30) | ✅ | ? | Names, parent FRS |
| `$DATA` (0x80) | ✅ | ? | File size, streams |
| `$ATTRIBUTE_LIST` (0x20) | ✅ | ? | Extension records |
| `$REPARSE_POINT` (0xC0) | ✅ | ? | Symlinks, junctions |

### Extension Record Merging

| Feature | Rust | C++ | Notes |
|---------|------|-----|-------|
| Merge extensions | ✅ | ? | Combine base + extension records |
| HashMap for pending | ✅ | ? | O(1) lookup |

---

## 8. Memory Allocation Strategy

| Feature | Rust | C++ | Notes |
|---------|------|-----|-------|
| Thread-local record buffer | ✅ 4KB | ? | Avoid per-record alloc |
| Pre-allocated chunk buffers | ✅ | ? | Reuse across reads |
| Buffer pool for IOCP | ✅ | ? | Reuse overlapped buffers |
| SmallVec for UTF-16 | ✅ 128 chars stack | ? | Avoid heap for short names |

---

## 9. Questions for C++ Team

1. **I/O Flags**: What `CreateFileW` flags do you use? Specifically:
   - Do you use `FILE_FLAG_SEQUENTIAL_SCAN`?
   - Any other optimization flags?

2. **Async I/O**: 
   - Do you use IOCP or synchronous reads?
   - If IOCP, how many concurrent reads in flight?
   - What's your chunk/buffer size?

3. **Read-ahead/Prefetch**:
   - Do you issue explicit prefetch hints?
   - Any OS-level read-ahead tuning?

4. **MFT Bitmap**:
   - Do you load and use the $MFT bitmap?
   - Do you skip reading unused record clusters?

5. **Parsing**:
   - Single-threaded or parallel parsing?
   - Any SIMD optimizations?

6. **Memory**:
   - Pre-allocated buffers or dynamic allocation?
   - Any memory-mapped I/O?

7. **Other Optimizations**:
   - Any CPU affinity settings?
   - Any NUMA awareness?
   - Priority boost for I/O thread?

---

## 10. Potential Rust Improvements to Investigate

Based on the 1.9x gap, areas to investigate:

1. **Larger chunk sizes**: Try 8-16 MB for HDD
2. **More IOCP concurrency**: Try 16-32 concurrent reads
3. **FILE_FLAG_SEQUENTIAL_SCAN**: Add this hint
4. **Reduce parsing overhead**: Profile hot paths
5. **Memory-mapped I/O**: Alternative to ReadFile
6. **CPU affinity**: Pin I/O thread to specific core
7. **Batch syscalls**: Reduce SetFilePointerEx + ReadFile pairs

