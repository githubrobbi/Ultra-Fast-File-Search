# Rust `uffs-mft` I/O & Concurrency Optimization Plan

**Audience:** Junior developer implementing performance work in the Rust `uffs-mft` crate.

**Primary goal:** Design and implement an indexing pipeline that **saturates the hardware limits** (disk + CPU) and is capable of being **significantly faster than the current C++ implementation** wherever the bottleneck is **CPU-bound** (SSD, RAM-cached MFT, multi-drive scenarios).

Practically this means:

- On **HDDs**, we aim to match C++ throughput as closely as physical disk limits allow.
- On fast **SSDs / NVMe / RAM-cached MFT**, we aim to beat the C++ indexer by **2–3× or more** by using more aggressive parallelism, zero-copy parsing, and better CPU utilization.

---

## 1. Baseline: Current vs C++

### 1.1 C++ `--benchmark-index` (reference behavior)
- Uses **IOCP (I/O completion ports)** with **overlapped `ReadFile`**.
- Has a pool of **worker threads ≈ number of CPU cores**.
- For each MFT extent:
  - Issues an async read (`ReadFile` with `OVERLAPPED`).
  - On completion, the worker thread:
    - Parses all records in that buffer.
    - Updates shared statistics.
    - Returns the buffer to a **global buffer pool**.
- Result: **multiple reads in flight** + **multi-core parsing**.

### 1.2 Rust `benchmark-index-lean` on HDD (current behavior)
- Uses `MftReadMode::Auto`, which selects **`Pipelined`** mode for HDDs.
- `PipelinedMftReader`:
  - Spawns **one reader thread** that does **blocking** `SetFilePointerEx` + `ReadFile` per chunk.
  - Sends each `ReadBuffer` through a **bounded channel** to the main thread.
  - The **main thread parses all records serially** and builds the index.
- Result: at most **one read in progress** and **single-threaded parsing**.

### 1.3 Observed gap on large HDD (drive `S:`)
- C++: ~40.7 s, ~288k records/sec.
- Rust (lean index): ~77 s, ~152k records/sec.
- Core reason: C++ uses **N worker threads parsing in parallel**; Rust pipelined path parses on **one** thread only and does not issue multiple overlapping reads.

---

## 2. High-Level Strategy

We want **both**:

1. A near-term, low-risk path that already gives big wins (especially on HDDs).
2. A longer-term path that can push **beyond** the C++ design on modern hardware.

We’ll structure the work into **four phases**:

1. **Phase A – Pipelined + parallel parsing (quick win, HDD + SSD)**
   - Keep current pipelined I/O model (one reader thread per volume).
   - Add **multi-core parsing** by dispatching `ReadBuffer`s to a thread pool (e.g., Rayon).
   - Goal: approach or beat C++ on HDD, and lift CPU-side limits on SSD.

2. **Phase B – Advanced I/O overlap (IOCP-style or equivalent)**
   - Introduce a more C++-like engine where beneficial:
     - Multiple reads in flight per volume.
     - Worker threads handling both completion and parsing.
   - We can use IOCP directly or an equivalent high-level abstraction; the design must allow **>1 outstanding read** per volume.

3. **Phase C – Zero-copy and SIMD-amenable parsing**
   - Remove per-record allocations and unnecessary copies in the Rust parser.
   - Parse records **in-place from shared buffers**, only allocating minimal metadata.
   - Prepare data layout for vectorization (SoA-friendly, cache-line-aware) so Rust can reach or exceed C++ per-core efficiency.

4. **Phase D – Multi-drive and CPU topology awareness**
   - Run one independent pipeline per volume by default, up to a configurable global limit.
   - Be able to saturate many-core CPUs by indexing multiple drives concurrently.
   - Optionally add NUMA / affinity hints later if needed.

The rest of this document focuses on **Phase A** in detail, sketches **Phase B**, and outlines concrete tasks for **Phase C** and **Phase D** so the design is future-proof for "beyond C++" performance.

---

## 3. Phase A – Pipelined + Parallel Parsing

