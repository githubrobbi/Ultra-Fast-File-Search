# MFT Reading Deep Dive

## Introduction

This document provides an exhaustive technical analysis of how Ultra Fast File Search reads the NTFS Master File Table (MFT) directly, bypassing the Windows file system APIs. This is the core technique that enables sub-second searches across millions of files.

A developer reading this document should be able to reimplement the MFT reading logic from scratch.

---

## Why Direct MFT Reading?

### The Problem with Standard APIs

Traditional file enumeration using `FindFirstFile`/`FindNextFile` or `std::filesystem::directory_iterator` is slow because:

1. **Per-file syscall overhead**: Each file requires kernel transitions
2. **Security checks**: ACLs evaluated for every file access
3. **Handle management**: `CreateFile`/`CloseHandle` pairs for metadata
4. **Directory traversal**: Recursive descent through directory tree
5. **Attribute queries**: Separate calls for size, timestamps, etc.

**Benchmark**: Standard enumeration: ~1,000-5,000 files/second
**MFT direct read**: ~500,000-2,000,000 files/second

### The MFT Advantage

The Master File Table is a single, mostly-contiguous file (`$MFT`) containing ALL file metadata on an NTFS volume. By reading it directly:

- **Single sequential read** instead of millions of random accesses
- **No security checks** (we have raw volume access)
- **No handle overhead** (one handle for entire volume)
- **All metadata in one place** (names, sizes, timestamps, attributes)

---

## NTFS On-Disk Structures

### Boot Sector (`NTFS_BOOT_SECTOR`)

**Location**: Sector 0 of the volume
**Size**: 512 bytes

```cpp
struct NTFS_BOOT_SECTOR {
    unsigned char Jump[3];              // 0x00: Jump instruction
    unsigned char Oem[8];               // 0x03: "NTFS    "
    unsigned short BytesPerSector;      // 0x0B: Usually 512
    unsigned char SectorsPerCluster;    // 0x0D: Power of 2 (1-128)
    unsigned short ReservedSectors;     // 0x0E: Always 0
    unsigned char Padding1[3];          // 0x10: Always 0
    unsigned short Unused1;             // 0x13: Always 0
    unsigned char MediaDescriptor;      // 0x15: 0xF8 for hard disk
    unsigned short Padding2;            // 0x16: Always 0
    unsigned short SectorsPerTrack;     // 0x18: Geometry
    unsigned short NumberOfHeads;       // 0x1A: Geometry
    unsigned long HiddenSectors;        // 0x1C: Sectors before partition
    unsigned long Unused2;              // 0x20: Always 0
    unsigned long Unused3;              // 0x24: Always 0x80008000
    long long TotalSectors;             // 0x28: Volume size
    long long MftStartLcn;              // 0x30: *** MFT location ***
    long long Mft2StartLcn;             // 0x38: MFT mirror location
    signed char ClustersPerFileRecordSegment; // 0x40: Usually -10 (1024 bytes)
    unsigned char Padding3[3];          // 0x41
    unsigned long ClustersPerIndexBlock;// 0x44: Usually 1
    long long VolumeSerialNumber;       // 0x48
    unsigned long Checksum;             // 0x50
    unsigned char BootStrap[426];       // 0x54: Boot code

    // Derived calculations
    unsigned int file_record_size() const {
        // If negative, it's a power of 2 (e.g., -10 = 2^10 = 1024)
        return ClustersPerFileRecordSegment >= 0
            ? ClustersPerFileRecordSegment * SectorsPerCluster * BytesPerSector
            : 1U << static_cast<int>(-ClustersPerFileRecordSegment);
    }

    unsigned int cluster_size() const {
        return SectorsPerCluster * BytesPerSector;
    }
};
```

**Key Fields**:
- `MftStartLcn`: Logical Cluster Number where $MFT begins
- `ClustersPerFileRecordSegment`: Usually -10 (meaning 2^10 = 1024 bytes per record)

### File Record Segment Header (`FILE_RECORD_SEGMENT_HEADER`)

Each file/directory in NTFS has one or more File Record Segments (FRS) in the MFT.

**Size**: Typically 1024 bytes (but can vary)

