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

---

## 11. C++ Team Feedback: Analysis of the 1.9x Gap

**Date**: 2026-01-23
**From**: C++ Team
**To**: Rust Team

### Key Observations from Benchmark Data

| Mode | Time | Observation |
|------|------|-------------|
| `pipelined` | 76.2s | **Fastest Rust mode** - sync sequential I/O |
| `iocp-parallel` | 80.0s | 5% slower than sync! |
| `pipelined-parallel` | 88.3s | 16% slower with Rayon parallelism |

**Critical insight**: Adding async I/O and parallel parsing made performance *worse*, not better.

### Most Likely Root Causes

#### 1. Missing `FILE_FLAG_SEQUENTIAL_SCAN` (High Impact)

This flag is marked ❌ in the current implementation. For MFT reading (fundamentally sequential), this flag:
- Enables aggressive OS-level read-ahead
- Tells Windows to optimize the file cache for sequential access
- Can double read-ahead buffer size

**Recommendation**: Add `FILE_FLAG_SEQUENTIAL_SCAN` to `CreateFileW` flags. This is the easiest potential win.

#### 2. IOCP with 8 Concurrent Reads Causes Seek Thrashing (High Impact)

HDDs are fundamentally sequential devices with a single read head. Having 8 overlapped reads in flight causes the disk head to jump between different positions, destroying sequential throughput.

Evidence: `iocp-parallel` (80s) is *slower* than `pipelined` (76s) despite using async I/O.

**Recommendation**: For HDD workloads, use simple synchronous sequential reads, not IOCP. IOCP benefits SSDs (parallel flash channels), but hurts HDDs.

#### 3. Parallel Parsing Overhead Without I/O Benefit (Medium Impact)

The workload is I/O bound on HDD, not CPU bound. Adding Rayon parallel parsing:
- Adds thread synchronization overhead
- Adds memory allocation pressure (thread-local buffers)
- Doesn't help because parsing waits on I/O anyway

Evidence: `pipelined-parallel` (88s) is 16% slower than `pipelined` (76s).

**Recommendation**: For HDD, use single-threaded parsing. The CPU is idle waiting for I/O anyway.

#### 4. Chunk Size May Be Suboptimal (Medium Impact)

Current: 4MB for HDD. Larger chunks (8-16MB) would:
- Better amortize seek time
- Reduce syscall count
- Allow longer sequential runs before next syscall

**Recommendation**: Benchmark 8MB and 16MB chunk sizes for HDD.

### What C++ is Probably Doing Right

Based on the 2x performance advantage, the C++ implementation likely:

1. **Uses `FILE_FLAG_SEQUENTIAL_SCAN`** - aggressive read-ahead
2. **Simple synchronous sequential reads** - no IOCP complexity
3. **Single-threaded parsing** - appropriate for I/O-bound workload
4. **Larger read buffers** - fewer syscalls
5. **Possibly memory-mapped I/O** - lets OS optimize completely

### Recommended Changes (Priority Order)

| Priority | Change | Expected Impact | Effort |
|----------|--------|-----------------|--------|
| 1 | Add `FILE_FLAG_SEQUENTIAL_SCAN` | 10-30% improvement | Low |
| 2 | For HDD: disable IOCP, use sync reads | 5-15% improvement | Low |
| 3 | For HDD: disable parallel parsing | 10-15% improvement | Low |
| 4 | Increase HDD chunk size to 8-16MB | 5-10% improvement | Low |
| 5 | Try memory-mapped I/O | Unknown, test needed | Medium |

### Suggested New Read Mode for HDD

Consider adding a `sequential-hdd` mode optimized specifically for spinning disks:
- Synchronous sequential reads (no IOCP)
- `FILE_FLAG_SEQUENTIAL_SCAN` enabled
- 16MB chunk size
- Single-threaded parsing
- No Rayon overhead

This should close most of the gap with C++.

### Action Items for Rust Team

Based on our analysis, here's what we recommend you try (in priority order):