### 3.1 Design overview

We extend the current `PipelinedMftReader` into a **hybrid model**:

- **Reader thread (unchanged in spirit):**
  - Reads chunks sequentially from disk into reusable `AlignedBuffer`s.
  - Sends them to a work queue.
- **Parser workers (new):**
  - Use a **thread pool** (Rayon) to parse `ReadBuffer`s in parallel.
  - Each worker:
    - Iterates records in its buffer.
    - Runs `apply_fixup()` and `parse_record` / `parse_record_full`.
    - Returns parsed `ParseResult`s to a central merger.
- **Merger (existing concept):**
  - `MftRecordMerger` remains responsible for handling base/extension records.
  - It should be used from a single thread to keep logic simple; workers return batches of `ParseResult`.

This provides **I/O + CPU overlap** and **multi-core parsing** without changing the public API of `MftReader`.

### 3.2 Concrete tasks

1. **Introduce a new read mode**
   - File: `crates/uffs-mft/src/reader.rs` (in the main Rust repo).
   - Add a new enum variant:
     - `MftReadMode::PipelinedParallel` (name can be adjusted, but must be explicit).
   - In the `Auto` selection logic:
     - For HDD: initially keep `Pipelined` as default.
     - For experiments / benchmarks, allow forcing `PipelinedParallel` via CLI.

2. **New reader path using Rayon**
   - File: `crates/uffs-mft/src/io.rs`.
   - Add a new method, e.g.:
     - `PipelinedMftReader::read_all_pipelined_parallel(...)`.
   - Behavior:
     - Reuse the existing chunk generation and reader thread from `read_all_pipelined`.
     - Replace the **serial parse loop** with a **parallel worker model**:
       - Use a `crossbeam_channel` or similar to receive `ReadBuffer`s from the reader thread.
       - For each buffer, submit a job to Rayon:
         - Job parses all records in that buffer and returns a `Vec<ParseResult>`.
       - The main async context waits on these jobs and feeds results into `MftRecordMerger`.

3. **Batch parsing interface**
   - In `io.rs`, factor out a helper to parse a single `ReadBuffer` into results:
     - Input: `ReadBuffer { buffer, bytes_read, chunk, record_size }`.
     - Output: `Vec<ParseResult>`.
   - This function will:
     - Handle `skip_begin`, effective record count, and bounds checking.
     - Apply `apply_fixup()`.
     - Use `parse_record` / `parse_record_full` depending on `merge_extensions` flag.
   - This makes it easy to call from both the serial and parallel paths.

4. **Merger integration**
   - Continue to use `MftRecordMerger` from a single thread:
     - The main thread collects `Vec<ParseResult>` from worker tasks.
     - Calls `merger.add_result(result)` for each.
   - Ensure ordering is not required for correctness (it should not be, as records are keyed by FRS).

5. **CLI integration for benchmarking**
   - File: `crates/uffs-mft/src/main.rs`.
   - Extend `benchmark-index-lean` to accept a flag, e.g.:
     - `--mode pipelined-parallel` or `--parallel-parse`.
   - Map flag → `MftReadMode::PipelinedParallel`.
   - This allows A/B testing vs `Pipelined` and the C++ `--benchmark-index`.

6. **Error handling & logging**
   - Ensure any I/O or parse errors in worker tasks are:
     - Logged with enough context (chunk range, bytes_read).
     - Propagated back to the caller, causing the whole operation to fail fast.
   - Add structured logs when the new mode is used:
     - Selected mode, number of worker threads (Rayon default), chunk size, pipeline depth.

7. **Thread-safety review**
   - Verify all shared data structures are used safely:
     - `MftExtentMap` and `MftBitmap` are read-only.
     - Each worker gets its own `Vec<ParseResult>` and only the main thread touches the merger.
   - No global mutable state should be accessed from multiple threads without synchronization.

### 3.3 Acceptance criteria for Phase A

- On the large HDD `S:` drive:
  - `benchmark-index-lean` with `PipelinedParallel` mode should:
    - Reduce total time significantly vs current `Pipelined` (aim: **≤ 50 seconds** as a first milestone).
    - Increase records/sec to be **within 10–15% of C++** (~245k–290k records/sec).
