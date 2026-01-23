# MFT Benchmark Tools

Diagnostic tools to measure MFT reading and indexing performance on NTFS volumes.

---

## Overview

UFFS provides two benchmark commands:

| Command | What It Measures |
|---------|------------------|
| `--benchmark-mft` | Raw disk read speed (synchronous I/O) |
| `--benchmark-index` | Full indexing speed (async I/O + parsing + index building) |

**Use `--benchmark-mft`** to measure your disk's raw throughput baseline.

**Use `--benchmark-index`** to measure actual UFFS indexing performance—this is what users experience.

---

## Usage

```bash
# Raw MFT read benchmark (disk I/O only)
uffs --benchmark-mft=<drive_letter>

# Full index build benchmark (the real UFFS pipeline)
uffs --benchmark-index=<drive_letter>
```

**Examples:**
```bash
uffs --benchmark-mft=C
uffs --benchmark-index=C
uffs --benchmark-index=D
```

**Requirements:**
- Must run as **Administrator** (elevated privileges required for raw disk access)
- Target volume must be **NTFS** formatted

---

## What It Measures

| Metric | Description |
|--------|-------------|
| **Total Bytes Read** | Complete MFT data read from disk |
| **Record Count** | Number of MFT records (MFT size ÷ record size) |
| **Time Elapsed** | Wall-clock time from first read to last read |
| **Read Speed** | Throughput in MB/s (bytes read ÷ time) |

---

## Output Format

The tool outputs structured information in several sections:

### 1. Volume Information

```
Volume Information:
  BytesPerSector: 512
  BytesPerCluster: 4096
  BytesPerFileRecordSegment: 1024
  MftValidDataLength: 419430400
  MftStartLcn: 786432
```

| Field | Description |
|-------|-------------|
| `BytesPerSector` | Physical sector size (typically 512 or 4096) |
| `BytesPerCluster` | Cluster size (allocation unit) |
| `BytesPerFileRecordSegment` | MFT record size (typically 1024 bytes) |
| `MftValidDataLength` | Total MFT size in bytes |
| `MftStartLcn` | Logical Cluster Number where MFT begins |

### 2. MFT Information

```
MFT Information:
  Extents: 3
  MFT Size: 419430400 bytes (400 MB)
  Record Size: 1024 bytes
  Record Count: 409600
  Total Bytes to Read: 419430400
```

| Field | Description |
|-------|-------------|
| `Extents` | Number of MFT fragments (1 = contiguous, >1 = fragmented) |
| `MFT Size` | Total MFT size in bytes and megabytes |
| `Record Size` | Size of each MFT record |
| `Record Count` | Total number of MFT records |
| `Total Bytes to Read` | Exact bytes that will be read |

### 3. Benchmark Results

```
=== Benchmark Results ===
Total bytes read: 419430400 (400 MB)
Total records: 409600
Time elapsed: 2345 ms (2.345 seconds)
Read speed: 170.55 MB/s
```

| Field | Description |
|-------|-------------|
| `Total bytes read` | Actual bytes read (should match Total Bytes to Read) |
| `Total records` | Number of MFT records covered |
| `Time elapsed` | Duration in milliseconds and seconds |
| `Read speed` | Throughput calculated as (bytes ÷ 1MB) ÷ seconds |

### 4. Proof of Complete Read

```
=== Proof of Complete Read ===
First 4 bytes (hex): 46 49 4C 45  (ASCII: FILE)
Last 4 bytes (hex):  00 00 00 00  (ASCII: ....)

Note: First 4 bytes should be 'FILE' (46 49 4C 45) - the MFT record signature.
```

| Field | Description |
|-------|-------------|
| `First 4 bytes` | First 4 bytes of MFT (should be "FILE" signature) |
| `Last 4 bytes` | Last 4 bytes of MFT data read |

The "FILE" signature (hex: `46 49 4C 45`) confirms that the tool is correctly reading the MFT, as every valid MFT record begins with this magic number.

---

## Methodology

