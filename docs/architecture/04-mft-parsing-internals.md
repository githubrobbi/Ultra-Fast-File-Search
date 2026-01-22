# MFT Parsing Internals

## Introduction

This document provides exhaustive detail on how Ultra Fast File Search parses NTFS Master File Table (MFT) records. After reading this document, you should be able to:

1. Parse any MFT FILE record from raw bytes
2. Extract all file metadata (names, timestamps, sizes, attributes)
3. Handle edge cases (hard links, extended records, corrupted data)
4. Build the in-memory index structures used for searching

---

## Overview: From Bytes to Searchable Index

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MFT Parsing Pipeline                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Raw MFT Bytes                                                          │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ 46 49 4C 45 30 00 03 00 ... (1024 bytes per record)              │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 1: Validate Magic Number                                          │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ Check: Magic == 'FILE' (0x454C4946, stored as 'ELIF' in memory)  │  │
│  │ If not 'FILE', skip this record                                   │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 2: Apply USA Fixup                                                │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ Verify and restore bytes at sector boundaries (every 512 bytes)  │  │
│  │ If fixup fails, mark as 'BAAD' and skip                          │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 3: Check Record Flags                                             │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ If !(Flags & FRH_IN_USE), record is deleted - skip               │  │
│  │ Note: FRH_DIRECTORY indicates this is a directory                │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 4: Iterate Attributes                                             │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ For each attribute until AttributeEnd (0xFFFFFFFF):              │  │
│  │   - $STANDARD_INFORMATION (0x10): timestamps, file attributes    │  │
│  │   - $FILE_NAME (0x30): filename, parent reference                │  │
│  │   - $DATA (0x80): file size, data runs                           │  │
│  │   - $INDEX_ROOT/$INDEX_ALLOCATION: directory info                │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 5: Build Index Structures                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ Store in: Records, LinkInfos, StreamInfos, ChildInfos, names     │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## FILE Record Structure

### Complete Layout (1024 bytes typical)

```
Offset  Size  Field                          Description
──────  ────  ─────                          ───────────
0x00    4     Magic                          'FILE' (0x454C4946)
0x04    2     USAOffset                      Offset to Update Sequence Array
0x06    2     USACount                       Number of USA entries
0x08    8     LogFileSequenceNumber          LSN for journaling
0x10    2     SequenceNumber                 Incremented on record reuse
0x12    2     LinkCount                      Number of hard links
0x14    2     FirstAttributeOffset           Offset to first attribute
0x16    2     Flags                          FRH_IN_USE | FRH_DIRECTORY
0x18    4     BytesInUse                     Used portion of record
0x1C    4     BytesAllocated                 Total record size (usually 1024)
0x20    8     BaseFileRecordSegment          Base FRS for extension records
0x28    2     NextAttributeNumber            Next attribute instance ID
0x2A    2     SegmentNumberUpper             (Unreliable - do not use)
0x2C    4     SegmentNumberLower             This record's FRS number
0x30    ...   Update Sequence Array          USA data
...     ...   Attributes                     Variable-length attribute list
```

### C++ Structure Definition

```cpp
#pragma pack(push, 1)
struct MULTI_SECTOR_HEADER {
    unsigned long Magic;           // 'FILE' = 0x454C4946
    unsigned short USAOffset;      // Offset to USA from start of record
    unsigned short USACount;       // Number of USA entries (including check value)
};

enum FILE_RECORD_HEADER_FLAGS {
    FRH_IN_USE    = 0x0001,       // Record contains valid file/directory
    FRH_DIRECTORY = 0x0002,       // Record is a directory
};

struct FILE_RECORD_SEGMENT_HEADER {
    MULTI_SECTOR_HEADER MultiSectorHeader;
    unsigned long long LogFileSequenceNumber;
    unsigned short SequenceNumber;
    unsigned short LinkCount;
    unsigned short FirstAttributeOffset;
    unsigned short Flags;          // FILE_RECORD_HEADER_FLAGS
    unsigned long BytesInUse;
    unsigned long BytesAllocated;
    unsigned long long BaseFileRecordSegment;
    unsigned short NextAttributeNumber;
    unsigned short SegmentNumberUpper_or_Reserved;  // Unreliable!
    unsigned long SegmentNumberLower;
    
    // Navigation helpers
    ATTRIBUTE_RECORD_HEADER* begin() {
        return reinterpret_cast<ATTRIBUTE_RECORD_HEADER*>(
            reinterpret_cast<unsigned char*>(this) + this->FirstAttributeOffset);
    }
    
    void* end(size_t max_buffer_size = ~size_t()) {
        return reinterpret_cast<unsigned char*>(this) + 
            (max_buffer_size < this->BytesInUse ? max_buffer_size : this->BytesInUse);
    }
};
#pragma pack(pop)
```

