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

## 7. Milestones & Tracking

Use these milestones to manage the work.

1. **M1 – Baseline & Instrumentation**
   - [ ] Re-run `benchmark-index-lean` and C++ `--benchmark-index` on S:.
   - [ ] Capture logs, records/sec, and throughput numbers.
   - [ ] Document current results in a short markdown file.

2. **M2 – Implement `PipelinedParallel` mode**
   - [ ] Add `MftReadMode::PipelinedParallel` and wire it into `MftReader`.
   - [ ] Implement `read_all_pipelined_parallel` using Rayon workers.
   - [ ] Add CLI flag to select the new mode.

3. **M3 – Functional Verification**
   - [ ] Run unit tests and CI; fix any regressions.
   - [ ] Add targeted tests for the new mode (e.g., small synthetic MFT source).

4. **M4 – Performance Evaluation**
   - [ ] Re-run benchmarks on S: and at least one SSD.
   - [ ] Compare against C++ `--benchmark-index`.
   - [ ] Decide if Phase B (IOCP-style engine) is necessary.

5. **M5 – (Optional) IOCP Engine**
   - [ ] Design and spike a minimal IOCP-based reader in Rust.
   - [ ] Integrate as `MftReadMode::IocpParallel`.
   - [ ] Benchmark and compare with existing modes.

Keep this document updated as milestones are completed so that progress is visible and the next steps are always clear.