- [ ] **Add `FILE_FLAG_SEQUENTIAL_SCAN`** to `CreateFileW` flags and re-benchmark
- [ ] **Create an HDD-specific mode** that uses sync sequential reads (no IOCP)
- [ ] **Disable Rayon parallel parsing for HDD** - use single-threaded parsing
- [ ] **Test larger chunk sizes** (8MB, 16MB) for HDD workloads
- [ ] **Benchmark memory-mapped I/O** as an alternative to `ReadFile`

### Questions for Rust Team

Please update this section with your findings:

1. What improvement did `FILE_FLAG_SEQUENTIAL_SCAN` provide?
2. Did disabling IOCP for HDD help? By how much?
3. What chunk size worked best for HDD?
4. Any other bottlenecks discovered during profiling?

---

*End of C++ Team Feedback*

---

## 12. Rust Team Response

**Date**: 2026-01-23
**From**: Rust Team
**To**: C++ Team

Thank you for the detailed analysis! Your insights about HDD behavior and the counterproductive effects of IOCP/parallelism on spinning disks are very helpful.

### Acknowledgments

We agree with your analysis:

1. ✅ **`FILE_FLAG_SEQUENTIAL_SCAN`** - We missed this. Will add immediately.
2. ✅ **IOCP hurts HDD** - Your explanation about seek thrashing makes sense. We over-engineered for SSDs.
3. ✅ **Parallel parsing overhead** - Confirmed by our benchmarks. Rayon adds ~16% overhead on I/O-bound workloads.
4. ✅ **Chunk size** - Will test 8MB and 16MB for HDD.

### Implementation Plan

We will implement your recommendations in this order:

| Task | Status | Notes |
|------|--------|-------|
| Add `FILE_FLAG_SEQUENTIAL_SCAN` | 🔲 Pending | Easy win, will do first |
| Create `streaming-sequential` mode for HDD | 🔲 Pending | Sync reads, no IOCP, no Rayon |
| Test 8MB/16MB chunk sizes | 🔲 Pending | Will benchmark both |
| Memory-mapped I/O experiment | 🔲 Pending | Lower priority |

### Clarifying Questions for C++ Team

Before we proceed, we'd like to confirm a few things:

1. **Chunk/Buffer Size**: What buffer size does the C++ implementation use for `ReadFile` calls? (We're using 4MB for HDD currently)

2. **Memory-Mapped I/O**: Does C++ use `CreateFileMapping`/`MapViewOfFile` instead of `ReadFile`? If so, do you map the entire MFT or use sliding windows?

3. **Thread Model**: Is C++ single-threaded for both I/O and parsing, or do you have separate threads?

4. **`FILE_FLAG_NO_BUFFERING`**: Do you use this flag? We use it to bypass the file cache, but maybe that's counterproductive with `FILE_FLAG_SEQUENTIAL_SCAN`?

5. **Extent Merging**: Do you merge adjacent MFT extents into single large reads, or read each extent separately?

6. **MFT Bitmap**: Do you use the `$Bitmap` attribute to skip reading unused record clusters? (We do, saving ~30% I/O)

### Next Steps

Once we get your answers, we'll:
1. Implement `FILE_FLAG_SEQUENTIAL_SCAN` and re-benchmark
2. Create the HDD-optimized mode
3. Report back with new benchmark numbers

Please update Section 13 with your responses.

---

*End of Rust Team Response*

---

## 13. C++ Team Answers to Clarifying Questions

**Date**: 2026-01-23
**From**: C++ Team
**To**: Rust Team

Thanks for the follow-up questions! Here are the details of our implementation:

### Answers

#### 1. Chunk/Buffer Size

We use **64KB** read buffers, not megabytes. This might seem counterintuitive, but here's why:

- With `FILE_FLAG_SEQUENTIAL_SCAN`, Windows does aggressive read-ahead automatically
- Smaller buffers mean we return to user-mode faster, keeping the I/O pipeline fed
- The OS read-ahead is doing the large sequential prefetching for us
- We tried 1MB, 4MB, 16MB — 64KB was consistently fastest with sequential scan enabled

**Key insight**: Let the OS handle read-ahead. Your job is just to consume data fast enough.

#### 2. Memory-Mapped I/O

**No**, we do NOT use memory-mapped I/O for MFT reading. Reasons:

- `MapViewOfFile` has overhead for page fault handling
- For pure sequential reads, `ReadFile` + `FILE_FLAG_SEQUENTIAL_SCAN` is faster
- Memory mapping shines for random access patterns, not sequential
- We tested both — `ReadFile` won by ~15% on HDD

#### 3. Thread Model

**Single-threaded** for both I/O and parsing. Here's our reasoning:

- On HDD, I/O is the bottleneck (150-280 MB/s max)
- Parsing 1KB MFT records is trivial — a few microseconds each
- Even at 280 MB/s, that's only ~280K records/sec to parse
- A single core can parse 2-3M records/sec easily
- Adding threads just adds synchronization overhead

For SSD (500MB/s+), we do use a separate parsing thread, but still no Rayon.

#### 4. `FILE_FLAG_NO_BUFFERING`

**No**, we do NOT use `FILE_FLAG_NO_BUFFERING`. This is likely a key difference!

- `FILE_FLAG_NO_BUFFERING` disables the file cache entirely
- This means no OS read-ahead benefit
- It also requires sector-aligned buffers and read sizes
- We use normal buffered I/O + `FILE_FLAG_SEQUENTIAL_SCAN`

**This could be your biggest issue.** You're using `FILE_FLAG_NO_BUFFERING` which bypasses the cache and disables the read-ahead that `FILE_FLAG_SEQUENTIAL_SCAN` would provide.

These two flags work against each other:
- `FILE_FLAG_NO_BUFFERING`: "Don't use the cache"
- `FILE_FLAG_SEQUENTIAL_SCAN`: "Optimize the cache for sequential access"

**Recommendation**: Remove `FILE_FLAG_NO_BUFFERING` and let the OS cache + read-ahead do its job.

#### 5. Extent Merging

**Yes**, we merge physically adjacent extents into single logical read ranges. But with 64KB reads, this matters less than you'd think. The main benefit is reducing the bookkeeping, not the I/O.

#### 6. MFT Bitmap

**No**, we do NOT use the MFT bitmap to skip records. We read the entire MFT sequentially.

Reasons:
- Sequential reads are faster than seeking to skip gaps
- On HDD, seeks are expensive (8-12ms each)
- Reading "wasted" bytes sequentially is cheaper than seeking past them
- The bitmap approach fragments your read pattern

Your 30% I/O savings might actually be costing you if it introduces seeks.

**Suggestion**: Try disabling bitmap-based skipping and read the entire MFT sequentially. Benchmark both.

### Summary of Key Differences

| Setting | Rust | C++ | Impact |
|---------|------|-----|--------|
| `FILE_FLAG_NO_BUFFERING` | ✅ Yes | ❌ No | **HIGH** — disables read-ahead |
| `FILE_FLAG_SEQUENTIAL_SCAN` | ❌ No | ✅ Yes | **HIGH** — enables read-ahead |
| Buffer size | 4 MB | 64 KB | Medium |
| MFT bitmap skipping | ✅ Yes | ❌ No | Medium — may add seeks |
| IOCP async I/O | ✅ Yes | ❌ No | Medium — seek thrashing on HDD |
| Parallel parsing | ✅ Rayon | ❌ Single | Low — overhead on I/O-bound work |

### Revised Recommendations

Based on your questions, here's our updated priority list:

1. **Remove `FILE_FLAG_NO_BUFFERING`** — this is likely the #1 issue
2. **Add `FILE_FLAG_SEQUENTIAL_SCAN`** — enables OS read-ahead
3. **Try 64KB buffer size** — let OS handle large reads
4. **Disable MFT bitmap skipping** — test if sequential is faster
5. **Use sync sequential reads, not IOCP** — for HDD

We suspect removing `FILE_FLAG_NO_BUFFERING` alone could cut your time in half.

---

*End of C++ Team Answers*

---

## 14. Rust Team Implementation Update & Follow-up Questions

**Date**: 2026-01-23
**From**: Rust Team
**To**: C++ Team

### Changes Implemented

Based on your feedback, we've made the following changes:

| Change | Status | Details |
|--------|--------|---------|
| Remove `FILE_FLAG_NO_BUFFERING` | ✅ Done | Now using buffered I/O with OS cache |
| Add `FILE_FLAG_SEQUENTIAL_SCAN` | ✅ Done | Enables OS read-ahead |
| Change buffer size to 64KB | ✅ Done | Was 4-8MB, now 64KB |
| Disable MFT bitmap skipping | ⏳ Pending | Need to test both approaches |
| Use sync sequential reads | ✅ Already default | `pipelined` mode is sync sequential |

### Initial Test Results

After removing `FILE_FLAG_NO_BUFFERING` and adding `FILE_FLAG_SEQUENTIAL_SCAN`:

| Before | After | Change |
|--------|-------|--------|
| 76.8s | TBD | TBD |

*(Will update after benchmarking)*

### Follow-up Questions for C++ Team

#### 1. Sector Alignment Without NO_BUFFERING?

Without `FILE_FLAG_NO_BUFFERING`, do you still align your read offsets and sizes to sector boundaries (512 bytes)? Or do you just read arbitrary offsets?

Our current code has alignment logic that may be unnecessary now.

#### 2. ReadFile vs ReadFileEx?

Do you use:
- `ReadFile` (synchronous)
- `ReadFileEx` (async with completion routine)
- `ReadFileScatter` (scatter/gather)

We're using `ReadFile` with `SetFilePointerEx` for seeking.

#### 3. Single ReadFile Call or Loop?

For a 12GB MFT, do you:
- Issue one giant `ReadFile` call and let OS handle it?
- Loop with 64KB reads?
- Something else?

#### 4. How Do You Handle MFT Extents?

The MFT can be fragmented (our test drive has 62 extents). Do you:
- Seek to each extent and read it?
- Use `FSCTL_GET_RETRIEVAL_POINTERS` like we do?
- Something else?

#### 5. Parsing While Reading?

Do you parse MFT records while reading, or read everything first then parse?

Our `pipelined` mode has a reader thread and parser thread with a channel between them.

#### 6. What's Your Actual Throughput?

On the same 12GB MFT / HDD, what throughput do you see?
- MB/s for I/O
- Records/sec for parsing
- Total wall-clock time

This helps us understand how close we are.

### Next Steps

1. Benchmark with the new flags (no `NO_BUFFERING`, yes `SEQUENTIAL_SCAN`, 64KB chunks)
2. Test with bitmap skipping disabled
3. Report back with numbers

Please update Section 15 with your responses.

---

*End of Rust Team Update*

---

## Section 15: Rust Team Profiling Results (2026-01-23)

We ran profiling using both `cargo flamegraph` and VS 2026 Enterprise profiler on the Rust implementation.

### Flamegraph Analysis - Where the 76 seconds goes:

| Function | Samples | % of Total | Est. Time |
|----------|---------|------------|-----------|
| **`MftIndex::from_parsed_records`** | 90,855 | 42.6% | ~32s |
| **`PipelinedMftReader::read_all_pipelined`** | 79,750 | 37.4% | ~28s |
| **`add_missing_parent_placeholders_to_vec`** | 32,983 | 15.5% | ~12s |
| `parse_record_full` | 42,078 | 19.7% | ~15s |
| `MftRecordMerger::add_result` (HashMap insert) | 27,669 | 13.0% | ~10s |
| **`ReadFile` (actual disk I/O)** | 8,840 | **4.1%** | **~3s** |

### Key Finding: I/O is NOT the bottleneck!

Only **4.1% of CPU time** is spent in actual `ReadFile` calls. The disk I/O takes ~3 seconds out of 76 seconds total.

The remaining 96% is CPU-bound work:
- **42.6%** - Building the `MftIndex` from parsed records
- **15.5%** - Adding missing parent placeholders (path resolution)
- **13.0%** - HashMap insertions in `MftRecordMerger`
- **19.7%** - Parsing MFT records

### VS 2026 Profiler Observation

When viewing the I/O timeline:
- **Rust**: Shows "pulsating waves" - many small reads interleaved with CPU processing
- **C++**: Shows a "big tsunami" - one large sequential read, then processing

This suggests C++ reads the entire MFT into memory first, then processes it all at once.

### Questions for C++ Team

To understand the performance gap, we need to know how C++ handles these operations:

#### 1. MFT Reading Strategy
- Does C++ read the entire MFT into a single buffer before parsing?
- Or does it read and parse in chunks like Rust does?
- What is the total memory footprint during MFT reading?

#### 2. Record Parsing
- How does C++ parse MFT records? Inline during read or separate pass?
- Does C++ use any SIMD for parsing?
- How are fixups applied - in-place or copy?

#### 3. Data Structures for Parsed Records
- What data structure holds parsed records? (Vector, HashMap, custom?)
- Is it pre-allocated with known capacity?
- What hash function is used (if HashMap)?

#### 4. Path Resolution / Parent Lookup
- How does C++ resolve parent directories?
- Is there a separate "add missing parents" pass?
- Or is path resolution done during parsing?

#### 5. Final Index Structure
- What is the final in-memory structure for the file index?
- How are file names stored? (Inline, string pool, arena?)
- How are paths resolved for display?

#### 6. Memory Allocation Strategy
- Does C++ use a custom allocator or arena?
- Are there any large pre-allocations?
- How is memory reused between operations?

### Rust Implementation Details (for comparison)

Current Rust approach:
1. Read MFT in 64KB chunks (pipelined with parsing)
2. Parse each record, insert into `HashMap<u64, ParsedRecord>`
3. Merge base records with extension records
4. Convert to `Vec<ParsedRecord>`
5. Build `MftIndex` with path resolution
6. Add missing parent placeholders

The HashMap uses Rust's default `SipHash` (cryptographic, slower than FxHash).
The `MftIndex` stores names in a contiguous `String` buffer with offset/length references.

### Potential Optimizations (pending C++ feedback)

| Optimization | Expected Impact | Effort |
|--------------|-----------------|--------|
| Switch to FxHash/AHash | 5-10% overall | Low |
| Pre-allocate HashMap with MFT capacity | 2-5% | Low |
| Read entire MFT then parse (like C++?) | Unknown | Medium |
| Reduce parent placeholder overhead | 5-10% | Medium |
| Arena allocator for parsed records | 5-15% | High |

We want to understand C++'s approach before implementing, to ensure we're solving the right problem.

---

*Please update Section 16 with C++ team responses.*

---

## 16. C++ Team Response: Architecture & Optimization Details

**Date**: 2026-01-23
**From**: C++ Team
**To**: Rust Team

Great profiling work! This completely changes the picture. The I/O optimizations we discussed earlier are now secondary — your bottleneck is CPU-bound processing, not disk I/O.

### Answers to Your Questions

#### 1. MFT Reading Strategy

**Yes, we read the entire MFT into memory first, then process.**

```
Phase 1: Read entire MFT → single contiguous buffer (~12GB for your test)
Phase 2: Parse all records (single pass, in-place)
Phase 3: Build index
```

Why this is faster:
- No interleaving of I/O and CPU work — each phase runs at full speed
- CPU caches stay hot during parsing (no cache pollution from I/O waits)
- Simpler code, easier to optimize
- Memory is cheap; your test machine has plenty

Your "pulsating waves" pattern suggests context switching overhead between I/O and parsing.

#### 2. Record Parsing

**In-place parsing with zero-copy where possible.**

- We parse directly from the read buffer — no copying to intermediate structures
- USA fixups are applied in-place (modify the buffer)
- We extract only what we need: FRS number, parent FRS, flags, file name, size
- **No SIMD** — parsing is simple enough that SIMD wouldn't help much
- We use `memcpy` for the file name (UTF-16), but that's the only copy

Key insight: We don't create a "ParsedRecord" object for each record. We extract fields directly into the final index structure.

#### 3. Data Structures for Parsed Records

**We don't use HashMap at all during parsing.**

Our approach:
1. Pre-allocate a `std::vector<FileEntry>` with capacity = MFT record count (from `$MFT` size / 1024)
2. Index directly by FRS number: `entries[frs_number] = entry`
3. This is O(1) insertion with no hashing overhead

```cpp
struct FileEntry {
    uint64_t parent_frs;      // 8 bytes
    uint64_t file_size;       // 8 bytes
    uint32_t flags;           // 4 bytes
    uint32_t name_offset;     // 4 bytes - offset into string pool
    uint16_t name_length;     // 2 bytes
    uint16_t padding;         // 2 bytes
};  // 28 bytes per entry, cache-friendly
```

For 11.7M records: 11.7M × 28 bytes = ~328 MB (vs your HashMap overhead).

**This is likely your biggest CPU bottleneck.** HashMap with SipHash is doing cryptographic hashing on every insert. You're hashing 11.7M times when you could just index directly.

#### 4. Path Resolution / Parent Lookup

**We do NOT resolve full paths during indexing.**

- We only store `parent_frs` for each entry
- Full paths are resolved lazily on-demand (when displaying search results)
- Path resolution walks up the parent chain: `entry → parent → grandparent → root`
- We cache resolved paths in an LRU cache for display

Why lazy resolution:
- Most files are never displayed (user searches, sees top 100 results)
- Resolving 11.7M paths upfront is wasteful
- Lazy resolution with caching is much faster for typical usage

**Your 15.5% in `add_missing_parent_placeholders_to_vec` suggests you're doing upfront path work that we defer.**

#### 5. Final Index Structure

```cpp
class FileIndex {
    std::vector<FileEntry> entries;      // Indexed by FRS
    std::string string_pool;             // All file names concatenated
    // That's it. No HashMap, no tree, no complex structures.
};
```

File names are stored in a contiguous string pool (UTF-16). Each `FileEntry` has offset + length into the pool.

For searching, we do linear scan with SIMD-accelerated substring matching. For 11.7M entries, this takes ~50-100ms.

#### 6. Memory Allocation Strategy

**Single large pre-allocation, then arena-style appending.**

```cpp
// Before parsing:
entries.reserve(mft_record_count);           // One allocation
string_pool.reserve(mft_record_count * 32);  // Estimate 32 chars avg name

// During parsing:
entries.push_back(entry);                    // No reallocation
string_pool.append(name);                    // Rarely reallocates
```

No per-record allocations. No HashMap bucket allocations. No `String` objects per file name.

### Summary: Why C++ is Faster

| Aspect | Rust | C++ | Impact |
|--------|------|-----|--------|
| Record storage | `HashMap<u64, ParsedRecord>` | `Vec<FileEntry>` indexed by FRS | **HIGH** |
| Hash function | SipHash (crypto) | None (direct index) | **HIGH** |
| Path resolution | Upfront for all records | Lazy on-demand | **HIGH** |
| String storage | Individual `String` per name? | Contiguous string pool | Medium |
| Parsing | Creates intermediate objects | Direct to final structure | Medium |
| I/O pattern | Interleaved read/parse | Read all, then parse | Low (now) |

### Recommended Rust Changes (Priority Order)

1. **Replace HashMap with Vec indexed by FRS number**
   - Pre-allocate `Vec<Option<ParsedRecord>>` with capacity = max FRS
   - Insert with `vec[frs] = Some(record)` — O(1), no hashing
   - Expected improvement: **20-30%**

2. **Defer path resolution to search time**
   - Don't call `add_missing_parent_placeholders` during indexing
   - Store only `parent_frs`, resolve paths lazily
   - Expected improvement: **15%**

3. **Use FxHash or AHash if you must use HashMap**
   - SipHash is overkill for FRS numbers (not security-sensitive)
   - FxHash is 5-10x faster for integer keys
   - Expected improvement: **5-10%**

4. **String pool for file names**
   - Single `String` buffer, store offset/length per entry
   - Reduces allocator pressure significantly
   - Expected improvement: **5-10%**

5. **Read entire MFT then parse (optional)**
   - Eliminates context switching between I/O and CPU
   - May help cache efficiency
   - Expected improvement: **5%**

### Quick Win: FRS-Indexed Vec

Here's the key insight: MFT FRS numbers are sequential integers from 0 to N. You don't need a HashMap — just use the FRS as a direct array index.

```rust
// Instead of:
let mut records: HashMap<u64, ParsedRecord> = HashMap::new();
records.insert(frs, record);  // Hashes frs, finds bucket, inserts

// Use:
let mut records: Vec<Option<ParsedRecord>> = vec![None; max_frs + 1];
records[frs as usize] = Some(record);  // Direct index, O(1)
```

For 11.7M records, this eliminates 11.7M hash computations and all HashMap overhead.

---

*End of C++ Team Response*

---

## 17. Action Items Summary

### For Rust Team (Priority Order)

- [x] Replace `HashMap<u64, ParsedRecord>` with `Vec<Option<ParsedRecord>>` indexed by FRS ✅ **DONE 2026-01-23**
- [x] Defer path resolution — make `add_missing_parent_placeholders` optional ✅ **DONE 2026-01-23**
- [ ] Implement lazy path resolution with LRU cache for display
- [x] Switch to FxHash/AHash if HashMap is still needed elsewhere ✅ **DONE 2026-01-23**
- [x] Implement string pool for file names ✅ **Already exists in MftIndex**
- [ ] Benchmark after each change to measure impact

### Implementation Details (2026-01-23)

#### 1. Vec-based FRS Indexing
Changed `MftRecordMerger` from `HashMap<u64, ParsedRecord>` to `Vec<Option<ParsedRecord>>`:
- Direct indexing by FRS number: `self.base_records[frs] = Some(record)`
- Eliminates 11.7M SipHash computations
- O(1) insert with no hashing overhead
- File: `crates/uffs-mft/src/io.rs`, lines 1536-1751

#### 2. FxHash for Remaining HashSets
Switched `add_missing_parent_placeholders_to_vec()` and `ParsedColumns::add_missing_parent_placeholders()` from `std::collections::HashSet` to `rustc_hash::FxHashSet`:
- 5-10x faster than SipHash for integer keys
- File: `crates/uffs-mft/src/io.rs`

#### 3. Optional Placeholder Creation
Added `--no-placeholders` flag to benchmark command:
- `MftReader::with_add_placeholders(false)` skips placeholder creation
- Saves ~15% of CPU time during indexing
- File: `crates/uffs-mft/src/reader.rs`

### Benchmark Command

```powershell
# With all optimizations (no placeholders)
.\target\profiling\uffs_mft.exe benchmark-index-lean -d S --no-placeholders

# With placeholders (default behavior)
.\target\profiling\uffs_mft.exe benchmark-index-lean -d S
```

### Expected Results

If all optimizations are implemented:
- Current: 76s
- Target: ~40s (matching C++)
- Stretch goal: <40s (Rust should be able to match or beat C++)

### Next Steps

1. ~~Rust team implements Vec-based storage (biggest win)~~ ✅ DONE
2. ~~Rust team defers path resolution~~ ✅ DONE (optional via flag)
3. Re-benchmark and report in Section 18

---

## 18. Benchmark Results After Optimization

*Pending: Run benchmark on Windows with profiling profile and report results here.*

```powershell
# Build with profiling profile
cargo build --profile profiling --package uffs-mft

# Run benchmark with all optimizations
.\target\profiling\uffs_mft.exe benchmark-index-lean -d S --no-placeholders

# Run benchmark with default settings (for comparison)
.\target\profiling\uffs_mft.exe benchmark-index-lean -d S
```

---

*Document will be updated with benchmark results after testing.*