### Magic Number Byte Order

**Critical Detail**: The magic number is stored in little-endian format:

```
On disk:  46 49 4C 45  ('F' 'I' 'L' 'E')
In memory as unsigned long: 0x454C4946
When compared as multi-char literal: 'ELIF'
```

The code checks `Magic == 'ELIF'` because C++ multi-character literals are stored with the first character in the least significant byte on little-endian systems.

---

## Update Sequence Array (USA) - Fixup Handling

### Why USA Exists

NTFS uses the Update Sequence Array to detect torn writes. When a sector (512 bytes) is written to disk, the last 2 bytes of each sector are replaced with a "check value". The original bytes are stored in the USA.

### USA Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│ FILE Record (1024 bytes = 2 sectors)                                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Offset 0x00: MULTI_SECTOR_HEADER                                       │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ Magic: 'FILE'  USAOffset: 0x30  USACount: 3                     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Offset 0x30: Update Sequence Array (USACount * 2 bytes)                │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ usa[0]: Check Value (e.g., 0x1234)                              │   │
│  │ usa[1]: Original bytes from offset 0x1FE (end of sector 1)      │   │
│  │ usa[2]: Original bytes from offset 0x3FE (end of sector 2)      │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Sector 1 (bytes 0x000 - 0x1FF):                                        │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ ... data ...                                    │ 0x1234 │      │   │
│  │                                                 └────────┘      │   │
│  │                                                 offset 0x1FE    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Sector 2 (bytes 0x200 - 0x3FF):                                        │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ ... data ...                                    │ 0x1234 │      │   │
│  │                                                 └────────┘      │   │
│  │                                                 offset 0x3FE    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### The unfixup() Algorithm

```cpp
bool unfixup(size_t max_size) {
    // Get pointer to USA (located at USAOffset from start of record)
    unsigned short* usa = reinterpret_cast<unsigned short*>(
        reinterpret_cast<unsigned char*>(this) + this->USAOffset);

    // usa[0] is the check value that should appear at end of each sector
    unsigned short const usa0 = usa[0];

    bool result = true;

    // For each sector (i=1 corresponds to first sector, i=2 to second, etc.)
    for (unsigned short i = 1; i < this->USACount; i++) {
        // Calculate offset to last 2 bytes of sector i
        // Sector 1 ends at 512, so check is at 512 - 2 = 510 = 0x1FE
        // Sector 2 ends at 1024, so check is at 1024 - 2 = 1022 = 0x3FE
        size_t const offset = i * 512 - sizeof(unsigned short);

        unsigned short* const check = reinterpret_cast<unsigned short*>(
            reinterpret_cast<unsigned char*>(this) + offset);

        if (offset < max_size) {
            // Verify: the check value should match usa[0]
            result &= (*check == usa0);

            // Restore: replace check value with original bytes from usa[i]
            *check = usa[i];
        } else {
            break;  // Beyond buffer bounds
        }
    }

    return result;  // false if any sector failed verification
}
```

### Handling Fixup Failures

If `unfixup()` returns `false`, the record is corrupted (torn write detected):

```cpp
if (frsh->MultiSectorHeader.Magic == 'ELIF') {
    if (frsh->MultiSectorHeader.unfixup(mft_record_size)) {
        // Record is valid, proceed with parsing
    } else {
        // Record is corrupted - mark as 'BAAD'
        frsh->MultiSectorHeader.Magic = 'DAAB';  // 'BAAD' in little-endian
        // Skip this record
    }
}
```

---

## Attribute Parsing

### Attribute Record Header

Every attribute in a FILE record starts with this header:

```
Offset  Size  Field              Description
──────  ────  ─────              ───────────
0x00    4     Type               Attribute type code (0x10, 0x30, 0x80, etc.)
0x04    4     Length             Total length of this attribute
0x08    1     IsNonResident      0 = resident, 1 = non-resident
0x09    1     NameLength         Attribute name length in characters
0x0A    2     NameOffset         Offset to attribute name
0x0C    2     Flags              0x0001=Compressed, 0x4000=Encrypted, 0x8000=Sparse
0x0E    2     Instance           Unique ID within this FILE record

--- If Resident (IsNonResident == 0) ---
0x10    4     ValueLength        Length of attribute value
0x14    2     ValueOffset        Offset to attribute value
0x16    2     Flags              0x0001 = Indexed

--- If Non-Resident (IsNonResident == 1) ---
0x10    8     LowestVCN          Starting Virtual Cluster Number
0x18    8     HighestVCN         Ending Virtual Cluster Number
0x20    2     MappingPairsOffset Offset to data run list
0x22    1     CompressionUnit    Log2 of compression unit clusters
0x23    5     Reserved
0x28    8     AllocatedSize      Space allocated on disk
0x30    8     DataSize           Logical file size
0x38    8     InitializedSize    Valid data length
0x40    8     CompressedSize     (Only if compressed)
```

### C++ Structure

```cpp
enum AttributeTypeCode {
    AttributeStandardInformation = 0x10,
    AttributeAttributeList       = 0x20,
    AttributeFileName            = 0x30,
    AttributeObjectId            = 0x40,
    AttributeSecurityDescriptor  = 0x50,
    AttributeVolumeName          = 0x60,
    AttributeVolumeInformation   = 0x70,
    AttributeData                = 0x80,
    AttributeIndexRoot           = 0x90,
    AttributeIndexAllocation     = 0xA0,
    AttributeBitmap              = 0xB0,
    AttributeReparsePoint        = 0xC0,
    AttributeEAInformation       = 0xD0,
    AttributeEA                  = 0xE0,
    AttributePropertySet         = 0xF0,
    AttributeLoggedUtilityStream = 0x100,
    AttributeEnd                 = -1,  // 0xFFFFFFFF - end marker
};

struct ATTRIBUTE_RECORD_HEADER {
    AttributeTypeCode Type;
    unsigned long Length;
    unsigned char IsNonResident;
    unsigned char NameLength;
    unsigned short NameOffset;
    unsigned short Flags;
    unsigned short Instance;

    union {
        struct RESIDENT {
            unsigned long ValueLength;
            unsigned short ValueOffset;
            unsigned short Flags;

            void* GetValue() {
                return reinterpret_cast<char*>(
                    CONTAINING_RECORD(this, ATTRIBUTE_RECORD_HEADER, Resident)
                ) + this->ValueOffset;
            }
        } Resident;

        struct NONRESIDENT {
            long long LowestVCN;
            long long HighestVCN;
            unsigned short MappingPairsOffset;
            unsigned char CompressionUnit;
            unsigned char Reserved[5];
            long long AllocatedSize;
            long long DataSize;
            long long InitializedSize;
            long long CompressedSize;
        } NonResident;
    };

    // Navigate to next attribute
    ATTRIBUTE_RECORD_HEADER* next() {
        return reinterpret_cast<ATTRIBUTE_RECORD_HEADER*>(
            reinterpret_cast<unsigned char*>(this) + this->Length);
    }

    // Get attribute name (if NameLength > 0)
    wchar_t* name() {
        return reinterpret_cast<wchar_t*>(
            reinterpret_cast<unsigned char*>(this) + this->NameOffset);
    }
};
```

### Attribute Iteration Loop

```cpp
void parse_attributes(FILE_RECORD_SEGMENT_HEADER* frsh, size_t mft_record_size) {
    void const* const frsh_end = frsh->end(mft_record_size);

    for (ATTRIBUTE_RECORD_HEADER const* ah = frsh->begin();
         ah < frsh_end &&
         ah->Type != AttributeTypeCode() &&  // Type 0 is invalid
         ah->Type != AttributeEnd;           // 0xFFFFFFFF marks end
         ah = ah->next())
    {
        // Sanity check: prevent infinite loop
        if (ah->Length == 0) break;

        switch (ah->Type) {
            case AttributeStandardInformation:
                parse_standard_information(ah);
                break;
            case AttributeFileName:
                parse_filename(ah);
                break;
            case AttributeData:
            case AttributeIndexRoot:
            case AttributeIndexAllocation:
            case AttributeBitmap:
                parse_stream(ah);
                break;
            // ... other attributes
        }
    }
}
```