```cpp
struct FILE_RECORD_SEGMENT_HEADER {
    MULTI_SECTOR_HEADER MultiSectorHeader;  // 0x00: Magic + USA info
    unsigned long long LogFileSequenceNumber; // 0x08: For journaling
    unsigned short SequenceNumber;          // 0x10: Incremented on reuse
    unsigned short LinkCount;               // 0x12: Hard link count
    unsigned short FirstAttributeOffset;    // 0x14: Offset to first attribute
    unsigned short Flags;                   // 0x16: FRH_IN_USE | FRH_DIRECTORY
    unsigned long BytesInUse;               // 0x18: Used portion of record
    unsigned long BytesAllocated;           // 0x1C: Total record size
    unsigned long long BaseFileRecordSegment; // 0x20: Base FRS for extensions
    unsigned short NextAttributeNumber;     // 0x28: Next attribute ID
    unsigned short SegmentNumberUpper;      // 0x2A: (Unreliable)
    unsigned long SegmentNumberLower;       // 0x2C: FRS number
};

enum FILE_RECORD_HEADER_FLAGS {
    FRH_IN_USE    = 0x0001,  // Record contains valid file
    FRH_DIRECTORY = 0x0002,  // Record is a directory
};
```

**Critical Check**: `Magic == 'FILE'` (0x454C4946) and `Flags & FRH_IN_USE`

### Multi-Sector Header and Update Sequence Array (USA)

NTFS protects against torn writes using the Update Sequence Array:

```cpp
struct MULTI_SECTOR_HEADER {
    unsigned long Magic;        // 'FILE' for MFT records
    unsigned short USAOffset;   // Offset to USA
    unsigned short USACount;    // Number of USA entries

    bool unfixup(size_t max_size) {
        // USA is an array of unsigned shorts
        unsigned short* usa = (unsigned short*)((char*)this + USAOffset);
        unsigned short usa0 = usa[0];  // The "check value"

        bool result = true;
        for (unsigned short i = 1; i < USACount; i++) {
            // Every 512 bytes, the last 2 bytes should equal usa0
            size_t offset = i * 512 - sizeof(unsigned short);
            unsigned short* check = (unsigned short*)((char*)this + offset);

            if (offset < max_size) {
                result &= (*check == usa0);  // Verify integrity
                *check = usa[i];             // Restore original value
            }
        }
        return result;
    }
};
```

**Why This Matters**:
- Disk sectors are 512 bytes
- If a write is interrupted, the USA detects corruption
- We MUST call `unfixup()` before parsing any record
- If `unfixup()` returns false, the record is corrupted → mark as 'BAAD'

### Attribute Record Header (`ATTRIBUTE_RECORD_HEADER`)

Each FRS contains a list of attributes. Attributes store all file metadata.

```cpp
enum AttributeTypeCode {
    AttributeStandardInformation = 0x10,  // Timestamps, flags
    AttributeAttributeList       = 0x20,  // For large files
    AttributeFileName            = 0x30,  // Filename + parent ref
    AttributeObjectId            = 0x40,  // GUID
    AttributeSecurityDescriptor  = 0x50,  // ACLs
    AttributeVolumeName          = 0x60,  // Volume label
    AttributeVolumeInformation   = 0x70,  // NTFS version
    AttributeData                = 0x80,  // File content
    AttributeIndexRoot           = 0x90,  // Directory B-tree root
    AttributeIndexAllocation     = 0xA0,  // Directory B-tree nodes
    AttributeBitmap              = 0xB0,  // Allocation bitmap
    AttributeReparsePoint        = 0xC0,  // Symlinks, mount points
    AttributeEAInformation       = 0xD0,  // Extended attributes
    AttributeEA                  = 0xE0,  // Extended attributes
    AttributePropertySet         = 0xF0,  // (Obsolete)
    AttributeLoggedUtilityStream = 0x100, // EFS
    AttributeEnd                 = -1,    // End marker
};

struct ATTRIBUTE_RECORD_HEADER {
    AttributeTypeCode Type;         // 0x00: Attribute type
    unsigned long Length;           // 0x04: Total attribute length
    unsigned char IsNonResident;    // 0x08: 0=resident, 1=non-resident
    unsigned char NameLength;       // 0x09: Attribute name length (chars)
    unsigned short NameOffset;      // 0x0A: Offset to attribute name
    unsigned short Flags;           // 0x0C: Compressed/Encrypted/Sparse
    unsigned short Instance;        // 0x0E: Unique ID within FRS

    union {
        struct RESIDENT {
            unsigned long ValueLength;   // 0x10: Data length
            unsigned short ValueOffset;  // 0x14: Offset to data
            unsigned short Flags;        // 0x16: Indexed flag
        } Resident;

        struct NONRESIDENT {
            long long LowestVCN;         // 0x10: Starting VCN
            long long HighestVCN;        // 0x18: Ending VCN
            unsigned short MappingPairsOffset; // 0x20: Offset to run list
            unsigned char CompressionUnit;     // 0x22: Log2 of compression unit
            unsigned char Reserved[5];         // 0x23
            long long AllocatedSize;     // 0x28: Allocated on disk
            long long DataSize;          // 0x30: Logical size
            long long InitializedSize;   // 0x38: Valid data length
            long long CompressedSize;    // 0x40: If compressed
        } NonResident;
    };

    // Navigation helpers
    ATTRIBUTE_RECORD_HEADER* next() {
        return (ATTRIBUTE_RECORD_HEADER*)((char*)this + Length);
    }
};
```