### Reading Strategy

1. **Open Volume** - Opens `\\.\X:` with `FILE_FLAG_NO_BUFFERING` for unbuffered I/O
2. **Get Volume Data** - Uses `FSCTL_GET_NTFS_VOLUME_DATA` to get MFT location and sizes
3. **Get Extents** - Uses `FSCTL_GET_RETRIEVAL_POINTERS` on `X:\$MFT` to get extent map
4. **Sequential Read** - Reads MFT data extent-by-extent in 1MB chunks

### Buffer Configuration

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Buffer Size | 1 MB | Balance between throughput and memory usage |
| Alignment | Sector-aligned | Required for `FILE_FLAG_NO_BUFFERING` |
| I/O Mode | Synchronous | Simple, predictable measurement |

### Timing

- Uses `std::chrono::high_resolution_clock` for precise timing
- Timer starts immediately before first `ReadFile()` call
- Timer stops immediately after last `ReadFile()` completes
- Does **not** include volume open, extent retrieval, or output formatting time

---

## Interpreting Results

### Typical Read Speeds

| Storage Type | Expected Speed |
|--------------|----------------|
| HDD (7200 RPM) | 80-150 MB/s |
| SATA SSD | 400-550 MB/s |
| NVMe SSD | 1000-3500 MB/s |
| RAM Disk | 5000+ MB/s |

### Factors Affecting Performance

1. **Disk Type** - SSD vs HDD makes the biggest difference
2. **Fragmentation** - More extents = more seek overhead (especially on HDD)
3. **System Load** - Other disk I/O competes for bandwidth
4. **Caching** - Repeated runs may be faster if MFT is cached
5. **Antivirus** - Real-time scanning can slow raw disk access

### Fragmentation Impact

```
Extents: 1    → Contiguous MFT, optimal read pattern
Extents: 3    → Slightly fragmented, minor impact
Extents: 100+ → Heavily fragmented, may significantly impact HDD performance
```

---

## Example Output

```
=== MFT Read Benchmark Tool ===
Drive: C:

Volume Information:
  BytesPerSector: 512
  BytesPerCluster: 4096
  BytesPerFileRecordSegment: 1024
  MftValidDataLength: 1073741824
  MftStartLcn: 786432

MFT Information:
  Extents: 2
  MFT Size: 1073741824 bytes (1024 MB)
  Record Size: 1024 bytes
  Record Count: 1048576
  Total Bytes to Read: 1073741824

Starting MFT read benchmark...

=== Benchmark Results ===
Total bytes read: 1073741824 (1024 MB)
Total records: 1048576
Time elapsed: 2156 ms (2.156 seconds)
Read speed: 474.88 MB/s

=== Proof of Complete Read ===
First 4 bytes (hex): 46 49 4C 45  (ASCII: FILE)
Last 4 bytes (hex):  00 00 00 00  (ASCII: ....)

Note: First 4 bytes should be 'FILE' (46 49 4C 45) - the MFT record signature.
```

---

## Error Conditions

| Error | Cause | Solution |
|-------|-------|----------|
| `Failed to open volume (error 5)` | Access denied | Run as Administrator |
| `Failed to get NTFS volume data` | Not NTFS volume | Use NTFS-formatted drive |
| `No MFT extents found` | Cannot access $MFT | Check volume health |
| `Failed to seek` | Disk error | Check disk for errors |
| `Failed to read from volume` | I/O error | Check disk health |

---

## Comparison: `--benchmark-mft` vs `--benchmark-index`

| Aspect | `--benchmark-mft` | `--benchmark-index` |
|--------|-------------------|---------------------|
| **What it measures** | Raw disk read speed | Full indexing pipeline |
| **I/O Mode** | Synchronous | Async (IOCP) |
| **Parsing** | None (just reads bytes) | Full MFT record parsing |
| **Index Building** | None | Builds complete NtfsIndex |
| **Bitmap Optimization** | No | Yes (skips unused records) |
| **Typical Speed** | Disk-limited | CPU+Disk balanced |
| **Use Case** | Baseline disk speed | Real-world performance |