---

## $STANDARD_INFORMATION (0x10)

### Structure

```cpp
struct STANDARD_INFORMATION {
    long long CreationTime;           // 0x00: File creation time
    long long LastModificationTime;   // 0x08: Last content modification
    long long LastChangeTime;         // 0x10: Last MFT record change
    long long LastAccessTime;         // 0x18: Last access time
    unsigned long FileAttributes;     // 0x20: FILE_ATTRIBUTE_* flags
    // NTFS 3.0+ additional fields:
    // unsigned long MaxVersions;
    // unsigned long VersionNumber;
    // unsigned long ClassId;
    // unsigned long OwnerId;
    // unsigned long SecurityId;
    // unsigned long long QuotaCharged;
    // unsigned long long USN;
};
```

### Timestamp Format

NTFS timestamps are 64-bit values representing 100-nanosecond intervals since January 1, 1601 UTC. This is the Windows FILETIME format.

```cpp
// Convert NTFS timestamp to FILETIME
FILETIME NtfsTimeToFileTime(long long ntfs_time) {
    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(ntfs_time);
    ft.dwHighDateTime = static_cast<DWORD>(ntfs_time >> 32);
    return ft;
}
```

### File Attributes

```cpp
// Standard Windows file attributes
#define FILE_ATTRIBUTE_READONLY            0x00000001
#define FILE_ATTRIBUTE_HIDDEN              0x00000002
#define FILE_ATTRIBUTE_SYSTEM              0x00000004
#define FILE_ATTRIBUTE_DIRECTORY           0x00000010
#define FILE_ATTRIBUTE_ARCHIVE             0x00000020
#define FILE_ATTRIBUTE_DEVICE              0x00000040
#define FILE_ATTRIBUTE_NORMAL              0x00000080
#define FILE_ATTRIBUTE_TEMPORARY           0x00000100
#define FILE_ATTRIBUTE_SPARSE_FILE         0x00000200
#define FILE_ATTRIBUTE_REPARSE_POINT       0x00000400
#define FILE_ATTRIBUTE_COMPRESSED          0x00000800
#define FILE_ATTRIBUTE_OFFLINE             0x00001000
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 0x00002000
#define FILE_ATTRIBUTE_ENCRYPTED           0x00004000
#define FILE_ATTRIBUTE_INTEGRITY_STREAM    0x00008000
#define FILE_ATTRIBUTE_NO_SCRUB_DATA       0x00020000
```

### Parsing $STANDARD_INFORMATION

```cpp
void parse_standard_information(ATTRIBUTE_RECORD_HEADER const* ah,
                                 Record* record) {
    // $STANDARD_INFORMATION is always resident
    STANDARD_INFORMATION const* si =
        static_cast<STANDARD_INFORMATION const*>(ah->Resident.GetValue());

    // Store timestamps
    record->stdinfo.created = si->CreationTime;
    record->stdinfo.written = si->LastModificationTime;
    record->stdinfo.accessed = si->LastAccessTime;

    // Store file attributes
    // Note: FRH_DIRECTORY flag from record header is ORed in
    record->stdinfo.attributes(si->FileAttributes);
}
```

---

## $FILE_NAME (0x30)

### Structure

```cpp
struct FILENAME_INFORMATION {
    unsigned long long ParentDirectory;  // 0x00: Parent FRS (48 bits) + sequence (16 bits)
    long long CreationTime;              // 0x08
    long long LastModificationTime;      // 0x10
    long long LastChangeTime;            // 0x18
    long long LastAccessTime;            // 0x20
    long long AllocatedLength;           // 0x28: Allocated size
    long long FileSize;                  // 0x30: Logical size
    unsigned long FileAttributes;        // 0x38: DOS attributes
    unsigned short PackedEaSize;         // 0x3C: Extended attributes size
    unsigned short Reserved;             // 0x3E
    unsigned char FileNameLength;        // 0x40: Filename length in characters
    unsigned char Flags;                 // 0x41: Namespace flags
    wchar_t FileName[1];                 // 0x42: Variable-length filename (Unicode)
};

// Filename namespace flags
enum {
    FILE_NAME_POSIX        = 0x00,  // Case-sensitive, any Unicode chars
    FILE_NAME_WIN32        = 0x01,  // Windows long filename
    FILE_NAME_DOS          = 0x02,  // 8.3 short filename only
    FILE_NAME_WIN32_AND_DOS = 0x03, // Both namespaces in one attribute
};
```

