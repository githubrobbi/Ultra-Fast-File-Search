# Indexing Engine

## Introduction

This document provides exhaustive detail on how Ultra Fast File Search builds and maintains its in-memory file index. After reading this document, you should be able to:

1. Understand the NtfsIndex class architecture and data structures
2. Implement the compact memory layout for millions of files
3. Build parent-child relationships from MFT records
4. Resolve full paths from parent references efficiently
5. Traverse the index for pattern matching

---

## Overview: Index Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         NtfsIndex Architecture                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                        NtfsIndex                                 │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │ Volume Information                                       │    │   │
│  │  │   _root_path: "C:\"                                      │    │   │
│  │  │   _volume: Handle to \\.\C:                              │    │   │
│  │  │   cluster_size, mft_record_size, mft_capacity            │    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  │                                                                  │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │ Core Data Structures                                     │    │   │
│  │  │   names: std::tvstring (all filenames concatenated)      │    │   │
│  │  │   records_data: vector<Record> (file metadata)           │    │   │
│  │  │   records_lookup: vector<uint> (FRS → record index)      │    │   │
│  │  │   nameinfos: vector<LinkInfo> (hard link chain)          │    │   │
│  │  │   streaminfos: vector<StreamInfo> (ADS chain)            │    │   │
│  │  │   childinfos: vector<ChildInfo> (directory contents)     │    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  │                                                                  │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │ Progress Tracking                                        │    │   │
│  │  │   _records_so_far: atomic<uint>                          │    │   │
│  │  │   _preprocessed_so_far: atomic<uint>                     │    │   │
│  │  │   _finished_event: Handle                                │    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Core Data Structures

### Record Structure

Each file or directory in the index is represented by a `Record`:

```cpp
struct Record {
    StandardInfo stdinfo;                    // Timestamps and attributes
    unsigned short name_count;               // Number of hard links (≤1024)
    unsigned short stream_count;             // Number of data streams (≤4106)
    ChildInfo::next_entry_type first_child;  // Index of first child (directories)
    LinkInfo first_name;                     // First/primary filename
    StreamInfo first_stream;                 // First/primary data stream

    Record() : stdinfo(), name_count(), stream_count(),
               first_child(negative_one), first_name(), first_stream() {
        this->first_stream.name.offset(negative_one);
        this->first_stream.next_entry = negative_one;
    }
};
```

### StandardInfo: Timestamps and Attributes

```cpp
struct StandardInfo {
    long long created;    // Creation time (FILETIME)
    long long written;    // Last write time (FILETIME)
    long long accessed;   // Last access time (FILETIME)

    // Bit-packed attributes for memory efficiency
    unsigned int is_readonly        : 1;
    unsigned int is_archive         : 1;
    unsigned int is_system          : 1;
    unsigned int is_hidden          : 1;
    unsigned int is_offline         : 1;
    unsigned int is_notcontentidx   : 1;
    unsigned int is_noscrubdata     : 1;
    unsigned int is_integretystream : 1;
    unsigned int is_pinned          : 1;
    unsigned int is_unpinned        : 1;
    unsigned int is_directory       : 1;
    unsigned int is_compressed      : 1;
    unsigned int is_encrypted       : 1;
    unsigned int is_sparsefile      : 1;
    unsigned int is_reparsepoint    : 1;

    // Reconstruct Windows FILE_ATTRIBUTE_* flags
    unsigned long attributes() const {
        return (this->is_readonly    ? FILE_ATTRIBUTE_READONLY    : 0U) |
               (this->is_archive     ? FILE_ATTRIBUTE_ARCHIVE     : 0U) |
               (this->is_system      ? FILE_ATTRIBUTE_SYSTEM      : 0U) |
               (this->is_hidden      ? FILE_ATTRIBUTE_HIDDEN      : 0U) |
               (this->is_directory   ? FILE_ATTRIBUTE_DIRECTORY   : 0U) |
               (this->is_compressed  ? FILE_ATTRIBUTE_COMPRESSED  : 0U) |
               (this->is_encrypted   ? FILE_ATTRIBUTE_ENCRYPTED   : 0U) |
               (this->is_sparsefile  ? FILE_ATTRIBUTE_SPARSE_FILE : 0U) |
               (this->is_reparsepoint? FILE_ATTRIBUTE_REPARSE_POINT:0U) |
               // ... other attributes
               0U;
    }

    void attributes(unsigned long value) {
        this->is_readonly     = !!(value & FILE_ATTRIBUTE_READONLY);
        this->is_archive      = !!(value & FILE_ATTRIBUTE_ARCHIVE);
        this->is_system       = !!(value & FILE_ATTRIBUTE_SYSTEM);
        this->is_hidden       = !!(value & FILE_ATTRIBUTE_HIDDEN);
        this->is_directory    = !!(value & FILE_ATTRIBUTE_DIRECTORY);
        this->is_compressed   = !!(value & FILE_ATTRIBUTE_COMPRESSED);
        this->is_encrypted    = !!(value & FILE_ATTRIBUTE_ENCRYPTED);
        this->is_sparsefile   = !!(value & FILE_ATTRIBUTE_SPARSE_FILE);
        this->is_reparsepoint = !!(value & FILE_ATTRIBUTE_REPARSE_POINT);
        // ... other attributes
    }
};
```