---

# Index Build Benchmark (`--benchmark-index`)

This benchmark measures the **actual UFFS indexing performance**—what users experience when launching the application.

---

## What It Measures

| Metric | Description |
|--------|-------------|
| **Time Elapsed** | Wall-clock time from start to index complete |
| **CPU Time** | Actual CPU time consumed |
| **Files** | Number of files indexed |
| **Directories** | Number of directories indexed |
| **MFT Read Speed** | Effective throughput (MFT size ÷ time) |
| **Records/sec** | MFT records processed per second |
| **Files+Dirs/sec** | Entries indexed per second |

---

## Output Format

### Volume Information

```
=== Volume Information ===
MFT Capacity: 1048576 records
MFT Record Size: 1024 bytes
MFT Total Size: 1073741824 bytes (1024 MB)
```

### Index Statistics

```
=== Index Statistics ===
Records Processed: 1048576
Files: 892451
Directories: 156125
Total Entries: 1048576
Name Entries: 1048576
Names + Streams: 1152034
```

### Benchmark Results

```
=== Benchmark Results ===
Time Elapsed: 1823 ms (1.823 seconds)
CPU Time: 1.456 seconds
MFT Read Speed: 561.45 MB/s
Record Processing: 575123 records/sec
File Indexing: 575123 files+dirs/sec

=== Summary ===
Indexed 1048576 items in 1.823 seconds
```

---

## Example Output

```
=== Index Build Benchmark Tool ===
Drive: C:
This measures the full UFFS indexing pipeline (async I/O + parsing + index building)

Creating index for C:\ ...
Indexing in progress...

=== Volume Information ===
MFT Capacity: 524288 records
MFT Record Size: 1024 bytes
MFT Total Size: 536870912 bytes (512 MB)

=== Index Statistics ===
Records Processed: 524288
Files: 423156
Directories: 101132
Total Entries: 524288
Name Entries: 524288
Names + Streams: 576512

=== Benchmark Results ===
Time Elapsed: 1245 ms (1.245 seconds)
CPU Time: 0.987 seconds
MFT Read Speed: 411.12 MB/s
Record Processing: 421116 records/sec
File Indexing: 421116 files+dirs/sec

=== Summary ===
Indexed 524288 items in 1.245 seconds
```

---

## How It Works

The `--benchmark-index` command uses the **real UFFS indexing pipeline**:

```
┌─────────────────────────────────────────────────────────────┐
│  1. Create NtfsIndex for the volume                         │
│  2. Create IoCompletionPort (IOCP)                          │
│  3. Post OverlappedNtfsMftReadPayload to IOCP               │
│  4. IOCP worker threads:                                    │
│     - Read MFT chunks asynchronously                        │
│     - Parse FILE_RECORD_SEGMENT_HEADER                      │
│     - Extract $FILE_NAME, $DATA, $STANDARD_INFORMATION      │
│     - Build index structures (records, names, streams)      │
│  5. Wait for finished_event                                 │
│  6. Report statistics                                       │
└─────────────────────────────────────────────────────────────┘
```

This is **exactly** what happens when UFFS starts up and indexes a volume.

---

## Interpreting Results

### Typical Index Build Speeds

| Storage Type | Files/sec | Time for 1M files |
|--------------|-----------|-------------------|
| HDD (7200 RPM) | 100K-200K | 5-10 seconds |
| SATA SSD | 300K-500K | 2-3 seconds |
| NVMe SSD | 500K-1M+ | 1-2 seconds |

### CPU vs I/O Bound

Compare **Time Elapsed** vs **CPU Time**:

- **CPU Time ≈ Elapsed Time**: CPU-bound (parsing is the bottleneck)
- **CPU Time << Elapsed Time**: I/O-bound (disk is the bottleneck)

On fast NVMe SSDs, indexing is typically CPU-bound.

---

*Document updated January 2026*