### Parent Directory Reference

The `ParentDirectory` field is a 64-bit value containing:
- **Bits 0-47**: File Record Segment (FRS) number of parent directory
- **Bits 48-63**: Sequence number (for detecting stale references)

```cpp
unsigned long long parent_frs = fn->ParentDirectory & 0x0000FFFFFFFFFFFF;
unsigned short parent_seq = (fn->ParentDirectory >> 48) & 0xFFFF;
```

### Multiple $FILE_NAME Attributes

A file typically has **multiple** $FILE_NAME attributes:

1. **WIN32 name**: The long filename (e.g., "My Document.txt")
2. **DOS name**: The 8.3 short name (e.g., "MYDOCU~1.TXT")
3. **WIN32_AND_DOS**: When long name is already 8.3 compliant

**UFFS skips DOS-only names** to avoid duplicate entries:

```cpp
if (fn->Flags == FILE_NAME_DOS) {
    return;  // Skip - we'll get the long name from another attribute
}
```

### Hard Links

Files with multiple hard links have multiple $FILE_NAME attributes with different parent directories. UFFS handles this by maintaining a linked list of `LinkInfo` structures:

```cpp
case AttributeFileName:
    if (fn->Flags != 0x02 /* FILE_NAME_DOS */) {
        // If this record already has a name, push current to linked list
        if (LinkInfo* existing = this->nameinfo(&*base_record)) {
            size_t link_index = this->nameinfos.size();
            this->nameinfos.push_back(base_record->first_name);
            base_record->first_name.next_entry = link_index;
        }

        // Store new name in first_name
        LinkInfo* info = &base_record->first_name;
        info->name.offset(this->names.size());
        info->name.length = fn->FileNameLength;
        info->name.ascii(is_ascii(fn->FileName, fn->FileNameLength));
        info->parent = static_cast<unsigned int>(fn->ParentDirectory);

        // Append filename to names buffer
        append_directional(this->names, fn->FileName, fn->FileNameLength,
                          info->name.ascii() ? 1 : 0);

        // Update parent's child list
        if (frs_parent != frs_base) {
            Records::iterator parent = this->at(frs_parent);
            ChildInfo* child = &this->childinfos.emplace_back();
            child->record_number = frs_base;
            child->name_index = base_record->name_count;
            child->next_entry = parent->first_child;
            parent->first_child = this->childinfos.size() - 1;
        }

        base_record->name_count++;
    }
    break;
```

---

## $DATA (0x80) and Stream Handling

### Overview

The $DATA attribute contains the actual file content. Files can have:
- **Unnamed $DATA**: The default data stream (what you see in Explorer)
- **Named $DATA**: Alternate Data Streams (ADS), e.g., `file.txt:Zone.Identifier`

### Resident vs Non-Resident Data

**Resident Data** (small files, typically < 700 bytes):
- Data is stored directly in the MFT record
- `IsNonResident == 0`
- Size from `Resident.ValueLength`

**Non-Resident Data** (larger files):
- Data is stored in clusters outside the MFT
- `IsNonResident == 1`
- Size from `NonResident.DataSize`
- Location described by data runs (mapping pairs)

### Parsing $DATA Attributes

