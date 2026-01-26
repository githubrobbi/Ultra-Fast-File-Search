# UFFS-MFT Field Enhancements

> **Scope**: `uffs_mft` crate - MftIndex structure, CSV/Parquet exports
> **Status**: Planning
> **Created**: 2026-01-26

## Overview

This document tracks enhancements to the raw MFT fields exported by `uffs_mft`. These fields are stored in the `MftIndex` structure and exported via CSV/Parquet formats.

**Design Principle**: `uffs_mft` is a forensic/power-user tool that should be truthful to the raw MFT structure. We export what's actually in the MFT record, plus essential derived fields (path).

---

## Current Export (31 columns)

| Category | Fields |
|----------|--------|
| **Identity** | frs, sequence_number, parent_frs, name, namespace, path |
| **Size** | size, allocated_size |
| **Timestamps ($SI)** | si_created, si_modified, si_accessed, si_mft_changed |
| **Timestamps ($FN)** | fn_created, fn_modified, fn_accessed, fn_mft_changed |
| **Flags** | is_directory, is_readonly, is_hidden, is_system, is_archive, is_compressed, is_encrypted, is_sparse, is_reparse, is_offline, is_not_indexed, is_temporary, flags |
| **Counts** | link_count, stream_count |

---

## Impact Summary

### Memory Impact per Record

| Field | Size | Impact on 1M files | Impact on 10M files |
|-------|------|-------------------|---------------------|
| **usn** | 8 bytes | +8 MB | +80 MB |
| **security_id** | 4 bytes | +4 MB | +40 MB |
| **owner_id** | 4 bytes | +4 MB | +40 MB |
| **lsn** | 8 bytes | +8 MB | +80 MB |
| **reparse_tag** | 4 bytes | +4 MB | +40 MB |
| **is_resident** | 1 byte (packed) | +1 MB | +10 MB |
| **is_deleted** | 1 byte (packed) | +1 MB | +10 MB |
| **is_corrupt** | 1 byte (packed) | +1 MB | +10 MB |
| **is_extension** | 1 byte (packed) | +1 MB | +10 MB |
| **base_frs** | 8 bytes | +8 MB | +80 MB |

**Current FileRecord size**: 176 bytes
**After P1**: 200 bytes (+24 bytes, +14%)
**After P1+P2**: 205 bytes (+29 bytes, +16%)
**After All**: 222 bytes (+46 bytes, +26%)

### Speed Impact

| Field | Parse Impact | Export Impact | Notes |
|-------|--------------|---------------|-------|
| **usn** | Negligible | Negligible | Already reading $STD_INFO, just extract 8 more bytes |
| **security_id** | Negligible | Negligible | Already reading $STD_INFO, just extract 4 more bytes |
| **owner_id** | Negligible | Negligible | Already reading $STD_INFO, just extract 4 more bytes |
| **lsn** | Negligible | Negligible | Already reading header, just extract 8 more bytes |
| **reparse_tag** | ~1-2% | Negligible | New attribute to parse, but only ~1% of files have reparse points |
| **is_resident** | Negligible | Negligible | Already checking $DATA header, just store the flag |
| **is_deleted** | +10-50% records | +10-50% export | Deleted records currently skipped; including them increases record count |
| **is_corrupt** | Negligible | Negligible | Already detecting corruption, just don't skip |
| **is_extension** | Negligible | Negligible | Already detecting extensions, just store flag |
| **base_frs** | Negligible | Negligible | Already have this value during parsing |

### Recommendation Matrix

| Field | Memory | Speed | Value | Recommend |
|-------|--------|-------|-------|-----------|
| **usn** | Low (+8B) | None | High (forensic) | ✅ Yes |
| **security_id** | Low (+4B) | None | High (ACL analysis) | ✅ Yes |
| **owner_id** | Low (+4B) | None | Medium (quota) | ✅ Yes |
| **lsn** | Low (+8B) | None | High (forensic) | ✅ Yes |
| **reparse_tag** | Low (+4B) | Low (~1%) | High (symlinks) | ✅ Yes |
| **is_resident** | Minimal (+1b) | None | Medium (perf analysis) | ✅ Yes |
| **is_deleted** | High (+10-50% records) | High (+10-50%) | Very High (forensic) | ⚠️ Optional flag |
| **is_corrupt** | Minimal | None | Medium (forensic) | ✅ Yes |
| **is_extension** | Minimal (+1b) | None | Low (forensic) | ⚠️ Optional |
| **base_frs** | Low (+8B) | None | Low (forensic) | ⚠️ Only if is_extension |

---

## Proposed Enhancements

### Priority 1: Easy Wins (from existing $STANDARD_INFORMATION parsing)

| Field | Type | Source | Target Audience | Usefulness |
|-------|------|--------|-----------------|------------|
| **usn** | u64 | $STD_INFO @0x40 | Forensic | Correlates with USN journal ($UsnJrnl) for timeline analysis. Critical for incident response - shows exact order of file operations. |
| **security_id** | u32 | $STD_INFO @0x34 | Forensic, Power | Index into $Secure file. Enables ACL analysis, permission auditing, and identifying files with unusual security descriptors. |
| **owner_id** | u32 | $STD_INFO @0x30 | Forensic | Quota owner ID. Useful for multi-user systems to identify file ownership patterns. |
| **lsn** | u64 | Header @0x08 | Forensic | Log File Sequence Number. Correlates with $LogFile journal for transaction analysis and recovery. |

