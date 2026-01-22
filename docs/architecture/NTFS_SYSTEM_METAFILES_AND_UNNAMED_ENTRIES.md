# NTFS System Metafiles and Unnamed Entry Handling

This document explains how Ultra-Fast-File-Search (UFFS) handles NTFS system metafiles and unnamed MFT entries during indexing and search operations.

---

## NTFS Reserved File Record Segments (FRS 0-15)

NTFS reserves the first 16 File Record Segments for system metafiles. These are internal structures that manage the filesystem itself:

| FRS | Name | Purpose |
|-----|------|---------|
| 0 | `$MFT` | Master File Table - index of all files |
| 1 | `$MFTMirr` | Mirror of first 4 MFT records for recovery |
| 2 | `$LogFile` | NTFS transaction journal |
| 3 | `$Volume` | Volume label, version, flags |
| 4 | `$AttrDef` | Attribute type definitions |
| 5 | `.` | **Root directory** (special - user-visible) |
| 6 | `$Bitmap` | Cluster allocation bitmap |
| 7 | `$Boot` | Boot sector and bootstrap code |
| 8 | `$BadClus` | Bad cluster tracking |
| 9 | `$Secure` | Security descriptor storage |
| 10 | `$UpCase` | Unicode uppercase mapping table |
| 11 | `$Extend` | Container for extended metadata |
| 12-15 | Reserved | Reserved for future NTFS versions |

### The `$Extend` Directory (FRS 11)

Contains additional system files:
- `$ObjId` - Object ID index
- `$Quota` - Disk quota data
- `$Reparse` - Reparse point index
- `$UsnJrnl` - Change journal
- `$RmMetadata` - Resource Manager metadata

---

## How UFFS Filters System Metafiles

### Primary Filter Logic

The core filtering happens during tree traversal in `UltraFastFileSearch.cpp`:

```cpp
// Line 5290: Main filter for which FRS records to process
if (frs < me->records_lookup.size() && 
    (frs == 0x00000005 || frs >= 0x00000010 || this->match_attributes))
{
    // Process this record...
}
```

**Translation:**
- `frs == 0x00000005` → Always include root directory (FRS 5)
- `frs >= 0x00000010` → Always include user files (FRS ≥ 16)
- `this->match_attributes` → Include system files only if attribute matching enabled

### Filtering Matrix

| FRS Range | Default Behavior | With `match_attributes` |
|-----------|------------------|-------------------------|
| 0-4 | **EXCLUDED** | Included |
| 5 (root) | **INCLUDED** | Included |
| 6-15 | **EXCLUDED** | Included |
| ≥ 16 | **INCLUDED** | Included |

### Why FRS 5 is Special

FRS 5 is the root directory (`.`) - the starting point for the entire directory tree. It must always be included because:

1. It's the parent of all top-level files and folders
2. The tree traversal algorithm starts from FRS 5
3. It's user-visible (unlike `$MFT`, `$Bitmap`, etc.)

```cpp
// Line 5269: Root directory name handling
if (!(match_paths && frs == 0x00000005))
{
    append_directional(temp, &me->names[j->name.offset()], j->name.length, ...);
}
```

The root directory's name (`.`) is suppressed in path display - you see `C:\folder` not `C:\.\folder`.

---

## Unnamed Entry Handling

### 1. Filename Namespace Filtering (DOS Names)

NTFS supports multiple filename namespaces per file:

| Value | Namespace | Description |
|-------|-----------|-------------|
| 0x00 | POSIX | Case-sensitive, any characters |
| 0x01 | Win32 | Standard Windows name |
| 0x02 | DOS | 8.3 short name only |
| 0x03 | Win32+DOS | Combined (name fits 8.3 format) |

**UFFS skips DOS-only names** to avoid duplicates:

```cpp
// Line 4468: Skip DOS-only names
if (fn->Flags != 0x02 /*FILE_NAME_DOS */)
{
    // Store this filename...
    LinkInfo* const info = &base_record->first_name;
    info->name.offset(static_cast<unsigned int>(this->names.size()));
    info->name.length = static_cast<unsigned char>(fn->FileNameLength);
    // ...
}
```

**Example:** A file might have:
- Win32 name: `This is a long filename.txt`
- DOS name: `THISIS~1.TXT`

UFFS keeps only the Win32 name, avoiding duplicate search results.

### 2. Records Without Filenames

Some MFT records have no `$FILE_NAME` attribute:
- Deleted files (marked not-in-use)
- Extension records (attributes overflow)
- Corrupted records

**Detection via `nameinfo()` function:**