```cpp
case AttributeData:
case AttributeIndexRoot:
case AttributeIndexAllocation:
case AttributeBitmap:
{
    // Get stream name (empty for default data stream)
    wchar_t const* stream_name = ah->name();
    size_t stream_name_len = ah->NameLength;

    // Get stream size
    long long stream_size;
    if (ah->IsNonResident) {
        stream_size = ah->NonResident.DataSize;
    } else {
        stream_size = ah->Resident.ValueLength;
    }

    // Skip zero-size streams (except for directories)
    if (stream_size == 0 && ah->Type == AttributeData) {
        return;
    }

    // Create StreamInfo entry
    StreamInfo* stream = &this->streaminfos.emplace_back();
    stream->type_name_id = ah->Type;
    stream->length = stream_size;
    stream->allocated = ah->IsNonResident ?
        ah->NonResident.AllocatedSize : stream_size;

    // Store stream name if present
    if (stream_name_len > 0) {
        stream->name.offset(this->names.size());
        stream->name.length = stream_name_len;
        stream->name.ascii(is_ascii(stream_name, stream_name_len));
        append_directional(this->names, stream_name, stream_name_len,
                          stream->name.ascii() ? 1 : 0);
    }

    // Link to record's stream list
    stream->next_entry = base_record->first_stream;
    base_record->first_stream = this->streaminfos.size() - 1;
    base_record->stream_count++;
}
break;
```

### Alternate Data Streams (ADS)

ADS are additional named $DATA attributes. Common examples:
- `Zone.Identifier`: Marks files downloaded from the internet
- `SummaryInformation`: Office document metadata
- Custom application data

```
File: C:\Downloads\setup.exe
├── $DATA (unnamed) - 15,234,567 bytes - The actual executable
└── $DATA:Zone.Identifier - 26 bytes - "[ZoneTransfer]\r\nZoneId=3"
```

UFFS indexes all streams, allowing searches like:
```
:Zone.Identifier    // Find all files with Zone.Identifier stream
```

---

## Directory Indexing ($INDEX_ROOT, $INDEX_ALLOCATION)

### Directory Structure

Directories in NTFS are implemented as B+ trees:
- **$INDEX_ROOT (0x90)**: Root node of the B+ tree (always resident)
- **$INDEX_ALLOCATION (0xA0)**: Additional B+ tree nodes (non-resident)
- **$BITMAP (0xB0)**: Tracks which index allocation blocks are in use

### Why UFFS Doesn't Parse Index Trees

UFFS builds its own parent-child relationships from $FILE_NAME attributes rather than parsing directory indexes because:

1. **Redundancy**: Every file's $FILE_NAME contains its parent reference
2. **Simplicity**: No need to parse complex B+ tree structures
3. **Completeness**: Catches orphaned files that might not be in any index

```cpp
// Parent-child relationship is built from $FILE_NAME parsing:
if (frs_parent != frs_base) {
    Records::iterator parent = this->at(frs_parent);
    ChildInfo* child = &this->childinfos.emplace_back();
    child->record_number = frs_base;
    child->name_index = base_record->name_count;
    child->next_entry = parent->first_child;
    parent->first_child = this->childinfos.size() - 1;
}
```

---

## Extension Records and $ATTRIBUTE_LIST

### When Extension Records Are Needed

A single 1024-byte MFT record can only hold so much data. Files with many attributes need extension records:
- Files with many hard links (each link = one $FILE_NAME)
- Files with many alternate data streams
- Highly fragmented files (long data run lists)

### $ATTRIBUTE_LIST (0x20)

When a file spans multiple MFT records, the base record contains an $ATTRIBUTE_LIST that maps attributes to their locations:

```
$ATTRIBUTE_LIST Entry:
┌─────────────────────────────────────────────────────────────────────────┐
│ Offset  Size  Field                                                     │
├─────────────────────────────────────────────────────────────────────────┤
│ 0x00    4     AttributeType        Type of attribute                    │
│ 0x04    2     RecordLength         Length of this entry                 │
│ 0x06    1     NameLength           Attribute name length                │
│ 0x07    1     NameOffset           Offset to attribute name             │
│ 0x08    8     LowestVCN            Starting VCN (for non-resident)      │
│ 0x10    8     SegmentReference     FRS containing this attribute        │
│ 0x18    2     Instance             Attribute instance number            │
│ 0x1A    var   AttributeName        Unicode name (if NameLength > 0)     │
└─────────────────────────────────────────────────────────────────────────┘
```

### UFFS Extension Record Handling

```cpp
// When parsing, check if this is an extension record
unsigned long long base_frs = frsh->BaseFileRecordSegment & 0x0000FFFFFFFFFFFF;

if (base_frs != 0) {
    // This is an extension record - attributes belong to base record
    frs_base = base_frs;
    base_record = &*this->at(frs_base);
} else {
    // This is a base record
    frs_base = frs;
    base_record = &*this->at(frs);
}
```

