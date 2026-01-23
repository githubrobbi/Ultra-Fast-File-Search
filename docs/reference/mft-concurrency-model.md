# MFT Reading Concurrency Model

This document explains how Ultra-Fast-File-Search achieves extreme MFT reading performance through sophisticated async I/O, buffer pooling, and concurrent parsing.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        MFT Reading Pipeline                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │  Phase 1: Get Retrieval Pointers (VCN → LCN mapping)                 │   │
│  │                                                                       │   │
│  │  FSCTL_GET_RETRIEVAL_POINTERS on:                                    │   │
│  │    • X:\$MFT::$BITMAP  (which records are in-use)                    │   │
│  │    • X:\$MFT::$DATA    (the actual MFT data)                         │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                              │                                               │
│                              ▼                                               │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │  Phase 2: Split Extents into Read Blocks (~1MB each)                 │   │
│  │                                                                       │   │
│  │  Large extents are chunked to:                                       │   │
│  │    • Enable parallel I/O                                             │   │
│  │    • Limit memory per operation                                      │   │
│  │    • Allow progress tracking                                         │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                              │                                               │
│                              ▼                                               │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │  Phase 3: Concurrent Async I/O via IOCP                              │   │
│  │                                                                       │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐                  │   │
│  │  │ Read 1  │  │ Read 2  │  │ Read 3  │  │ Read N  │  (in-flight)    │   │
│  │  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘                  │   │
│  │       │            │            │            │                        │   │
│  │       └────────────┴─────┬──────┴────────────┘                        │   │
│  │                          ▼                                            │   │
│  │              ┌─────────────────────┐                                  │   │
│  │              │  I/O Completion Port │                                 │   │
│  │              │  (Kernel-managed)    │                                 │   │
│  │              └──────────┬──────────┘                                  │   │
│  │                         │                                             │   │
│  │       ┌─────────────────┼─────────────────┐                           │   │
│  │       ▼                 ▼                 ▼                           │   │
│  │  ┌─────────┐       ┌─────────┐       ┌─────────┐                      │   │
│  │  │ Worker  │       │ Worker  │       │ Worker  │  (N = CPU cores)    │   │
│  │  │ Thread  │       │ Thread  │       │ Thread  │                      │   │
│  │  └────┬────┘       └────┬────┘       └────┬────┘                      │   │
│  │       │                 │                 │                           │   │
│  │       └─────────────────┴─────────────────┘                           │   │
│  │                         │                                             │   │
│  │                         ▼                                             │   │
│  │              ┌─────────────────────┐                                  │   │
│  │              │  Parse MFT Records  │                                  │   │
│  │              │  Build Index        │                                  │   │
│  │              └─────────────────────┘                                  │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Key Data Structures

### RetPtr - Retrieval Pointer Entry

```cpp
struct RetPtr {
    unsigned long long vcn;           // Virtual Cluster Number (offset in file)
    unsigned long long cluster_count; // Number of clusters in this extent
    long long lcn;                    // Logical Cluster Number (disk offset)
    
    // Bitmap-driven skip optimization
    atomic<unsigned long long> skip_begin;  // Clusters to skip at start (unused records)
    atomic<unsigned long long> skip_end;    // Clusters to skip at end (unused records)
};
```

### OverlappedNtfsMftReadPayload - Main Controller

```cpp
class OverlappedNtfsMftReadPayload : public Overlapped {
    IoCompletionPort volatile* iocp;      // The completion port
    Handle closing_event;                  // Shutdown signal
    
    RetPtrs bitmap_ret_ptrs;              // Extents for $MFT::$BITMAP
    RetPtrs data_ret_ptrs;                // Extents for $MFT::$DATA
    
    unsigned int cluster_size;            // Bytes per cluster
    unsigned long long read_block_size;   // Target read size (~1MB)
    
    // Atomic progress counters
    atomic<size_t> jbitmap;               // Next bitmap chunk to read
    atomic<size_t> nbitmap_chunks_left;   // Bitmap chunks remaining
    atomic<size_t> jdata;                 // Next data chunk to read
    atomic<unsigned int> valid_records;   // Records found in-use
    
    Bitmap mft_bitmap;                    // Which records are in-use
    intrusive_ptr<NtfsIndex volatile> p;  // The index being built
};
```