### LinkInfo: Hard Link Information

```cpp
struct LinkInfo {
    typedef small_t<size_t>::type next_entry_type;

    next_entry_type next_entry;  // Index of next LinkInfo (for multiple hard links)
    NameInfo name;               // Filename reference
    unsigned int parent;         // Parent directory FRS number

    LinkInfo() : next_entry(negative_one) {
        this->name.offset(negative_one);
    }
};
```

**Hard Link Chain**: Most files have only one name, stored directly in `Record::first_name`. Files with multiple hard links form a linked list through `next_entry`, with additional entries stored in `nameinfos`.

### StreamInfo: Data Stream Information

```cpp
struct StreamInfo : SizeInfo {
    typedef small_t<size_t>::type next_entry_type;

    next_entry_type next_entry;  // Index of next StreamInfo
    NameInfo name;               // Stream name (empty for default $DATA)

    // Bit-packed flags
    unsigned char is_sparse : 1;
    unsigned char is_allocated_size_accounted_for_in_main_stream : 1;
    unsigned char type_name_id : 6;  // Attribute type identifier

    StreamInfo() : SizeInfo(), next_entry(), name(), type_name_id() {}
};

struct SizeInfo {
    file_size_type length;     // Logical file size
    file_size_type allocated;  // Allocated size on disk
    file_size_type bulkiness;  // Size including slack space
    unsigned int treesize;     // For directories: descendant count
};
```

### ChildInfo: Directory Contents

```cpp
struct ChildInfo {
    typedef small_t<size_t>::type next_entry_type;

    next_entry_type next_entry;                    // Next child in linked list
    small_t<Records::size_type>::type record_number;  // FRS of child
    unsigned short name_index;                     // Which name (for hard links)

    ChildInfo() : next_entry(negative_one),
                  record_number(negative_one),
                  name_index(negative_one) {}
};
```

---

## Memory Layout

### The Names Buffer

All filenames are stored in a single contiguous buffer:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           names buffer                                   │
├─────────────────────────────────────────────────────────────────────────┤
│ offset 0                                                                │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ "WINDOWS" (ASCII, 7 bytes)                                          │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│ offset 7                                                                │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ "System32" (ASCII, 8 bytes)                                         │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│ offset 15                                                               │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ "日本語.txt" (Unicode, 9 chars × 2 bytes = 18 bytes)                │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│ ...                                                                     │
└─────────────────────────────────────────────────────────────────────────┘
```

**Benefits**:
- Single allocation for all names
- Cache-friendly sequential access
- No per-string allocation overhead

### Records Lookup

The `records_lookup` vector provides O(1) access from FRS to record:

```
FRS:           0     1     2     3     4     5     6     7     8    ...
             ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
records_lookup: │ -1  │ -1  │ -1  │ -1  │ -1  │  0  │ -1  │ -1  │  1  │ ...
             └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
                                        │                    │
                                        ▼                    ▼
                                   records_data[0]     records_data[1]
                                   (root dir)          (file at FRS 8)