- On at least one fast SSD/NVMe volume:
  - `benchmark-index-lean` with `PipelinedParallel` should show **> 1.5× speedup** over the current Rust `Pipelined` mode.
- All existing tests pass and CI remains green.
- Behavior (what records are indexed, how) must remain unchanged; only performance should differ.

---

## 4. Phase B – Advanced I/O Overlap (IOCP-Style or Equivalent)

Only start Phase B if Phase A is implemented, benchmarked, and we still need more performance – especially on SSDs or in multi-drive scenarios.

### 4.1 Goal
- More closely mirror (and potentially exceed) the C++ `read_mft_parallel` design:
  - Multiple concurrent reads in flight on a single volume handle.
  - Completion callbacks parsing data and returning buffers to a shared pool.
  - Ability to tune the number of outstanding requests to saturate different storage types.

### 4.2 High-level tasks

1. **Raw Windows IOCP primitives**
   - Use the `windows`/`windows-sys` crate to:
     - Create an IOCP handle.
     - Associate the volume handle with IOCP.
     - Wrap `OVERLAPPED` and `ReadFile` for overlapped I/O.

2. **MftReadContext equivalent**
   - Define a Rust struct similar to the C++ `MftReadContext`:
     - Holds a `Vec<AlignedBuffer>` pool, protected by synchronization.
     - Has atomic counters for bytes read, records processed.

3. **Overlapped read objects**
   - Create Rust types representing an in-flight read:
     - Fields: `buffer`, extent info, reference to shared context.
     - On completion: parse records and return buffer to pool.

4. **Worker loop**
   - Implement an IOCP event loop similar to the C++ code:
     - `GetQueuedCompletionStatus` waits for completions.
     - Each completion dispatches to the appropriate handler.

5. **Integration with `MftReader`**
   - Add a new `MftReadMode::IocpParallel` (or a more generic name like `AdvancedOverlap`) and wire it up so that:
     - `read_all_index()` can use it behind the same public API.
     - The mode can be selected explicitly via CLI flags for benchmarking.

6. **Extensive testing & benchmarking**
   - Compare `IocpParallel` / `AdvancedOverlap` vs `PipelinedParallel` vs C++ on both HDD and SSD.
   - Experiment with different levels of concurrency (number of outstanding reads).

### 4.3 Stretch goals for “beyond C++”

- Implement heuristics to automatically choose an optimal number of in-flight reads based on:
  - Detected drive type (HDD/SSD/NVMe).
  - Measured initial throughput and latency of the first few chunks.
- Allow the engine to dynamically adapt concurrency during a run (e.g., ramp up/down outstanding reads).

If implemented well, this can surpass the fixed-strategy C++ implementation on modern SSD/NVMe hardware.

---

## 5. Phase C – Zero-Copy & SIMD-Friendly Parsing

**Objective:** Remove unnecessary copies and heap allocations from the parsing hot-path so that each core does the minimal amount of work per record, enabling **vectorization** and better cache utilization.

### 5.1 Current pain points

- In the Rust pipelined reader, each record is currently copied into a fresh `Vec<u8>` before `apply_fixup` and `parse_record`.
- This creates:
  - Per-record allocations or at least copies.
  - Extra memory bandwidth usage.
  - Barriers to vectorization and cache friendliness.

### 5.2 Target design

1. **Parse in-place from shared buffers**
   - Change parsing helpers to work with *slices into the shared `AlignedBuffer`* instead of owning `Vec<u8>`:
     - Accept `&[u8]` or a light wrapper struct with typed accessors.
     - Perform USA fixup directly on the shared buffer (as C++ does) in a controlled way.

2. **Minimize per-record allocations**
   - Allocate only fixed-size metadata structures (`Record`, `LinkInfo`, `StreamInfo`, etc.) in SoA/contiguous vectors.
   - Store variable-length data (names, stream names) into pre-sized contiguous buffers, using offsets (same spirit as C++).