### ReadOperation - Per-Read State

```cpp
class ReadOperation : public Overlapped {
    unsigned long long _voffset;          // Virtual offset in MFT
    unsigned long long _skipped_begin;    // Bytes skipped at start
    unsigned long long _skipped_end;      // Bytes skipped at end
    clock_t _time;                        // Start time (for speed calc)
    bool _is_bitmap;                      // Reading bitmap or data?
    intrusive_ptr<OverlappedNtfsMftReadPayload volatile> q;  // Parent
    
    // Buffer is allocated inline after this struct!
    // Access via: (this + 1)
};
```

---

## The Reading Pipeline

### Step 1: Get Volume Info and Retrieval Pointers

```cpp
int OverlappedNtfsMftReadPayload::operator()(size_t, uintptr_t key) {
    // Get NTFS volume data
    NTFS_VOLUME_DATA_BUFFER info;
    DeviceIoControl(volume, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &info, sizeof(info), &br, NULL);
    
    this->cluster_size = info.BytesPerCluster;
    p->mft_record_size = info.BytesPerFileRecordSegment;
    p->mft_capacity = info.MftValidDataLength.QuadPart / info.BytesPerFileRecordSegment;
    
    // Associate volume with IOCP for async I/O
    this->iocp->associate(volume, reinterpret_cast<uintptr_t>(&*p));
    
    // Get $MFT::$BITMAP extents
    RP ret_ptrs = get_retrieval_pointers("X:\\$MFT::$BITMAP", &llsize, ...);
    // Split into read_block_size chunks → bitmap_ret_ptrs
    
    // Get $MFT::$DATA extents
    RP ret_ptrs = get_retrieval_pointers("X:\\$MFT::$DATA", &llsize, ...);
    // Split into read_block_size chunks → data_ret_ptrs

    // Start concurrent reads (concurrency = 2 initially)
    for (int concurrency = 0; concurrency < 2; ++concurrency) {
        this->queue_next();
    }
}
```

### Step 2: Extent Chunking

Large MFT extents are split into ~1MB read blocks:

```cpp
unsigned long long read_block_size = 1 << 20;  // 1MB

for (auto& extent : ret_ptrs) {
    long long clusters_left = extent.first - prev_vcn;

    for (long long m = 0; m < clusters_left; m += n) {
        // Chunk size: min(remaining, read_block_size / cluster_size)
        n = std::min(extent.first - prev_vcn,
                     1 + (read_block_size - 1) / cluster_size);

        data_ret_ptrs.push_back(RetPtr(prev_vcn, n, extent.second + m));
        prev_vcn += n;
    }
}
```

**Why 1MB?**
- Large enough for efficient disk I/O
- Small enough to limit memory per operation
- Enables fine-grained progress tracking
- Allows multiple in-flight operations

### Step 3: Queue Next Read (Self-Perpetuating)

```cpp
void OverlappedNtfsMftReadPayload::queue_next() volatile {
    // Try bitmap first (needed to optimize data reads)
    size_t jbitmap = this->jbitmap.fetch_add(1, memory_order_acq_rel);

    if (jbitmap < bitmap_ret_ptrs.size()) {
        // Issue bitmap read
        RetPtr& j = bitmap_ret_ptrs[jbitmap];
        unsigned long long skip_begin = j.skip_begin.load(memory_order_acquire);
        unsigned long long skip_end = j.skip_end.load(memory_order_acquire);

        unsigned int cb = (j.cluster_count - skip_begin - skip_end) * cluster_size;

        // Allocate ReadOperation with inline buffer
        intrusive_ptr<ReadOperation> p(new(cb) ReadOperation(this, true));
        p->offset((j.lcn + skip_begin) * cluster_size);  // Disk offset
        p->voffset((j.vcn + skip_begin) * cluster_size); // Virtual offset
        p->skipped_begin(skip_begin * cluster_size);
        p->skipped_end(skip_end * cluster_size);

        iocp->read_file(volume, p.get() + 1, cb, p);  // Buffer is after struct
        return;
    }

    // Then data reads
    size_t jdata = this->jdata.fetch_add(1, memory_order_acq_rel);

    if (jdata < data_ret_ptrs.size()) {
        // Issue data read (same pattern as bitmap)
        // ...
    }
}
```