```

**Note**: FRS 0-4 are reserved system files, FRS 5 is the root directory.

```cpp
Records::iterator at(size_t frs, Records::iterator* existing = NULL) {
    // Expand lookup table if needed
    if (frs >= this->records_lookup.size()) {
        this->records_lookup.resize(frs + 1, ~RecordsLookup::value_type());
    }

    RecordsLookup::iterator k = this->records_lookup.begin() + frs;
    if (!~*k) {  // Entry doesn't exist yet
        // Save position of existing iterator (may be invalidated by resize)
        ptrdiff_t j = (existing ? *existing : this->records_data.end())
                      - this->records_data.begin();

        // Add new record
        *k = static_cast<unsigned int>(this->records_data.size());
        this->records_data.resize(this->records_data.size() + 1);

        // Restore existing iterator
        if (existing) {
            *existing = this->records_data.begin() + j;
        }
    }

    return this->records_data.begin() + *k;
}

Records::value_type const* find(key_type::frs_type frs) const {
    if (frs < this->records_lookup.size()) {
        RecordsLookup::value_type islot = this->records_lookup[frs];
        return fast_subscript(this->records_data.begin(), islot);
    }
    return this->records_data.empty() ? NULL : &*(this->records_data.end() - 1) + 1;
}
```

---

## Building the Index

### Processing MFT Records

When an MFT record is parsed, the index is updated:

```cpp
void process_mft_record(unsigned int frs, FILE_RECORD_SEGMENT_HEADER* frsh) {
    // Skip deleted records
    if (!(frsh->Flags & FRH_IN_USE)) return;

    // Handle extension records (redirect to base record)
    unsigned int frs_base = frs;
    if (frsh->BaseFileRecordSegment) {
        frs_base = static_cast<unsigned int>(frsh->BaseFileRecordSegment);
    }

    // Get or create record
    Records::iterator base_record = this->at(frs_base);

    // Parse attributes
    for (ATTRIBUTE_RECORD_HEADER* ah = first_attribute(frsh);
         ah && ah->TypeCode != AttributeEnd;
         ah = next_attribute(ah)) {

        switch (ah->TypeCode) {
            case AttributeStandardInformation:
                process_standard_info(base_record, ah);
                break;

            case AttributeFileName:
                process_filename(base_record, frs_base, ah);
                break;

            case AttributeData:
            case AttributeIndexRoot:
            case AttributeIndexAllocation:
                process_stream(base_record, ah);
                break;
        }
    }
}
```

### Processing $FILE_NAME Attributes

```cpp
void process_filename(Records::iterator& base_record, unsigned int frs_base,
                      ATTRIBUTE_RECORD_HEADER* ah) {
    FILENAME_INFORMATION* fn = ah->Resident.GetValue();

    // Skip DOS-only names (8.3 short names)
    if (fn->Flags == FILE_NAME_DOS) return;

    unsigned int frs_parent = static_cast<unsigned int>(fn->ParentDirectory);

    // If record already has a name, push current to linked list
    if (LinkInfo* existing = this->nameinfo(&*base_record)) {
        size_t link_index = this->nameinfos.size();
        this->nameinfos.push_back(base_record->first_name);
        base_record->first_name.next_entry = link_index;
    }

    // Store new name in first_name
    LinkInfo* info = &base_record->first_name;
    info->name.offset(this->names.size());
    info->name.length = fn->FileNameLength;

    // Check if name is pure ASCII (optimization)
    bool ascii = is_ascii(fn->FileName, fn->FileNameLength);
    info->name.ascii(ascii);
    info->parent = frs_parent;

    // Append filename to names buffer
    append_directional(this->names, fn->FileName, fn->FileNameLength,
                       ascii ? 1 : 0);

    // Build parent-child relationship
    if (frs_parent != frs_base) {
        Records::iterator parent = this->at(frs_parent, &base_record);

        size_t child_index = this->childinfos.size();
        this->childinfos.push_back(ChildInfo());

        ChildInfo* child_info = &this->childinfos.back();
        child_info->record_number = frs_base;
        child_info->name_index = base_record->name_count;
        child_info->next_entry = parent->first_child;

        parent->first_child = child_index;
    }

    ++base_record->name_count;
}
```

---

## Path Resolution

### The ParentIterator Class

UFFS builds full paths on-demand by traversing parent references:

```cpp
class ParentIterator {
    struct value_type_internal {
        void const* first;           // Pointer to name data
        size_t second : sizeof(size_t) * CHAR_BIT - 1;  // Name length
        size_t ascii : 1;            // Is ASCII?
    };