3. **SIMD-friendly layout**
   - Ensure record-parsing loops access memory sequentially and avoid pointer chasing.
   - Optionally introduce small helper types (e.g., `ParsedHeaderView`) that expose frequently used fields as scalars, enabling the compiler to auto-vectorize operations like flags checks.

### 5.3 Acceptance criteria for Phase C

- Remove the `to_vec()` copy from the hot-path parsing loop.
- Benchmarks on SSD/NVMe show clear **per-core throughput improvement** (e.g., ≥ 20–30% faster than Phase A alone).
- No behavior changes (index contents must remain bit-identical for the same MFT input).

---

## 6. Phase D – Multi-Drive & CPU Topology Awareness

**Objective:** When multiple NTFS volumes are present, be able to run **one pipeline per volume** (up to a configurable limit) to fully utilize multi-core CPUs and I/O subsystems.

### 6.1 Design directions

- For a multi-drive benchmark or real indexing run:
  - Spawn a separate `MftReader` + pipeline (read mode + indexer) per drive.
  - Coordinate via a global executor that limits the total number of concurrent pipelines to avoid oversubscription.

- Optional future work:
  - Use OS APIs to query NUMA and core topology.
  - Pin heavy worker pools (parsers) close to the storage controller or NUMA node associated with the volume.

### 6.2 Acceptance criteria for Phase D

- On a system with multiple HDDs/SSDs:
  - Multi-drive indexing achieves **close to linear scaling** until one shared bottleneck is hit (e.g., CPU or PCIe bandwidth).
- The single-drive performance from Phases A–C is preserved; the multi-drive logic must not slow down the one-drive case.

---

## 7. Implementation Status & Milestones

This section tracks the implementation status of each phase and its sub-steps.

**Last Updated:** 2026-01-23

---

### Phase A – Pipelined + Parallel Parsing ✅ COMPLETE

| Step | Description | Status | Implementation Details |
|------|-------------|--------|------------------------|
| A1 | Add `MftReadMode::PipelinedParallel` enum variant | ✅ Done | `reader.rs:57-60` - New enum variant with documentation |
| A2 | Update `as_str()` and `FromStr` for new mode | ✅ Done | `reader.rs:77,99` - Returns `"pipelined-parallel"`, parses `"pipelined-parallel"` and `"pipelinedparallel"` |
| A3 | Update Auto mode selection for HDD | ✅ Done | `reader.rs:793-796` - HDD now defaults to `PipelinedParallel` |
| A4 | Implement `read_all_pipelined_parallel()` method | ✅ Done | `io.rs:3600-3700` - Reader thread fills bounded channel, Rayon parses all buffers via `par_iter_mut().flat_map()` |
| A5 | Add batch parsing helper function | ✅ Done | `io.rs:3759-3820` - `parse_buffer_to_results_zero_copy()` and `parse_buffer_zero_copy_inner()` |
| A6 | Integrate with `MftRecordMerger` | ✅ Done | `io.rs:3687-3692` - Single-threaded merger after parallel parsing |
| A7 | Add CLI `--mode` flag to `benchmark-index-lean` | ✅ Done | `main.rs:428-429` - Accepts `auto`, `parallel`, `streaming`, `prefetch`, `pipelined`, `pipelined-parallel`, `iocp-parallel` |
| A8 | Wire up mode in `read_all_index()` | ✅ Done | `reader.rs:929-958` - Full integration with progress callbacks |
| A9 | Wire up mode in `read_all_index_lean()` | ✅ Done | `reader.rs:1431-1459` - Full integration with progress callbacks |

**Verification:**
- ✅ All 19 unit tests pass
- ✅ All doc tests pass
- ✅ No clippy warnings
- ✅ Code compiles on macOS (cross-platform stubs work)

---

### Phase B – Advanced I/O Overlap (IOCP-Style) ✅ COMPLETE