### Filename Information (`FILENAME_INFORMATION`)

The `$FILE_NAME` attribute (0x30) contains the filename and parent reference:

```cpp
struct FILENAME_INFORMATION {
    unsigned long long ParentDirectory;    // 0x00: Parent FRS + sequence
    long long CreationTime;                // 0x08: File creation time
    long long LastModificationTime;        // 0x10: Last write time
    long long LastChangeTime;              // 0x18: MFT record change time
    long long LastAccessTime;              // 0x20: Last access time
    long long AllocatedLength;             // 0x28: Allocated size
    long long FileSize;                    // 0x30: Logical size
    unsigned long FileAttributes;          // 0x38: DOS attributes
    unsigned short PackedEaSize;           // 0x3C: Extended attributes
    unsigned short Reserved;               // 0x3E
    unsigned char FileNameLength;          // 0x40: Filename length (chars)
    unsigned char Flags;                   // 0x41: Namespace flags
    wchar_t FileName[1];                   // 0x42: Variable-length filename
};

// Filename namespace flags
enum {
    FILE_NAME_POSIX = 0x00,  // Case-sensitive, any Unicode
    FILE_NAME_WIN32 = 0x01,  // Windows long filename
    FILE_NAME_DOS   = 0x02,  // 8.3 short filename
    FILE_NAME_WIN32_AND_DOS = 0x03,  // Both in one
};
```

**Important**: Files often have MULTIPLE `$FILE_NAME` attributes:
- One for the long name (WIN32)
- One for the 8.3 short name (DOS)
- We skip `FILE_NAME_DOS` (0x02) to avoid duplicates

### Standard Information (`STANDARD_INFORMATION`)

The `$STANDARD_INFORMATION` attribute (0x10) contains timestamps and flags:

```cpp
struct STANDARD_INFORMATION {
    long long CreationTime;           // 0x00
    long long LastModificationTime;   // 0x08
    long long LastChangeTime;         // 0x10
    long long LastAccessTime;         // 0x18
    unsigned long FileAttributes;     // 0x20: FILE_ATTRIBUTE_* flags
    // ... additional fields for NTFS 3.0+
};
```

---

## The MFT Reading Pipeline

### Phase 1: Volume Access

```cpp
// Open volume with direct access (requires admin privileges)
HANDLE volume = CreateFile(
    L"\\\\.\\C:",                    // Volume path
    GENERIC_READ,                     // Read access
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,  // Direct + async
    NULL
);
```

**Flags Explained**:
- `FILE_FLAG_NO_BUFFERING`: Bypass file system cache for raw access
- `FILE_FLAG_OVERLAPPED`: Enable async I/O for parallelism

### Phase 2: Get Volume Metadata

```cpp
NTFS_VOLUME_DATA_BUFFER info;
DWORD bytesReturned;
DeviceIoControl(
    volume,
    FSCTL_GET_NTFS_VOLUME_DATA,  // Get NTFS-specific info
    NULL, 0,
    &info, sizeof(info),
    &bytesReturned, NULL
);

// Extract critical values
unsigned int cluster_size = info.BytesPerCluster;        // e.g., 4096
unsigned int mft_record_size = info.BytesPerFileRecordSegment; // e.g., 1024
unsigned int mft_capacity = info.MftValidDataLength.QuadPart / mft_record_size;
long long mft_start_lcn = info.MftStartLcn.QuadPart;
```