    NtfsIndex const* index;
    key_type key;
    unsigned char state;
    unsigned short iteration;
    file_pointers ptrs;
    value_type_internal result;

    bool is_root() const {
        return key.frs() == 0x000000000005;  // Root directory FRS
    }

    bool is_attribute() const {
        return ptrs.stream->type_name_id &&
               (ptrs.stream->type_name_id << (CHAR_BIT / 2)) != AttributeData;
    }

public:
    ParentIterator(NtfsIndex const* index, key_type key)
        : index(index), key(key), state(0), iteration(0) {}

    bool next();  // Advance to next path component

    value_type_internal const* operator->() const { return &result; }

    unsigned short icomponent() const { return iteration; }

    unsigned int attributes() const {
        return index->find(key.frs())->stdinfo.attributes();
    }
};
```

### Building Full Paths

```cpp
size_t get_path(key_type key, std::tvstring& result,
                bool name_only, unsigned int* attributes = NULL) const {
    size_t old_size = result.size();

    // Iterate through parent chain
    for (ParentIterator pi(this, key);
         pi.next() && !(name_only && pi.icomponent()); ) {

        // Capture attributes from first component
        if (attributes) {
            *attributes = pi.attributes();
            attributes = NULL;
        }

        TCHAR const* s = static_cast<TCHAR const*>(pi->first);

        // Skip single-dot components (current directory)
        if (name_only || !(pi->second == 1 &&
            (pi->ascii ? *static_cast<char const*>(static_cast<void const*>(s))
                       : *s) == _T('.'))) {
            append_directional(result, s, pi->second, pi->ascii ? -1 : 0, true);
        }
    }

    // Path was built in reverse order, so reverse it
    std::reverse(result.begin() + old_size, result.end());

    return result.size() - old_size;
}
```

### Path Building Example

```
File: C:\Windows\System32\notepad.exe (FRS 12345)

ParentIterator traversal:
  1. FRS 12345 → "notepad.exe" (parent: FRS 100)
  2. FRS 100   → "System32"    (parent: FRS 50)
  3. FRS 50    → "Windows"     (parent: FRS 5)
  4. FRS 5     → "."           (root directory)

Result buffer (before reverse): "exe.dapeton\23metsyS\swodniW\:C"
Result buffer (after reverse):  "C:\Windows\System32\notepad.exe"
```

---

## Index Traversal

### The Matcher Template

UFFS uses a depth-first traversal to find matching files:

```cpp
template<class F>
void matches(F func, std::tvstring& path,
             bool match_paths, bool match_streams, bool match_attributes) const {
    Matcher<F&> matcher = {
        this, func, match_paths, match_streams, match_attributes, &path, 0
    };

    // Start from root directory (FRS 5)
    return matcher(0x000000000005);
}

template<class F>
struct Matcher {
    NtfsIndex const* me;
    F func;
    bool match_paths;
    bool match_streams;
    bool match_attributes;
    std::tvstring* path;
    size_t basename_index_in_path;
    NameInfo name;
    size_t depth;

    void operator()(key_type::frs_type frs) {
        if (frs >= me->records_lookup.size()) return;

        TCHAR const dirsep = getdirsep();
        std::tvstring temp;

        Records::value_type const* i = me->find(frs);
        unsigned short ji = 0;

        // Iterate through all names (hard links)
        for (LinkInfo const* j = me->nameinfo(i); j;
             j = me->nameinfo(j->next_entry), ++ji) {

            size_t old_basename_index = basename_index_in_path;
            basename_index_in_path = path->size();

            temp.clear();
            append_directional(temp, &dirsep, 1, 0);

            if (!(match_paths && frs == 0x00000005)) {
                append_directional(temp, &me->names[j->name.offset()],
                                   j->name.length, j->name.ascii() ? -1 : 0);
            }

            this->operator()(frs, ji, temp.data(), temp.size());
            basename_index_in_path = old_basename_index;
        }
    }

    void operator()(key_type::frs_type frs, key_type::name_info_type name_info,
                    TCHAR const stream_prefix[], size_t stream_prefix_size) {
        // Process streams and recurse into children
        // ... (see full implementation)
    }
};
```

---

## Key Type

### Identifying Index Entries

Each entry in the index is uniquely identified by a `key_type`:

```cpp
struct key_type_internal {
    typedef unsigned int frs_type;
    typedef unsigned short name_info_type;
    typedef unsigned short stream_info_type;

private:
    frs_type _frs;
    name_info_type _name_info;
    stream_info_type _stream_info;

public:
    frs_type frs() const { return _frs; }
    void frs(frs_type v) { _frs = v; }