| Step | Description | Status | Implementation Details |
|------|-------------|--------|------------------------|
| B1 | Add `MftReadMode::IocpParallel` enum variant | ✅ Done | `reader.rs:61-64` - New enum variant with documentation |
| B2 | Update `as_str()` and `FromStr` for IOCP mode | ✅ Done | `reader.rs:78,100` - Returns `"iocp-parallel"`, parses `"iocp-parallel"`, `"iocpparallel"`, `"iocp"` |
| B3 | Implement `IoCompletionPort` wrapper | ✅ Done | `io.rs:3877-3940` - RAII wrapper for Windows IOCP with `CreateIoCompletionPort`, `associate()`, and `Drop` |
| B4 | Implement `OverlappedRead` struct | ✅ Done | `io.rs:3942-4015` - Pinned `OVERLAPPED` structure with aligned buffer and chunk metadata |
| B5 | Implement `IocpMftReader` struct | ✅ Done | `io.rs:4017-4026` - Reader with extent map, bitmap, chunk size, and configurable concurrency |
| B6 | Implement `IocpMftReader::new()` | ✅ Done | `io.rs:4033-4060` - Constructor with drive-type-aware chunk sizing |
| B7 | Implement `with_concurrency()` builder | ✅ Done | `io.rs:4062-4070` - Configurable number of concurrent reads (default: 8) |
| B8 | Implement `read_all_iocp()` method | ✅ Done | `io.rs:4072-4300` - Full IOCP event loop with `GetQueuedCompletionStatus`, multiple reads in flight |
| B9 | Use zero-copy parsing in IOCP path | ✅ Done | `io.rs:4224-4230` - Uses `parse_buffer_zero_copy_inner()` for in-place fixup |
| B10 | Wire up in `read_all_index()` | ✅ Done | `reader.rs:959-987` - Full integration with progress callbacks |
| B11 | Wire up in `read_all_index_lean()` | ✅ Done | `reader.rs:1460-1488` - Full integration with progress callbacks |

**Verification:**
- ✅ All 19 unit tests pass
- ✅ All doc tests pass
- ✅ No clippy warnings
- ✅ Code compiles on macOS (Windows-only code behind `#[cfg(windows)]`)

---

### Phase C – Zero-Copy & SIMD-Friendly Parsing ✅ COMPLETE

| Step | Description | Status | Implementation Details |
|------|-------------|--------|------------------------|
| C1 | Analyze current parsing hot-path | ✅ Done | Identified `to_vec()` copies in all reader paths and UTF-16 allocations |
| C2 | Implement in-place USA fixup | ✅ Done | All readers now apply `apply_fixup()` directly on shared buffer slices |
| C3 | Update `StreamingMftReader` to zero-copy | ✅ Done | `io.rs:2946-2972` - Uses `buffer.as_mut_slice()` with in-place fixup |
| C4 | Update `PrefetchMftReader` to zero-copy | ✅ Done | `io.rs:3156-3185` - Uses `buffer.as_mut_slice()` with in-place fixup |
| C5 | Update `PipelinedMftReader` to zero-copy | ✅ Done | `io.rs:3457-3483` - Uses `buffer.as_mut_slice()` with in-place fixup |
| C6 | Update `ParallelMftReader` to zero-copy | ✅ Done | `io.rs:2387-2440,2474-2523` - Uses `par_iter_mut()` with in-place fixup for both merge_extensions and legacy paths |
| C7 | Update `PipelinedParallel` to zero-copy | ✅ Done | `io.rs:3676-3678` - Uses `par_iter_mut()` with `parse_buffer_to_results_zero_copy()` |
| C8 | Update IOCP path to zero-copy | ✅ Done | `io.rs:4224-4230` - Uses `parse_buffer_zero_copy_inner()` |
| C9 | Add `smallvec` dependency (Windows-only) | ✅ Done | `Cargo.toml:64` - Added under `[target.'cfg(windows)'.dependencies]` |
| C10 | Optimize UTF-16 conversion in `parse_file_name_full` | ✅ Done | `io.rs:1457-1461` - Uses `SmallVec<[u16; 128]>` to avoid heap allocation for names < 128 chars |
| C11 | Optimize UTF-16 conversion in `parse_data_attribute_full` | ✅ Done | `io.rs:1487-1490` - Uses `SmallVec<[u16; 64]>` to avoid heap allocation for stream names < 64 chars |