**Key Insight**: Each completion handler calls `queue_next()`, maintaining a constant number of in-flight operations.

### Step 4: Completion Handler

```cpp
int ReadOperation::operator()(size_t size, uintptr_t key) {
    if (!q->p->cancelled()) {
        // Queue next read FIRST (keeps pipeline full)
        this->q->queue_next();

        void* buffer = this + 1;  // Buffer is inline after struct

        if (this->_is_bitmap) {
            // Process bitmap: count valid records, update skip hints
            process_bitmap(buffer, size);
        } else {
            // Process MFT data: parse records, build index
            q->p->preload_concurrent(virtual_offset, buffer, size);
            lock(q->p)->load(virtual_offset, buffer, size,
                             skipped_begin(), skipped_end());
        }

        // Report speed for progress display
        lock(q->p)->report_speed(size, this->time(), clock());
    }

    return -1;  // Don't requeue this operation
}
```

---

## Buffer Pool (Zero-Allocation After Warmup)

### The Problem

Allocating 1MB buffers per read is expensive:
- `malloc`/`free` overhead
- Memory fragmentation
- Cache pollution

### The Solution: Inline Buffer + Recycling

```cpp
class ReadOperation : public Overlapped {
    // ... fields ...

    // Custom allocator with recycling
    static recursive_mutex recycled_mutex;
    static std::vector<std::pair<size_t, void*>> recycled;

    static void* operator new(size_t n, size_t buffer_size) {
        // Try to find a recycled buffer of sufficient size
        {
            unique_lock<recursive_mutex> guard(recycled_mutex);

            size_t best = recycled.size();
            for (size_t i = 0; i < recycled.size(); ++i) {
                if (recycled[i].first >= n + buffer_size) {
                    if (best >= recycled.size() ||
                        recycled[i].first <= recycled[best].first) {
                        best = i;
                    }
                }
            }

            if (best < recycled.size()) {
                void* p = recycled[best].second;
                recycled.erase(recycled.begin() + best);
                return p;
            }
        }

        // No suitable buffer - allocate new
        return ::operator new(n + buffer_size);
    }

    static void operator delete(void* p) {
        // Return to pool instead of freeing
        unique_lock<recursive_mutex> guard(recycled_mutex);
        recycled.push_back({_msize(p), p});
    }
};
```

**Result**: After initial warmup, zero allocations during MFT reading.

---

## Bitmap-Driven Skip Optimization

### The Insight

The MFT bitmap tells us which records are in-use. We can skip reading clusters that contain only unused records.

### Implementation

```cpp
// When bitmap chunk completes:
int ReadOperation::operator()(size_t size, uintptr_t key) {
    if (this->_is_bitmap) {
        // Count valid records using popcount
        static unsigned char popcount[] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};

        unsigned int nrecords = 0;
        for (size_t i = 0; i < size; ++i) {
            unsigned char byte = buffer[i];
            nrecords += popcount[byte & 0xF] + popcount[byte >> 4];
        }

        q->valid_records.fetch_add(nrecords, memory_order_relaxed);

        // Update skip hints for corresponding data chunks
        // If a data chunk has no valid records, skip_begin/skip_end
        // can be set to skip the entire chunk
    }
}
```

---

## Concurrency Control

### Atomic Progress Counters