    name_info_type name_info() const { return _name_info; }
    void name_info(name_info_type v) { _name_info = v; }

    stream_info_type stream_info() const { return _stream_info; }
    void stream_info(stream_info_type v) { _stream_info = v; }

    key_type_internal(frs_type frs = 0, name_info_type name = 0,
                      stream_info_type stream = 0)
        : _frs(frs), _name_info(name), _stream_info(stream) {}
};
```

**Components**:
- `frs`: File Record Segment number (unique per file)
- `name_info`: Which hard link name (0 for primary)
- `stream_info`: Which data stream (0 for default $DATA)

---

## Memory Efficiency

### Size Analysis

For a typical volume with 1 million files:

| Structure | Per-Entry Size | Total (1M files) |
|-----------|---------------|------------------|
| Record | ~48 bytes | 48 MB |
| records_lookup | 4 bytes | 4 MB |
| LinkInfo (avg 1.1 per file) | ~12 bytes | 13 MB |
| StreamInfo (avg 1.2 per file) | ~24 bytes | 29 MB |
| ChildInfo (avg 1 per file) | ~10 bytes | 10 MB |
| Names buffer (avg 20 chars) | ~20 bytes | 20 MB |
| **Total** | | **~124 MB** |

### Optimization Techniques

1. **Bit-packing**: Attributes stored as bit fields
2. **ASCII optimization**: Single-byte storage for ASCII names
3. **Inline first entries**: First name/stream stored in Record, not separate
4. **Sentinel values**: `~0` used instead of separate "valid" flags
5. **Contiguous storage**: All vectors, no per-entry allocations

---

## Thread Safety

### Locking Strategy

```cpp
class NtfsIndex : public RefCounted<NtfsIndex> {
    atomic_namespace::recursive_mutex _mutex;

    // ... data members ...

public:
    // Lock for read/write access
    lock_ptr<NtfsIndex> lock() {
        return lock_ptr<NtfsIndex>(this, this->_mutex);
    }

    lock_ptr<NtfsIndex const> lock() const {
        return lock_ptr<NtfsIndex const>(this, this->_mutex);
    }
};

// Usage
void search(NtfsIndex volatile* index) {
    auto locked = lock(index);  // Acquires mutex
    locked->matches(...);       // Safe access
}  // Mutex released
```

### Progress Tracking

```cpp
// Atomic counters for progress reporting
atomic<unsigned int> _records_so_far;
atomic<unsigned int> _preprocessed_so_far;

// Finished event for synchronization
Handle _finished_event;

size_t records_so_far() const volatile {
    return this->_records_so_far.load(atomic_namespace::memory_order_acquire);
}

void set_finished() {
    SetEvent(this->_finished_event);
}
```

---

## Summary

The UFFS indexing engine achieves high performance through:

1. **Compact data structures**: Bit-packing, inline storage, ASCII optimization
2. **O(1) FRS lookup**: Direct indexing via records_lookup
3. **On-demand path building**: Paths constructed only when needed
4. **Depth-first traversal**: Efficient pattern matching
5. **Thread-safe access**: Recursive mutex with lock_ptr

Key implementation patterns:

| Pattern | Purpose |
|---------|---------|
| `records_lookup` | O(1) FRS to record mapping |
| `names` buffer | Single allocation for all filenames |
| `LinkInfo` chain | Hard link support |
| `ChildInfo` chain | Directory contents |
| `ParentIterator` | On-demand path resolution |
| `Matcher` template | Depth-first search traversal |

---

## See Also

- [01-architecture-overview.md](01-architecture-overview.md) - System architecture
- [02-ntfs-mft-fundamentals.md](02-ntfs-mft-fundamentals.md) - NTFS structure
- [04-mft-parsing-internals.md](04-mft-parsing-internals.md) - MFT record parsing
- [05-pattern-matching-engine.md](05-pattern-matching-engine.md) - Pattern matching
- [08-search-algorithm.md](08-search-algorithm.md) - Search implementation