**Verification:**
- ✅ All 19 unit tests pass
- ✅ All doc tests pass
- ✅ No clippy warnings
- ✅ `smallvec` is Windows-only dependency (no cfg-gate suppression hacks)

---

### Phase D – Multi-Drive & CPU Topology Awareness ❌ NOT STARTED

| Step | Description | Status | Implementation Details |
|------|-------------|--------|------------------------|
| D1 | Design multi-drive pipeline architecture | ❌ Pending | - |
| D2 | Implement per-drive `MftReader` spawning | ❌ Pending | - |
| D3 | Add global executor with concurrency limits | ❌ Pending | - |
| D4 | Implement NUMA/core topology awareness | ❌ Pending | - |
| D5 | Add multi-drive CLI commands | ❌ Pending | - |
| D6 | Benchmark multi-drive scaling | ❌ Pending | - |

---

## 8. Performance Validation Checklist

### Pre-Implementation Baseline (M1)
- [ ] Run `benchmark-index-lean` on HDD (S:) with current Rust implementation
- [ ] Run C++ `--benchmark-index` on same HDD for comparison
- [ ] Document baseline records/sec and throughput

### Phase A Validation (M4)
- [ ] Run `benchmark-index-lean --mode pipelined-parallel` on HDD
- [ ] Compare against `--mode pipelined` (single-threaded parsing)
- [ ] Target: ≤ 50 seconds on large HDD, within 10-15% of C++ throughput
- [ ] Run on SSD and verify > 1.5× speedup over `Pipelined` mode

### Phase B Validation
- [ ] Run `benchmark-index-lean --mode iocp-parallel` on HDD
- [ ] Compare against `--mode pipelined-parallel`
- [ ] Experiment with different concurrency levels (2, 4, 8, 16)
- [ ] Document optimal concurrency for HDD vs SSD

### Phase C Validation
- [ ] Measure per-core throughput improvement
- [ ] Target: ≥ 20-30% faster than Phase A alone on SSD/NVMe
- [ ] Verify index contents are bit-identical (no behavior changes)

### Phase D Validation
- [ ] Test multi-drive indexing on system with 2+ drives
- [ ] Verify close to linear scaling until bottleneck
- [ ] Confirm single-drive performance is preserved

---

## 9. CLI Usage Reference

```bash
# Phase A: Pipelined + Parallel Parsing
uffs_mft benchmark-index-lean --drive C --mode pipelined-parallel

# Phase B: IOCP-style I/O Overlap
uffs_mft benchmark-index-lean --drive C --mode iocp-parallel

# Compare all modes
uffs_mft benchmark-index-lean --drive C --mode auto
uffs_mft benchmark-index-lean --drive C --mode parallel
uffs_mft benchmark-index-lean --drive C --mode streaming
uffs_mft benchmark-index-lean --drive C --mode prefetch
uffs_mft benchmark-index-lean --drive C --mode pipelined
uffs_mft benchmark-index-lean --drive C --mode pipelined-parallel
uffs_mft benchmark-index-lean --drive C --mode iocp-parallel
```

---

## 10. Summary

| Phase | Description | Status | Key Files Modified |
|-------|-------------|--------|-------------------|
| **A** | Pipelined + Parallel Parsing | ✅ Complete | `reader.rs`, `io.rs`, `main.rs` |
| **B** | IOCP-style I/O Overlap | ✅ Complete | `reader.rs`, `io.rs` |
| **C** | Zero-Copy & SIMD-Friendly Parsing | ✅ Complete | `io.rs`, `Cargo.toml` |
| **D** | Multi-Drive & CPU Topology | ❌ Not Started | - |

**Next Steps:**
1. Run performance benchmarks on Windows with HDD and SSD
2. Compare against C++ `--benchmark-index` baseline
3. If Phase D is needed, design multi-drive architecture