---

## In-Memory Index Structures

### Record Structure

```cpp
struct Record {
    // Standard information (timestamps, attributes)
    struct {
        long long created;
        long long written;
        long long accessed;
        unsigned int _attributes;

        void attributes(unsigned int value) { _attributes = value; }
        unsigned int attributes() const { return _attributes; }
    } stdinfo;

    // First filename (most files have only one)
    LinkInfo first_name;

    // Index of first child (for directories)
    unsigned int first_child;

    // Index of first stream
    unsigned int first_stream;

    // Counts
    unsigned short name_count;    // Number of hard links
    unsigned short stream_count;  // Number of data streams
};
```

### LinkInfo Structure

```cpp
struct LinkInfo {
    // Filename stored in names buffer
    struct {
        unsigned int _offset;
        unsigned short length;

        void offset(size_t v) { _offset = static_cast<unsigned int>(v); }
        size_t offset() const { return _offset; }

        void ascii(bool v) { /* stored in high bit of _offset */ }
        bool ascii() const { /* extracted from high bit */ }
    } name;

    // Parent directory FRS number
    unsigned int parent;

    // Index of next LinkInfo (for hard links)
    unsigned int next_entry;
};
```

### ChildInfo Structure

```cpp
struct ChildInfo {
    unsigned int record_number;  // FRS of child file/directory
    unsigned int name_index;     // Which name (for hard links)
    unsigned int next_entry;     // Next child in linked list
};
```

### StreamInfo Structure

```cpp
struct StreamInfo {
    // Stream name (empty for default $DATA)
    struct {
        unsigned int _offset;
        unsigned short length;
        // ascii flag in high bit
    } name;

    unsigned int type_name_id;   // Attribute type (0x80 for $DATA)
    long long length;            // Logical size
    long long allocated;         // Allocated size
    unsigned int next_entry;     // Next stream in linked list
};
```

### Names Buffer

All filenames and stream names are stored in a single contiguous buffer:

```cpp
std::vector<unsigned char> names;

// Appending a name:
void append_directional(std::vector<unsigned char>& names,
                        wchar_t const* str, size_t len, int direction) {
    if (direction == 1) {
        // ASCII: store as single bytes
        for (size_t i = 0; i < len; i++) {
            names.push_back(static_cast<unsigned char>(str[i]));
        }
    } else {
        // Unicode: store as wchar_t
        unsigned char const* bytes = reinterpret_cast<unsigned char const*>(str);
        names.insert(names.end(), bytes, bytes + len * sizeof(wchar_t));
    }
}
```

---

## Complete Parsing Flow

### Main Parsing Loop

```cpp
void NtfsIndex::parse_mft(unsigned char const* mft_data,
                          size_t mft_size,
                          size_t mft_record_size) {
    size_t num_records = mft_size / mft_record_size;

    // Pre-allocate records
    this->records.resize(num_records);

    for (size_t frs = 0; frs < num_records; frs++) {
        unsigned char* record_data = mft_data + frs * mft_record_size;
        FILE_RECORD_SEGMENT_HEADER* frsh =
            reinterpret_cast<FILE_RECORD_SEGMENT_HEADER*>(record_data);

        // Step 1: Check magic number
        if (frsh->MultiSectorHeader.Magic != 'ELIF') {
            continue;  // Not a valid FILE record
        }

        // Step 2: Apply USA fixup
        if (!frsh->MultiSectorHeader.unfixup(mft_record_size)) {
            frsh->MultiSectorHeader.Magic = 'DAAB';  // Mark as bad
            continue;
        }

        // Step 3: Check if record is in use
        if (!(frsh->Flags & FRH_IN_USE)) {
            continue;  // Deleted record
        }

        // Step 4: Determine base record
        unsigned long long base_frs =
            frsh->BaseFileRecordSegment & 0x0000FFFFFFFFFFFF;
        Record* base_record;

        if (base_frs != 0) {
            // Extension record - attributes go to base
            base_record = &this->records[base_frs];
        } else {
            // Base record
            base_record = &this->records[frs];
            base_record->stdinfo.attributes(
                (frsh->Flags & FRH_DIRECTORY) ? FILE_ATTRIBUTE_DIRECTORY : 0);
        }

        // Step 5: Parse all attributes
        void const* frsh_end = frsh->end(mft_record_size);

        for (ATTRIBUTE_RECORD_HEADER const* ah = frsh->begin();
             ah < frsh_end &&
             ah->Type != AttributeTypeCode() &&
             ah->Type != AttributeEnd;
             ah = ah->next())
        {
            if (ah->Length == 0) break;  // Prevent infinite loop

            switch (ah->Type) {
                case AttributeStandardInformation:
                    parse_standard_information(ah, base_record);
                    break;

                case AttributeFileName:
                    parse_filename(ah, frs, base_record);
                    break;

                case AttributeData:
                case AttributeIndexRoot:
                case AttributeIndexAllocation:
                case AttributeBitmap:
                    parse_stream(ah, base_record);
                    break;
            }
        }
    }
}
```