```cpp
// Returns NULL if record has no valid filename
LinkInfos::value_type const* nameinfo(Records::value_type const* const i) const
{
    return ~i->first_name.name.offset() ? &i->first_name : NULL;
}
```

The `~i->first_name.name.offset()` check tests if offset is not `0xFFFFFFFF` (uninitialized).

**Traversal automatically skips unnamed records:**

```cpp
// Line 5261-5276: Iterate only records with names
for (LinkInfos::value_type const* j = me->nameinfo(fr); j; j = me->nameinfo(j->next_entry), ++ji)
{
    // j is NULL if no filename exists - loop doesn't execute
}
```

### 3. Data Stream Naming

Files can have multiple data streams. The primary content has no name:

```cpp
// Line 4761: Identify default (unnamed) stream
bool const is_default_stream = is_data_attribute && !k->name.length;
```

| Stream Type | Display Format | Example |
|-------------|----------------|---------|
| Default (unnamed) | `filename.ext` | `document.pdf` |
| Named stream | `filename.ext:streamname` | `file.txt:Zone.Identifier` |
| With attribute | `filename.ext:stream:$TYPE` | `file.txt::$DATA` |

**Stream name display logic:**

```cpp
// Line 5324-5337: Append stream name if present
if (k->name.length)
{
    path->push_back(_T(':'));
    append_directional(*path, &me->names[k->name.offset()], k->name.length, ...);
}
```

---

## Extension Records and Base Record Resolution

Large files may have attributes spanning multiple MFT records. UFFS handles this via base record resolution:

```cpp
// Line 4371-4372: Determine base record for extension records
unsigned int const frs_base = frsh->BaseFileRecordSegment
    ? static_cast<unsigned int>(frsh->BaseFileRecordSegment)
    : frs;
```

- If `BaseFileRecordSegment` is non-zero, this is an extension record
- All attributes are merged into the base record
- Extension records themselves have no filename - they're invisible to users

---

## Hard Links (Multiple Names)

A single file can have multiple names (hard links). UFFS stores all of them:

```cpp
// Line 4470-4476: Handle multiple names per record
if (LinkInfos::value_type* const si = this->nameinfo(&*base_record))
{
    // Already has a name - push current to linked list
    size_t const link_index = this->nameinfos.size();
    this->nameinfos.push_back(base_record->first_name);
    base_record->first_name.next_entry = static_cast<...>(link_index);
}

// Store new name in first_name slot
LinkInfo* const info = &base_record->first_name;
info->name.offset(static_cast<unsigned int>(this->names.size()));
info->name.length = static_cast<unsigned char>(fn->FileNameLength);
```

**Data Structure:**
```
Record
  └── first_name (LinkInfo)
        ├── name: "hardlink1.txt"
        ├── parent: FRS of parent directory
        └── next_entry → nameinfos[N]
                           ├── name: "hardlink2.txt"
                           ├── parent: FRS of different parent
                           └── next_entry → ...
```

---

## Deleted Records

Deleted files are filtered by the `FRH_IN_USE` flag:

```cpp
// Line 4369: Only process in-use records
if (frsh->MultiSectorHeader.Magic == 'ELIF' && !!(frsh->Flags & ntfs::FRH_IN_USE))
{
    // Process this record...
}
```

- `Magic == 'ELIF'` → Valid FILE record signature
- `Flags & FRH_IN_USE` → Record is active (not deleted)

Deleted records are completely skipped during indexing.

---

## Summary Table

| Entry Type | Indexed? | Displayed? | Notes |
|------------|----------|------------|-------|
| System metafiles (FRS 0-4, 6-15) | Yes | No* | *Unless `match_attributes` enabled |
| Root directory (FRS 5) | Yes | Yes | Name suppressed in paths |
| User files (FRS ≥ 16) | Yes | Yes | Normal files and folders |
| DOS-only names (8.3) | No | No | Skipped to avoid duplicates |
| Extension records | Merged | No | Attributes merged to base record |
| Deleted records | No | No | Filtered by `FRH_IN_USE` flag |
| Hard links | Yes | Yes | All names stored and searchable |
| Default data stream | Yes | Yes | No `:name` suffix |
| Named data streams | Yes | Yes | Shown as `file:streamname` |

---

## Practical Implications

### For Users
- System files like `$MFT` won't clutter search results
- 8.3 short names won't cause duplicate matches
- Hard-linked files appear under all their names
- Deleted files are not shown

### For Developers
- FRS 0-15 filtering is intentional, not a bug
- The `match_attributes` flag enables system file visibility
- Extension records are transparent - no special handling needed
- The `nameinfo()` NULL check is the safety net for unnamed records

---

*Document generated from UFFS source code analysis - January 2026*