**Implementation Difficulty**: 🟢 **Easy**
- These fields already exist in structures we parse
- $STANDARD_INFORMATION: Add 3 fields to parsing (usn, security_id, owner_id)
- Header: LSN is at fixed offset 0x08, already accessible
- No architectural changes required

---

### Priority 2: Medium Effort, High Value

| Field | Type | Source | Target Audience | Usefulness |
|-------|------|--------|-----------------|------------|
| **reparse_tag** | u32 | $REPARSE_POINT | All | Identifies reparse point type: symlinks (0xA000000C), junctions (0xA0000003), OneDrive (0x9000001A), dedup (0x80000013). Essential for understanding file system structure. |
| **is_resident** | bool | $DATA header | Power | Whether file data is stored directly in MFT record (small files <700 bytes). Useful for performance analysis and understanding MFT bloat. |

**Implementation Difficulty**: 🟡 **Medium**

**Why Medium for reparse_tag**:
- Need to parse new attribute type: $REPARSE_POINT (0xC0)
- Attribute can be resident or non-resident
- Need to extract ReparseTag (first 4 bytes of attribute value)
- Must handle case where attribute doesn't exist (most files)

**Why Medium for is_resident**:
- Need to check `IsNonResident` flag in $DATA attribute header
- Must track this per-file during parsing
- Need to handle files with no $DATA attribute (directories)
- Need to handle multiple $DATA attributes (ADS)

---

### Priority 3: Architecture Changes Required

| Field | Type | Source | Target Audience | Usefulness |
|-------|------|--------|-----------------|------------|
| **is_deleted** | bool | Header flags | Forensic | Include deleted files (FRH_IN_USE = 0). Forensic gold - enables deleted file recovery and timeline analysis. |
| **is_corrupt** | bool | Magic='BAAD' | Forensic | Include corrupted records (USA fixup failed). Identifies disk corruption or tampering. |
| **is_extension** | bool | BaseFileRecordSegment | Forensic | Flag for extension records. Helps understand MFT structure for large files. |
| **base_frs** | u64 | BaseFileRecordSegment | Forensic | For extension records, which base record they belong to. |

**Implementation Difficulty**: 🔴 **Hard**

**Why Hard for is_deleted/is_corrupt**:
- Currently we skip these records entirely in `parse_record_full()`
- Would need to change return type to include "deleted" and "corrupt" variants
- Need to decide: separate rows? or mark existing rows?
- Deleted records may have incomplete data (no $FILE_NAME, etc.)
- Memory impact: could significantly increase record count
- Path resolution: deleted files may have orphaned parents

**Why Hard for is_extension/base_frs**:
- Currently extension records are merged into base records transparently
- Would need to track which FRS contributed which attributes
- Changes the fundamental "one row per file" model
- May confuse end users who expect one row per file

---

## Reparse Tag Reference

Common reparse tag values for reference:

| Tag | Hex Value | Description |
|-----|-----------|-------------|
| IO_REPARSE_TAG_MOUNT_POINT | 0xA0000003 | Junction point (directory symlink) |
| IO_REPARSE_TAG_SYMLINK | 0xA000000C | Symbolic link (file or directory) |
| IO_REPARSE_TAG_CLOUD | 0x9000001A | OneDrive placeholder |
| IO_REPARSE_TAG_CLOUD_1 | 0x9000101A | OneDrive (variant) |
| IO_REPARSE_TAG_DEDUP | 0x80000013 | Data deduplication |
| IO_REPARSE_TAG_WCI | 0x80000018 | Windows Container Isolation |
| IO_REPARSE_TAG_WOF | 0x80000017 | Windows Overlay Filter (compressed) |
| IO_REPARSE_TAG_HSM | 0xC0000004 | Hierarchical Storage Management |
| IO_REPARSE_TAG_HSM2 | 0x80000006 | HSM variant |
| IO_REPARSE_TAG_APPEXECLINK | 0x8000001B | UWP app execution link |

---

## Implementation Tracking

### Priority 1: Easy Wins

| Field | Status | PR/Commit | Notes |
|-------|--------|-----------|-------|
| usn | ⬜ Not Started | | |
| security_id | ⬜ Not Started | | |
| owner_id | ⬜ Not Started | | |
| lsn | ⬜ Not Started | | |

### Priority 2: Medium Effort

| Field | Status | PR/Commit | Notes |
|-------|--------|-----------|-------|
| reparse_tag | ⬜ Not Started | | |
| is_resident | ⬜ Not Started | | |

### Priority 3: Architecture Changes

| Field | Status | PR/Commit | Notes |
|-------|--------|-----------|-------|
| is_deleted | ⬜ Not Started | | Requires design discussion |
| is_corrupt | ⬜ Not Started | | Requires design discussion |
| is_extension | ⬜ Not Started | | Requires design discussion |
| base_frs | ⬜ Not Started | | Requires design discussion |

---

## Status Legend

| Symbol | Meaning |
|--------|---------|
| ⬜ | Not Started |
| 🔄 | In Progress |
| ✅ | Complete |
| ❌ | Cancelled |
| 🔒 | Blocked |

---

## Version History

| Date | Version | Changes |
|------|---------|---------|
| 2026-01-26 | 1.0 | Initial document |