---

## Edge Cases and Error Handling

### Corrupted Records

```cpp
// Magic number check
if (frsh->MultiSectorHeader.Magic != 'ELIF') {
    // Could be:
    // - 'DAAB' (BAAD): Previously detected corruption
    // - 0x00000000: Uninitialized/zeroed record
    // - Other: Completely corrupted
    continue;
}

// USA fixup failure
if (!frsh->MultiSectorHeader.unfixup(mft_record_size)) {
    // Torn write detected - sector boundaries don't match
    frsh->MultiSectorHeader.Magic = 'DAAB';
    continue;
}
```

### Orphaned Records

Files whose parent directory no longer exists:

```cpp
// When building path, check if parent exists
if (parent_frs >= records.size() ||
    !(records[parent_frs].stdinfo.attributes() & FILE_ATTRIBUTE_DIRECTORY)) {
    // Orphaned file - parent doesn't exist or isn't a directory
    // UFFS still indexes these, they just won't have a valid path
}
```

### Circular References

Prevent infinite loops when building paths:

```cpp
std::wstring build_path(unsigned int frs, std::set<unsigned int>& visited) {
    if (visited.count(frs)) {
        return L"<circular>";  // Circular reference detected
    }
    visited.insert(frs);

    // ... build path recursively ...
}
```

### Very Long Paths

NTFS supports paths up to 32,767 characters. UFFS handles this by:
1. Storing paths as linked parent references, not full strings
2. Building full paths on-demand during search result display

---

## Performance Optimizations

### Memory Layout

```cpp
// Records are stored contiguously, indexed by FRS number
std::vector<Record> records;

// This allows O(1) lookup by FRS:
Record& get_record(unsigned int frs) {
    return records[frs];
}
```

### ASCII Optimization

Most filenames are ASCII-only. UFFS stores these as single bytes:

```cpp
if (is_ascii(filename, length)) {
    // Store as 1 byte per character
    info->name.ascii(true);
    for (size_t i = 0; i < length; i++) {
        names.push_back(static_cast<unsigned char>(filename[i]));
    }
} else {
    // Store as 2 bytes per character (UTF-16)
    info->name.ascii(false);
    names.insert(names.end(),
                 reinterpret_cast<unsigned char const*>(filename),
                 reinterpret_cast<unsigned char const*>(filename + length));
}
```

This reduces memory usage by ~50% for typical file systems.

### Linked List vs Vector

Child and stream lists use linked lists instead of vectors:
- **Pro**: No memory allocation per file
- **Pro**: Easy to add items during parsing
- **Con**: O(n) traversal, but n is typically small (1-3 items)

---

## Summary

The MFT parsing process:

1. **Read raw MFT data** from disk using direct volume access
2. **Validate each record** by checking magic number and applying USA fixup
3. **Skip deleted records** by checking FRH_IN_USE flag
4. **Handle extension records** by redirecting attributes to base record
5. **Parse attributes** to extract:
   - $STANDARD_INFORMATION: timestamps, file attributes
   - $FILE_NAME: filename, parent reference (skip DOS-only names)
   - $DATA and others: stream information
6. **Build index structures**:
   - Records: one per file/directory
   - LinkInfos: for hard links
   - StreamInfos: for alternate data streams
   - ChildInfos: for directory contents
   - Names buffer: all filenames and stream names

The result is a compact, searchable index that can be queried in milliseconds.