### Phase 3: Get MFT Extent Map (Retrieval Pointers)

The MFT itself can be fragmented. We need its "run list" to know where all pieces are:

```cpp
// Get retrieval pointers for $MFT::$DATA
std::vector<std::pair<unsigned long long, long long>> get_retrieval_pointers(
    const wchar_t* path,  // e.g., "C:\\$MFT::$DATA"
    long long* size_out,
    long long mft_start_lcn,
    unsigned int mft_record_size
) {
    HANDLE h = CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL, OPEN_EXISTING, 0, NULL);

    STARTING_VCN_INPUT_BUFFER vcn_input = { 0 };
    RETRIEVAL_POINTERS_BUFFER* rp = ...;  // Allocate buffer

    DeviceIoControl(h, FSCTL_GET_RETRIEVAL_POINTERS,
                    &vcn_input, sizeof(vcn_input),
                    rp, buffer_size, &bytesReturned, NULL);

    // rp->Extents[] contains (NextVcn, Lcn) pairs
    // Convert to (VCN, LCN) pairs for reading
    std::vector<std::pair<ull, ll>> result;
    for (DWORD i = 0; i < rp->ExtentCount; i++) {
        result.push_back({rp->Extents[i].NextVcn.QuadPart,
                          rp->Extents[i].Lcn.QuadPart});
    }
    return result;
}
```

**Output**: A list of (VCN, LCN) pairs describing where each MFT fragment is on disk.



---

## OPTIMIZATION #1: The MFT Bitmap Skip

### The Problem

The MFT contains slots for ALL files ever created, including deleted ones. On a typical system:
- MFT might have 10 million slots
- Only 2 million are in use
- Reading 8 million empty slots wastes 80% of I/O time

### The Solution: `$MFT::$BITMAP`

NTFS maintains a bitmap (`$MFT::$BITMAP`) where each bit indicates if an MFT slot is in use:

```cpp
// Read the MFT bitmap FIRST
std::vector<unsigned char> mft_bitmap;
mft_bitmap.resize(mft_capacity / 8);  // 1 bit per record

// Read $MFT::$BITMAP using same retrieval pointer technique
auto bitmap_extents = get_retrieval_pointers("C:\\$MFT::$BITMAP", ...);
// ... read bitmap data ...

// Now we know which records to skip!
```

### Implementation in UFFS

```cpp
class OverlappedNtfsMftReadPayload {
    Bitmap mft_bitmap;  // The bitmap data
    RetPtrs bitmap_ret_ptrs;  // Extents for $BITMAP
    RetPtrs data_ret_ptrs;    // Extents for $DATA

    // After bitmap is fully read, calculate skip ranges
    void calculate_skip_ranges() {
        for (auto& extent : data_ret_ptrs) {
            size_t irecord = extent.vcn * cluster_size / mft_record_size;
            size_t nrecords = extent.cluster_count * cluster_size / mft_record_size;

            // Scan from beginning: how many unused records?
            size_t skip_begin = 0;
            for (; skip_begin < nrecords; ++skip_begin) {
                size_t j = irecord + skip_begin;
                if (mft_bitmap[j / 8] & (1 << (j % 8))) {
                    break;  // Found a used record
                }
            }

            // Scan from end: how many unused records?
            size_t skip_end = 0;
            for (; skip_end < nrecords - skip_begin; ++skip_end) {
                size_t j = irecord + nrecords - 1 - skip_end;
                if (mft_bitmap[j / 8] & (1 << (j % 8))) {
                    break;  // Found a used record
                }
            }

            // Store skip counts (converted to clusters)
            extent.skip_begin = skip_begin * mft_record_size / cluster_size;
            extent.skip_end = skip_end * mft_record_size / cluster_size;
        }
    }
};
```

### The Optimization Effect

```
Before: Read 10GB of MFT data
After:  Read 2GB of MFT data (skip 8GB of unused records)
Speedup: 5x for I/O-bound operations
```

**Note**: The code comments acknowledge this is a bit-by-bit scan which could be optimized further, but it's still much faster than the I/O it saves.

---

## OPTIMIZATION #2: Async I/O with Completion Ports

### The Problem

Sequential disk reads waste CPU time waiting for I/O:

```
[CPU idle] → [Read 1MB] → [CPU idle] → [Read 1MB] → ...
```

### The Solution: I/O Completion Ports

UFFS uses Windows I/O Completion Ports for maximum parallelism:

```cpp
class IoCompletionPort {
    Handle handle;  // CreateIoCompletionPort handle

    IoCompletionPort() {
        handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    }

    void associate(HANDLE file, uintptr_t key) {
        CreateIoCompletionPort(file, handle, key, 0);
    }

    void read_file(HANDLE volume, void* buffer, DWORD size,
                   intrusive_ptr<Overlapped> op) {
        OVERLAPPED* ovl = op.get();
        ReadFile(volume, buffer, size, NULL, ovl);
        // Completion will be posted to the port
    }

    bool get(size_t& size, uintptr_t& key, Overlapped*& payload, DWORD timeout) {
        return GetQueuedCompletionStatus(handle, &size, &key,
                                         (OVERLAPPED**)&payload, timeout);
    }
};
```

### Pipeline Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    I/O Completion Port                          │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │
│  │ Read #1 │  │ Read #2 │  │ Read #3 │  │ Read #4 │  ...       │
│  │ (1MB)   │  │ (1MB)   │  │ (1MB)   │  │ (1MB)   │            │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘            │
│       │            │            │            │                  │
│       ▼            ▼            ▼            ▼                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │              Completion Queue                                ││
│  │  [Done #1] → [Done #3] → [Done #2] → [Done #4] → ...        ││
│  └─────────────────────────────────────────────────────────────┘│
│       │                                                         │
│       ▼                                                         │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │              Worker Thread                                   ││
│  │  Process completions, parse MFT records, queue next reads   ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

### Concurrency Control

```cpp
// Start with 2 concurrent reads
for (int concurrency = 0; concurrency < 2; ++concurrency) {
    this->queue_next();
}

// Each completion triggers the next read
int ReadOperation::operator()(size_t size, uintptr_t key) {
    // Process completed read
    q->p->load(virtual_offset, buffer, size, skipped_begin, skipped_end);

    // Queue next read (maintains concurrency level)
    q->queue_next();

    return -1;  // Don't requeue this operation
}
```


---

## OPTIMIZATION #3: Memory Pool for Read Buffers

### The Problem

Allocating/freeing 1MB buffers for each read is expensive:
- `malloc`/`free` overhead
- Memory fragmentation
- Cache pollution

### The Solution: Buffer Recycling

```cpp
class ReadOperation : public Overlapped {
    static std::vector<std::pair<size_t, void*>> recycled;
    static recursive_mutex recycled_mutex;

    static void* operator new(size_t n, size_t buffer_size) {
        void* p = nullptr;

        {
            unique_lock<recursive_mutex> guard(recycled_mutex);

            // Find smallest buffer that fits
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
                p = recycled[best].second;
                recycled.erase(recycled.begin() + best);
            }
        }

        if (!p) {
            p = malloc(n + buffer_size);  // Use malloc for _msize()
        }
        return p;
    }

    static void operator delete(void* p) {
        // Don't free - recycle for next read
        unique_lock<recursive_mutex> guard(recycled_mutex);
        recycled.push_back({_msize(p), p});
    }
};
```

**Effect**: After warmup, zero allocations during MFT reading.

---

## MFT Record Parsing

### The Complete Parsing Flow

```cpp
void NtfsMftReadPayload::load(
    unsigned long long virtual_offset,
    unsigned char* buffer,
    size_t size,
    size_t skipped_begin,
    size_t skipped_end
) {
    // Calculate which MFT records are in this buffer
    size_t first_record = virtual_offset / mft_record_size;
    size_t record_count = size / mft_record_size;

    for (size_t i = 0; i < record_count; ++i) {
        size_t record_index = first_record + i;
        unsigned char* record = buffer + (i * mft_record_size);

        // Check if record is in skipped region (unused)
        if (i < skipped_begin || i >= record_count - skipped_end) {
            continue;  // Skip unused records
        }

        parse_mft_record(record_index, record);
    }
}

void parse_mft_record(size_t record_index, unsigned char* record) {
    FILE_RECORD_SEGMENT_HEADER* header = (FILE_RECORD_SEGMENT_HEADER*)record;

    // Step 1: Validate magic number
    if (header->MultiSectorHeader.Magic != 'FILE') {
        return;  // Not a valid record
    }

    // Step 2: Apply USA fixup (critical!)
    if (!header->MultiSectorHeader.unfixup(mft_record_size)) {
        // Record is corrupted - mark as 'BAAD'
        header->MultiSectorHeader.Magic = 'BAAD';
        return;
    }

    // Step 3: Check if record is in use
    if (!(header->Flags & FRH_IN_USE)) {
        return;  // Deleted file
    }

    // Step 4: Parse attributes
    bool is_directory = (header->Flags & FRH_DIRECTORY) != 0;
    ATTRIBUTE_RECORD_HEADER* attr = (ATTRIBUTE_RECORD_HEADER*)
        ((char*)header + header->FirstAttributeOffset);

    while ((char*)attr < (char*)header + header->BytesInUse) {
        if (attr->Type == AttributeEnd) break;
        if (attr->Length == 0) break;  // Prevent infinite loop

        switch (attr->Type) {
            case AttributeFileName:
                parse_filename_attribute(record_index, attr, is_directory);
                break;
            case AttributeStandardInformation:
                parse_standard_info(record_index, attr);
                break;
            case AttributeData:
                parse_data_attribute(record_index, attr);
                break;
        }

        attr = attr->next();
    }
}
```

### Parsing the Filename Attribute

```cpp
void parse_filename_attribute(
    size_t record_index,
    ATTRIBUTE_RECORD_HEADER* attr,
    bool is_directory
) {
    // Filename is always resident
    FILENAME_INFORMATION* fn = (FILENAME_INFORMATION*)
        ((char*)attr + attr->Resident.ValueOffset);

    // Skip DOS-only names (8.3 short names)
    if (fn->Flags == FILE_NAME_DOS) {
        return;  // We'll get the long name from another attribute
    }

    // Extract parent reference
    unsigned long long parent_frs = fn->ParentDirectory & 0x0000FFFFFFFFFFFF;
    unsigned short parent_seq = (fn->ParentDirectory >> 48) & 0xFFFF;

    // Extract filename
    std::wstring filename(fn->FileName, fn->FileNameLength);

    // Store in our index
    FileInfo info;
    info.frs_index = record_index;
    info.parent_frs = parent_frs;
    info.filename = filename;
    info.is_directory = is_directory;
    info.file_size = fn->FileSize;
    info.creation_time = fn->CreationTime;
    info.modification_time = fn->LastModificationTime;
    info.attributes = fn->FileAttributes;

    // Add to the file index
    file_index.add(info);
}
```

---

## Building the Directory Tree

### The Parent Reference Problem

Each file stores its parent's FRS index, but we need full paths for display. The solution:

```cpp
class FileIndex {
    std::vector<FileInfo> files;  // Indexed by FRS

    std::wstring get_full_path(size_t frs) {
        std::vector<std::wstring> components;

        size_t current = frs;
        while (current != 5) {  // FRS 5 is the root directory
            FileInfo& info = files[current];
            components.push_back(info.filename);
            current = info.parent_frs;
        }

        // Reverse and join
        std::wstring path = L"C:";
        for (auto it = components.rbegin(); it != components.rend(); ++it) {
            path += L"\\" + *it;
        }
        return path;
    }
};
```

### Special MFT Records

The first 16 MFT records are reserved for system files:

| FRS | Name | Purpose |
|-----|------|---------|
| 0 | $MFT | The MFT itself |
| 1 | $MFTMirr | MFT mirror (first 4 records) |
| 2 | $LogFile | Transaction log |
| 3 | $Volume | Volume information |
| 4 | $AttrDef | Attribute definitions |
| 5 | . (root) | Root directory |
| 6 | $Bitmap | Cluster allocation bitmap |
| 7 | $Boot | Boot sector |
| 8 | $BadClus | Bad cluster list |
| 9 | $Secure | Security descriptors |
| 10 | $UpCase | Uppercase table |
| 11 | $Extend | Extended metadata |
| 12-15 | Reserved | Future use |

---

## Complete Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         MFT Reading Pipeline                            │
└─────────────────────────────────────────────────────────────────────────┘

1. INITIALIZATION
   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
   │ Open Volume  │────▶│ Get NTFS     │────▶│ Get $MFT     │
   │ \\.\C:       │     │ Volume Data  │     │ Ret Pointers │
   └──────────────┘     └──────────────┘     └──────────────┘
                                                    │
                                                    ▼
2. BITMAP PHASE                              ┌──────────────┐
   ┌──────────────┐     ┌──────────────┐     │ Get $BITMAP  │
   │ Read Bitmap  │◀────│ Queue Bitmap │◀────│ Ret Pointers │
   │ Extents      │     │ Reads        │     └──────────────┘
   └──────────────┘     └──────────────┘
          │
          ▼
3. SKIP CALCULATION
   ┌──────────────────────────────────────────────────────────┐
   │ For each $DATA extent:                                   │
   │   - Scan bitmap from start → find first used record      │
   │   - Scan bitmap from end → find last used record         │
   │   - Calculate skip_begin and skip_end                    │
   └──────────────────────────────────────────────────────────┘
          │
          ▼
4. MFT DATA PHASE
   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
   │ Queue Read   │────▶│ Async Read   │────▶│ Completion   │
   │ Operations   │     │ (1MB chunks) │     │ Port         │
   └──────────────┘     └──────────────┘     └──────────────┘
          ▲                                         │
          │                                         ▼
          │                                  ┌──────────────┐
          └──────────────────────────────────│ Process      │
                                             │ Completion   │
                                             └──────────────┘
                                                    │
                                                    ▼
5. RECORD PARSING
   ┌──────────────────────────────────────────────────────────┐
   │ For each 1KB record in buffer:                           │
   │   1. Check magic == 'FILE'                               │
   │   2. Apply USA unfixup                                   │
   │   3. Check FRH_IN_USE flag                               │
   │   4. Parse $FILE_NAME attribute                          │
   │   5. Extract: name, parent, size, timestamps             │
   │   6. Add to file index                                   │
   └──────────────────────────────────────────────────────────┘
          │
          ▼
6. INDEX BUILDING
   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
   │ Build Parent │────▶│ Build Search │────▶│ Ready for    │
   │ References   │     │ Index        │     │ Queries      │
   └──────────────┘     └──────────────┘     └──────────────┘
```

---

## Error Handling

### Common Failure Modes

1. **Access Denied**: Volume access requires admin privileges
2. **Corrupted Records**: USA check fails → mark as 'BAAD', skip
3. **Fragmented MFT**: Handle multiple extents via retrieval pointers
4. **Concurrent Modifications**: MFT can change during read → handle gracefully

### Robustness Strategies

```cpp
// Handle corrupted records gracefully
if (!header->MultiSectorHeader.unfixup(mft_record_size)) {
    stats.corrupted_records++;
    continue;  // Skip this record, don't crash
}

// Validate attribute boundaries
if ((char*)attr + attr->Length > (char*)header + header->BytesInUse) {
    break;  // Attribute extends beyond record - stop parsing
}

// Handle missing parent references
if (info.parent_frs >= files.size() || !files[info.parent_frs].valid) {
    info.parent_frs = 5;  // Orphan to root directory
}
```

---

## Performance Characteristics

### Typical Performance (NVMe SSD, 2M files)

| Phase | Time | Notes |
|-------|------|-------|
| Volume open | <1ms | One-time |
| Get retrieval pointers | ~5ms | Two calls ($DATA, $BITMAP) |
| Read bitmap | ~10ms | Small file |
| Calculate skips | ~5ms | CPU-bound |
| Read MFT data | ~200ms | I/O-bound, ~2GB |
| Parse records | ~100ms | CPU-bound, parallel with I/O |
| Build index | ~50ms | CPU-bound |
| **Total** | **~350ms** | For 2 million files |

### Comparison with Standard APIs

| Method | Time for 2M files | Speedup |
|--------|-------------------|---------|
| FindFirstFile/FindNextFile | ~30 minutes | 1x |
| std::filesystem | ~25 minutes | 1.2x |
| Direct MFT reading | ~350ms | 5000x |

---

## Summary

The MFT reading technique achieves its speed through:

1. **Single sequential read** instead of millions of random accesses
2. **Bitmap-based skipping** of unused records
3. **Async I/O** with completion ports for parallelism
4. **Buffer recycling** to eliminate allocation overhead
5. **Direct parsing** of on-disk structures without API overhead

This enables sub-second indexing of millions of files, making real-time search possible.