```cpp
atomic<size_t> jbitmap;  // Next bitmap chunk index
atomic<size_t> jdata;    // Next data chunk index

// In queue_next():
size_t j = this->jdata.fetch_add(1, memory_order_acq_rel);

if (j >= data_ret_ptrs.size()) {
    // Oops, incremented past end - restore
    this->jdata.fetch_sub(1, memory_order_acq_rel);
    return;
}
```

### Memory Ordering

| Operation | Ordering | Reason |
|-----------|----------|--------|
| `jdata.fetch_add` | `acq_rel` | Synchronize with other workers |
| `skip_begin.load` | `acquire` | See bitmap thread's writes |
| `skip_begin.store` | `release` | Publish to data threads |
| `valid_records.fetch_add` | `relaxed` | Just a counter, no sync needed |

### Concurrency Level

```cpp
// Start with 2 concurrent operations
for (int concurrency = 0; concurrency < 2; ++concurrency) {
    this->queue_next();
}
```

**Why 2?**
- Enough to keep disk busy
- Not so many that we exhaust memory
- Each completion queues the next, maintaining level

---

## MFT Record Parsing (load function)

```cpp
void NtfsIndex::load(unsigned long long virtual_offset, void* buffer,
                     size_t size, unsigned long long skipped_begin,
                     unsigned long long skipped_end) {
    unsigned int mft_record_size = this->mft_record_size;

    // Calculate log2 of record size for fast division
    unsigned int mft_record_size_log2 = 0;
    while (mft_record_size >> (mft_record_size_log2 + 1)) {
        ++mft_record_size_log2;
    }

    // Process each record in buffer
    for (size_t i = 0; i + mft_record_size <= size; i += mft_record_size) {
        unsigned int frs = (virtual_offset + i) >> mft_record_size_log2;

        FILE_RECORD_SEGMENT_HEADER* frsh =
            reinterpret_cast<FILE_RECORD_SEGMENT_HEADER*>(&buffer[i]);

        // Check magic number
        if (frsh->MultiSectorHeader.Magic != 'ELIF') continue;

        // Apply USA fixup (correct sector-end bytes)
        if (!frsh->MultiSectorHeader.unfixup(mft_record_size)) {
            frsh->MultiSectorHeader.Magic = 'DAAB';  // Mark as bad
            continue;
        }

        // Check if in-use
        if (!(frsh->Flags & FRH_IN_USE)) continue;

        // Parse attributes: $FILE_NAME, $DATA, $STANDARD_INFORMATION, etc.
        parse_attributes(frs, frsh);
    }

    // Update progress
    this->_records_so_far.fetch_add(size >> mft_record_size_log2);
}
```

---

## Performance Characteristics

### Throughput

| Component | Typical Speed |
|-----------|---------------|
| Raw disk read | 100-500 MB/s (SSD) |
| MFT parsing | 1-2 GB/s (CPU-bound) |
| Index building | 500 MB/s - 1 GB/s |

### Memory Usage

| Buffer | Size | Count |
|--------|------|-------|
| Read buffer | ~1 MB | 2-4 in flight |
| Recycled pool | ~1 MB each | Grows to peak |
| MFT bitmap | MFT size / 8 | 1 |
| Index data | ~50 bytes/file | All files |

### Latency

| Phase | Time (4M files) |
|-------|-----------------|
| Get retrieval pointers | < 10 ms |
| Read MFT | 2-5 seconds |
| Parse + index | Overlapped with read |
| Total | 2-5 seconds |

---

## Summary

UFFS achieves extreme MFT reading speed through:

1. **IOCP-based async I/O** - Kernel-managed thread pool, zero-copy completions
2. **Self-perpetuating pipeline** - Each completion queues the next read
3. **Buffer recycling** - Zero allocations after warmup
4. **Bitmap-driven skipping** - Don't read clusters with no valid records
5. **Inline buffers** - Buffer allocated with ReadOperation struct
6. **Atomic progress tracking** - Lock-free coordination between workers
7. **Overlapped parsing** - Parse while next read is in flight

The result: Reading and indexing millions of files in 2-5 seconds.

---

*Document generated from UFFS source code analysis - January 2026*

