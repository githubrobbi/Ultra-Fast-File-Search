# NTFS MFT Record Format - Complete Reference

> **Author**: NTFS/MFT Specialist Reference  
> **Last Updated**: 2026-01-26  
> **Scope**: Complete byte-level documentation of NTFS Master File Table records

---

## Table of Contents

1. [Overview](#overview)
2. [Record Layout](#record-layout)
3. [FILE_RECORD_SEGMENT_HEADER](#file_record_segment_header)
4. [Update Sequence Array (USA)](#update-sequence-array-usa)
5. [Attribute Record Header](#attribute-record-header)
6. [Attribute Types](#attribute-types)
7. [$STANDARD_INFORMATION (0x10)](#standard_information-0x10)
8. [$FILE_NAME (0x30)](#file_name-0x30)
9. [$DATA (0x80)](#data-0x80)
10. [Record Termination](#record-termination)
11. [Extension Records](#extension-records)
12. [$ATTRIBUTE_LIST (0x20)](#attribute_list-0x20)
13. [Complete Parsing Algorithm](#complete-parsing-algorithm)
14. [Summary](#summary)
15. [References](#references)

---

## Overview

### What is an MFT Record?

The Master File Table (MFT) is the heart of NTFS. Every file and directory on an NTFS volume has at least one MFT record (also called a File Record Segment or FRS).

### Key Characteristics

| Property | Value | Notes |
|----------|-------|-------|
| **Record Size** | 1024 bytes (typical) | Can be 512, 1024, 2048, or 4096 bytes |
| **Fixed Size** | Yes | All records are the same size on a volume |
| **Delimiter** | None | Records are contiguous, no separator |
| **Terminator** | `0xFFFFFFFF` | AttributeEnd marker inside record |
| **Alignment** | Record size boundary | Records start at multiples of record size |

### How Records Are Located

```
MFT Layout (assuming 1024-byte records):
┌──────────────────────────────────────────────────────────────────────────────┐
│ Offset 0x000: FRS 0 ($MFT itself)                                            │
├──────────────────────────────────────────────────────────────────────────────┤
│ Offset 0x400: FRS 1 ($MFTMirr)                                               │
├──────────────────────────────────────────────────────────────────────────────┤
│ Offset 0x800: FRS 2 ($LogFile)                                               │
├──────────────────────────────────────────────────────────────────────────────┤
│ Offset 0xC00: FRS 3 ($Volume)                                                │
├──────────────────────────────────────────────────────────────────────────────┤
│ ...                                                                          │
├──────────────────────────────────────────────────────────────────────────────┤
│ Offset N*1024: FRS N (your file)                                             │
└──────────────────────────────────────────────────────────────────────────────┘

FRS Number = Byte Offset / Record Size
Byte Offset = FRS Number * Record Size
```

**There is NO delimiter or "newline" between records.** Records are fixed-size and contiguous.

---

## Record Layout

### Complete 1024-Byte Record Structure (Fixed Header: 0x000-0x02F)

The first 48 bytes (0x30) are the fixed FILE_RECORD_SEGMENT_HEADER. Every field is shown with its exact byte position.

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ FIXED HEADER: FILE_RECORD_SEGMENT_HEADER (48 bytes = 0x30)                                          │
├──────────┬──────────┬────────────────────────────────────┬──────────────────────────────────────────┤
│ Offset   │ Size     │ Field Name                         │ Description                              │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x000    │ 1 byte   │ Magic[0]                           │ 'F' (0x46)                               │
│ 0x001    │ 1 byte   │ Magic[1]                           │ 'I' (0x49)                               │
│ 0x002    │ 1 byte   │ Magic[2]                           │ 'L' (0x4C)                               │
│ 0x003    │ 1 byte   │ Magic[3]                           │ 'E' (0x45)                               │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x004    │ 1 byte   │ USAOffset (low byte)               │ Offset to USA, typically 0x30            │
│ 0x005    │ 1 byte   │ USAOffset (high byte)              │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x006    │ 1 byte   │ USACount (low byte)                │ Number of USA entries, typically 3       │
│ 0x007    │ 1 byte   │ USACount (high byte)               │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x008    │ 1 byte   │ LogFileSequenceNumber[0]           │ LSN byte 0 (lowest)                      │
│ 0x009    │ 1 byte   │ LogFileSequenceNumber[1]           │ LSN byte 1                               │
│ 0x00A    │ 1 byte   │ LogFileSequenceNumber[2]           │ LSN byte 2                               │
│ 0x00B    │ 1 byte   │ LogFileSequenceNumber[3]           │ LSN byte 3                               │
│ 0x00C    │ 1 byte   │ LogFileSequenceNumber[4]           │ LSN byte 4                               │
│ 0x00D    │ 1 byte   │ LogFileSequenceNumber[5]           │ LSN byte 5                               │
│ 0x00E    │ 1 byte   │ LogFileSequenceNumber[6]           │ LSN byte 6                               │
│ 0x00F    │ 1 byte   │ LogFileSequenceNumber[7]           │ LSN byte 7 (highest)                     │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x010    │ 1 byte   │ SequenceNumber (low byte)          │ Reuse counter, incremented on delete     │
│ 0x011    │ 1 byte   │ SequenceNumber (high byte)         │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x012    │ 1 byte   │ LinkCount (low byte)               │ Number of hard links                     │
│ 0x013    │ 1 byte   │ LinkCount (high byte)              │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x014    │ 1 byte   │ FirstAttributeOffset (low byte)    │ Offset to first attribute, typically 0x38│
│ 0x015    │ 1 byte   │ FirstAttributeOffset (high byte)   │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x016    │ 1 byte   │ Flags (low byte)                   │ Bit 0: IN_USE, Bit 1: DIRECTORY          │
│ 0x017    │ 1 byte   │ Flags (high byte)                  │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x018    │ 1 byte   │ BytesInUse[0]                      │ Actual bytes used, byte 0 (lowest)       │
│ 0x019    │ 1 byte   │ BytesInUse[1]                      │ Actual bytes used, byte 1                │
│ 0x01A    │ 1 byte   │ BytesInUse[2]                      │ Actual bytes used, byte 2                │
│ 0x01B    │ 1 byte   │ BytesInUse[3]                      │ Actual bytes used, byte 3 (highest)      │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x01C    │ 1 byte   │ BytesAllocated[0]                  │ Total record size, byte 0 (lowest)       │
│ 0x01D    │ 1 byte   │ BytesAllocated[1]                  │ Total record size, byte 1                │
│ 0x01E    │ 1 byte   │ BytesAllocated[2]                  │ Total record size, byte 2                │
│ 0x01F    │ 1 byte   │ BytesAllocated[3]                  │ Total record size, byte 3 (highest)      │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x020    │ 1 byte   │ BaseFileRecordSegment[0]           │ Base FRS byte 0 (lowest)                 │
│ 0x021    │ 1 byte   │ BaseFileRecordSegment[1]           │ Base FRS byte 1                          │
│ 0x022    │ 1 byte   │ BaseFileRecordSegment[2]           │ Base FRS byte 2                          │
│ 0x023    │ 1 byte   │ BaseFileRecordSegment[3]           │ Base FRS byte 3                          │
│ 0x024    │ 1 byte   │ BaseFileRecordSegment[4]           │ Base FRS byte 4                          │
│ 0x025    │ 1 byte   │ BaseFileRecordSegment[5]           │ Base FRS byte 5 (bits 40-47 of FRS)      │
│ 0x026    │ 1 byte   │ BaseFileRecordSegment[6]           │ Sequence number (low byte)               │
│ 0x027    │ 1 byte   │ BaseFileRecordSegment[7]           │ Sequence number (high byte)              │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x028    │ 1 byte   │ NextAttributeNumber (low byte)     │ Next instance ID to assign               │
│ 0x029    │ 1 byte   │ NextAttributeNumber (high byte)    │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x02A    │ 1 byte   │ SegmentNumberUpper (low byte)      │ Upper 16 bits of FRS (UNRELIABLE!)       │
│ 0x02B    │ 1 byte   │ SegmentNumberUpper (high byte)     │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x02C    │ 1 byte   │ SegmentNumberLower[0]              │ Lower 32 bits of FRS, byte 0 (lowest)    │
│ 0x02D    │ 1 byte   │ SegmentNumberLower[1]              │ Lower 32 bits of FRS, byte 1             │
│ 0x02E    │ 1 byte   │ SegmentNumberLower[2]              │ Lower 32 bits of FRS, byte 2             │
│ 0x02F    │ 1 byte   │ SegmentNumberLower[3]              │ Lower 32 bits of FRS, byte 3 (highest)   │
└──────────┴──────────┴────────────────────────────────────┴──────────────────────────────────────────┘
TOTAL FIXED HEADER: 48 bytes (0x30)
```

### Update Sequence Array (0x030-0x035 for typical 1024-byte record)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ UPDATE SEQUENCE ARRAY (6 bytes for 1024-byte record with USACount=3)                                │
├──────────┬──────────┬────────────────────────────────────┬──────────────────────────────────────────┤
│ Offset   │ Size     │ Field Name                         │ Description                              │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x030    │ 1 byte   │ CheckValue (low byte)              │ Value written to end of each sector      │
│ 0x031    │ 1 byte   │ CheckValue (high byte)             │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x032    │ 1 byte   │ OriginalSector1End (low byte)      │ Original bytes from offset 0x1FE         │
│ 0x033    │ 1 byte   │ OriginalSector1End (high byte)     │ (little-endian)                          │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x034    │ 1 byte   │ OriginalSector2End (low byte)      │ Original bytes from offset 0x3FE         │
│ 0x035    │ 1 byte   │ OriginalSector2End (high byte)     │ (little-endian)                          │
└──────────┴──────────┴────────────────────────────────────┴──────────────────────────────────────────┘
TOTAL USA: 6 bytes (USACount * 2)
```

### Padding/Alignment (0x036-0x037)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ALIGNMENT PADDING (2 bytes to align FirstAttributeOffset to 8-byte boundary)                        │
├──────────┬──────────┬────────────────────────────────────┬──────────────────────────────────────────┤
│ Offset   │ Size     │ Field Name                         │ Description                              │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x036    │ 1 byte   │ Padding[0]                         │ Alignment padding (usually 0x00)         │
│ 0x037    │ 1 byte   │ Padding[1]                         │ Alignment padding (usually 0x00)         │
└──────────┴──────────┴────────────────────────────────────┴──────────────────────────────────────────┘
TOTAL PADDING: 2 bytes
```

### First Attribute Starts at 0x038

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ATTRIBUTES REGION (0x038 to BytesInUse - 4)                                                         │
├──────────┬──────────┬────────────────────────────────────┬──────────────────────────────────────────┤
│ Offset   │ Size     │ Field Name                         │ Description                              │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x038    │ variable │ Attribute 1                        │ First attribute (usually $STD_INFO)      │
│ ...      │ variable │ Attribute 2                        │ Second attribute (usually $FILE_NAME)    │
│ ...      │ variable │ Attribute N                        │ Additional attributes                    │
│ ...      │ 4 bytes  │ AttributeEnd                       │ 0xFFFFFFFF end marker                    │
└──────────┴──────────┴────────────────────────────────────┴──────────────────────────────────────────┘
```

### Sector End Positions (Where USA Check Values Are Written)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ SECTOR END POSITIONS (overwritten with CheckValue on disk)                                          │
├──────────┬──────────┬────────────────────────────────────┬──────────────────────────────────────────┤
│ Offset   │ Size     │ Field Name                         │ Description                              │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x1FE    │ 1 byte   │ Sector1End (low byte)              │ Replaced with CheckValue on disk         │
│ 0x1FF    │ 1 byte   │ Sector1End (high byte)             │ Original stored in USA[1]                │
├──────────┼──────────┼────────────────────────────────────┼──────────────────────────────────────────┤
│ 0x3FE    │ 1 byte   │ Sector2End (low byte)              │ Replaced with CheckValue on disk         │
│ 0x3FF    │ 1 byte   │ Sector2End (high byte)             │ Original stored in USA[2]                │
└──────────┴──────────┴────────────────────────────────────┴──────────────────────────────────────────┘
```

### Complete 1024-Byte Record Summary

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ COMPLETE 1024-BYTE RECORD BYTE MAP                                                                  │
├──────────────────────┬──────────────────────────────────────────────────────────────────────────────┤
│ Byte Range           │ Content                                                                      │
├──────────────────────┼──────────────────────────────────────────────────────────────────────────────┤
│ 0x000 - 0x003        │ Magic: 'FILE' (4 bytes)                                                      │
│ 0x004 - 0x005        │ USAOffset: 0x0030 (2 bytes)                                                  │
│ 0x006 - 0x007        │ USACount: 0x0003 (2 bytes)                                                   │
│ 0x008 - 0x00F        │ LogFileSequenceNumber (8 bytes)                                              │
│ 0x010 - 0x011        │ SequenceNumber (2 bytes)                                                     │
│ 0x012 - 0x013        │ LinkCount (2 bytes)                                                          │
│ 0x014 - 0x015        │ FirstAttributeOffset: 0x0038 (2 bytes)                                       │
│ 0x016 - 0x017        │ Flags (2 bytes)                                                              │
│ 0x018 - 0x01B        │ BytesInUse (4 bytes)                                                         │
│ 0x01C - 0x01F        │ BytesAllocated: 0x00000400 = 1024 (4 bytes)                                  │
│ 0x020 - 0x027        │ BaseFileRecordSegment (8 bytes)                                              │
│ 0x028 - 0x029        │ NextAttributeNumber (2 bytes)                                                │
│ 0x02A - 0x02B        │ SegmentNumberUpper (2 bytes) - UNRELIABLE                                    │
│ 0x02C - 0x02F        │ SegmentNumberLower (4 bytes)                                                 │
│ 0x030 - 0x031        │ USA CheckValue (2 bytes)                                                     │
│ 0x032 - 0x033        │ USA OriginalSector1End (2 bytes)                                             │
│ 0x034 - 0x035        │ USA OriginalSector2End (2 bytes)                                             │
│ 0x036 - 0x037        │ Alignment Padding (2 bytes)                                                  │
│ 0x038 - (BytesInUse-4) │ Attributes (variable)                                                      │
│ (BytesInUse-4) - (BytesInUse-1) │ AttributeEnd: 0xFFFFFFFF (4 bytes)                               │
│ BytesInUse - 0x1FD   │ Slack Space (unused)                                                         │
│ 0x1FE - 0x1FF        │ Sector 1 End (CheckValue on disk, original after unfixup)                    │
│ 0x200 - 0x3FD        │ Slack Space continued (if attributes don't extend here)                      │
│ 0x3FE - 0x3FF        │ Sector 2 End (CheckValue on disk, original after unfixup)                    │
└──────────────────────┴──────────────────────────────────────────────────────────────────────────────┘
TOTAL: 1024 bytes (0x400)
```

### Visual Representation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        FILE RECORD (1024 bytes)                              │
├─────────────────────────────────────────────────────────────────────────────┤
│ 0x000 ┌─────────────────────────────────────────────────────────────────┐   │
│       │ MULTI_SECTOR_HEADER (8 bytes)                                   │   │
│       │ ┌─────────┬───────────┬───────────┐                             │   │
│       │ │ Magic   │ USAOffset │ USACount  │                             │   │
│       │ │ 4 bytes │ 2 bytes   │ 2 bytes   │                             │   │
│       │ └─────────┴───────────┴───────────┘                             │   │
│ 0x008 ├─────────────────────────────────────────────────────────────────┤   │
│       │ LogFileSequenceNumber (8 bytes)                                 │   │
│ 0x010 ├─────────────────────────────────────────────────────────────────┤   │
│       │ SequenceNumber (2) │ LinkCount (2) │ FirstAttrOffset (2)        │   │
│ 0x016 ├─────────────────────────────────────────────────────────────────┤   │
│       │ Flags (2) │ BytesInUse (4) │ BytesAllocated (4)                 │   │
│ 0x020 ├─────────────────────────────────────────────────────────────────┤   │
│       │ BaseFileRecordSegment (8 bytes)                                 │   │
│ 0x028 ├─────────────────────────────────────────────────────────────────┤   │
│       │ NextAttrNum (2) │ SegNumUpper (2) │ SegNumLower (4)             │   │
│ 0x030 ├─────────────────────────────────────────────────────────────────┤   │
│       │ UPDATE SEQUENCE ARRAY (USACount * 2 bytes)                      │   │
│       │ ┌──────────┬──────────┬──────────┐                              │   │
│       │ │ Check    │ Orig@1FE │ Orig@3FE │  (for 1024-byte record)      │   │
│       │ │ Value    │          │          │                              │   │
│       │ └──────────┴──────────┴──────────┘                              │   │
│ 0x038 ├─────────────────────────────────────────────────────────────────┤   │
│       │ ATTRIBUTE 1 ($STANDARD_INFORMATION)                             │   │
│       │ ┌─────────────────────────────────────────────────────────────┐ │   │
│       │ │ Type=0x10 │ Length │ ... │ Value (timestamps, attrs)        │ │   │
│       │ └─────────────────────────────────────────────────────────────┘ │   │
│       ├─────────────────────────────────────────────────────────────────┤   │
│       │ ATTRIBUTE 2 ($FILE_NAME)                                        │   │
│       │ ┌─────────────────────────────────────────────────────────────┐ │   │
│       │ │ Type=0x30 │ Length │ ... │ Value (parent, name, times)      │ │   │
│       │ └─────────────────────────────────────────────────────────────┘ │   │
│       ├─────────────────────────────────────────────────────────────────┤   │
│       │ ATTRIBUTE 3 ($DATA or others)                                   │   │
│       │ ┌─────────────────────────────────────────────────────────────┐ │   │
│       │ │ Type=0x80 │ Length │ ... │ Data runs or resident data       │ │   │
│       │ └─────────────────────────────────────────────────────────────┘ │   │
│       ├─────────────────────────────────────────────────────────────────┤   │
│       │ ATTRIBUTE END MARKER                                            │   │
│       │ ┌─────────────────────────────────────────────────────────────┐ │   │
│       │ │ 0xFFFFFFFF (4 bytes)                                        │ │   │
│       │ └─────────────────────────────────────────────────────────────┘ │   │
│       ├─────────────────────────────────────────────────────────────────┤   │
│       │ SLACK SPACE (unused, may contain old data)                      │   │
│ 0x3FF └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## FILE_RECORD_SEGMENT_HEADER

### Complete Byte-by-Byte Layout (48 bytes)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ FILE_RECORD_SEGMENT_HEADER - Complete Structure (48 bytes = 0x30)                                   │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ Magic[0] = 'F' (0x46)                              │
│ 0x01     │ 1 byte   │ unsigned char            │ Magic[1] = 'I' (0x49)                              │
│ 0x02     │ 1 byte   │ unsigned char            │ Magic[2] = 'L' (0x4C)                              │
│ 0x03     │ 1 byte   │ unsigned char            │ Magic[3] = 'E' (0x45)                              │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x04     │ 1 byte   │ unsigned char            │ USAOffset[0] (low byte) - typically 0x30          │
│ 0x05     │ 1 byte   │ unsigned char            │ USAOffset[1] (high byte) - typically 0x00         │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x06     │ 1 byte   │ unsigned char            │ USACount[0] (low byte) - typically 0x03           │
│ 0x07     │ 1 byte   │ unsigned char            │ USACount[1] (high byte) - typically 0x00          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ LogFileSequenceNumber[0] (byte 0, lowest)         │
│ 0x09     │ 1 byte   │ unsigned char            │ LogFileSequenceNumber[1] (byte 1)                 │
│ 0x0A     │ 1 byte   │ unsigned char            │ LogFileSequenceNumber[2] (byte 2)                 │
│ 0x0B     │ 1 byte   │ unsigned char            │ LogFileSequenceNumber[3] (byte 3)                 │
│ 0x0C     │ 1 byte   │ unsigned char            │ LogFileSequenceNumber[4] (byte 4)                 │
│ 0x0D     │ 1 byte   │ unsigned char            │ LogFileSequenceNumber[5] (byte 5)                 │
│ 0x0E     │ 1 byte   │ unsigned char            │ LogFileSequenceNumber[6] (byte 6)                 │
│ 0x0F     │ 1 byte   │ unsigned char            │ LogFileSequenceNumber[7] (byte 7, highest)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x10     │ 1 byte   │ unsigned char            │ SequenceNumber[0] (low byte)                      │
│ 0x11     │ 1 byte   │ unsigned char            │ SequenceNumber[1] (high byte)                     │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x12     │ 1 byte   │ unsigned char            │ LinkCount[0] (low byte)                           │
│ 0x13     │ 1 byte   │ unsigned char            │ LinkCount[1] (high byte)                          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x14     │ 1 byte   │ unsigned char            │ FirstAttributeOffset[0] (low) - typically 0x38    │
│ 0x15     │ 1 byte   │ unsigned char            │ FirstAttributeOffset[1] (high) - typically 0x00   │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x16     │ 1 byte   │ unsigned char            │ Flags[0] (low byte) - see Flags table below       │
│ 0x17     │ 1 byte   │ unsigned char            │ Flags[1] (high byte)                              │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x18     │ 1 byte   │ unsigned char            │ BytesInUse[0] (byte 0, lowest)                    │
│ 0x19     │ 1 byte   │ unsigned char            │ BytesInUse[1] (byte 1)                            │
│ 0x1A     │ 1 byte   │ unsigned char            │ BytesInUse[2] (byte 2)                            │
│ 0x1B     │ 1 byte   │ unsigned char            │ BytesInUse[3] (byte 3, highest)                   │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x1C     │ 1 byte   │ unsigned char            │ BytesAllocated[0] (byte 0) - typically 0x00       │
│ 0x1D     │ 1 byte   │ unsigned char            │ BytesAllocated[1] (byte 1) - typically 0x04       │
│ 0x1E     │ 1 byte   │ unsigned char            │ BytesAllocated[2] (byte 2) - typically 0x00       │
│ 0x1F     │ 1 byte   │ unsigned char            │ BytesAllocated[3] (byte 3) - typically 0x00       │
│          │          │                          │ (0x00000400 = 1024 in little-endian)              │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x20     │ 1 byte   │ unsigned char            │ BaseFileRecordSegment[0] (FRS byte 0, lowest)     │
│ 0x21     │ 1 byte   │ unsigned char            │ BaseFileRecordSegment[1] (FRS byte 1)             │
│ 0x22     │ 1 byte   │ unsigned char            │ BaseFileRecordSegment[2] (FRS byte 2)             │
│ 0x23     │ 1 byte   │ unsigned char            │ BaseFileRecordSegment[3] (FRS byte 3)             │
│ 0x24     │ 1 byte   │ unsigned char            │ BaseFileRecordSegment[4] (FRS byte 4)             │
│ 0x25     │ 1 byte   │ unsigned char            │ BaseFileRecordSegment[5] (FRS byte 5, bits 40-47) │
│ 0x26     │ 1 byte   │ unsigned char            │ BaseFileRecordSegment[6] (Seq# low byte)          │
│ 0x27     │ 1 byte   │ unsigned char            │ BaseFileRecordSegment[7] (Seq# high byte)         │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x28     │ 1 byte   │ unsigned char            │ NextAttributeNumber[0] (low byte)                 │
│ 0x29     │ 1 byte   │ unsigned char            │ NextAttributeNumber[1] (high byte)                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x2A     │ 1 byte   │ unsigned char            │ SegmentNumberUpper[0] (low) - UNRELIABLE!         │
│ 0x2B     │ 1 byte   │ unsigned char            │ SegmentNumberUpper[1] (high) - UNRELIABLE!        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x2C     │ 1 byte   │ unsigned char            │ SegmentNumberLower[0] (byte 0, lowest)            │
│ 0x2D     │ 1 byte   │ unsigned char            │ SegmentNumberLower[1] (byte 1)                    │
│ 0x2E     │ 1 byte   │ unsigned char            │ SegmentNumberLower[2] (byte 2)                    │
│ 0x2F     │ 1 byte   │ unsigned char            │ SegmentNumberLower[3] (byte 3, highest)           │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL: 48 bytes (0x30)
```

### Flags Field (Offset 0x16) - Complete Bit-by-Bit

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ FLAGS FIELD (2 bytes = 16 bits at offset 0x16)                                                      │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Bit      │ Mask     │ Name                     │ Description                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 0    │ 0x0001   │ FRH_IN_USE               │ 1 = Record in use (valid file/directory)           │
│          │          │                          │ 0 = Record is deleted/free                         │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 1    │ 0x0002   │ FRH_DIRECTORY            │ 1 = Record is a directory                          │
│          │          │                          │ 0 = Record is a file                               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 2    │ 0x0004   │ FRH_EXTENSION            │ 1 = Extension record (rare, not always set)        │
│          │          │                          │ 0 = Base record or extension without flag          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 3    │ 0x0008   │ FRH_SPECIAL_INDEX        │ 1 = Special index present (rare)                   │
│          │          │                          │ 0 = Normal record                                  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 4    │ 0x0010   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 5    │ 0x0020   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 6    │ 0x0040   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 7    │ 0x0080   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 8    │ 0x0100   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 9    │ 0x0200   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 10   │ 0x0400   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 11   │ 0x0800   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 12   │ 0x1000   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 13   │ 0x2000   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 14   │ 0x4000   │ Reserved                 │ Reserved for future use                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 15   │ 0x8000   │ Reserved                 │ Reserved for future use                            │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
```

**Critical**: If `!(Flags & FRH_IN_USE)`, the record is **deleted** and should be skipped.

### Magic Number Values - Complete Byte-by-Byte

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ MAGIC NUMBER (4 bytes at offset 0x00)                                                               │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Byte     │ Value    │ ASCII                    │ Description                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ═════════════════════════════════════ VALID RECORD ('FILE') ═══════════════════════════════════════ │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 0x46     │ 'F'                      │ Magic byte 0                                       │
│ 0x01     │ 0x49     │ 'I'                      │ Magic byte 1                                       │
│ 0x02     │ 0x4C     │ 'L'                      │ Magic byte 2                                       │
│ 0x03     │ 0x45     │ 'E'                      │ Magic byte 3                                       │
│          │          │                          │ As DWORD: 0x454C4946 (little-endian 'ELIF')        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ═════════════════════════════════════ CORRUPTED RECORD ('BAAD') ═══════════════════════════════════ │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 0x42     │ 'B'                      │ Magic byte 0                                       │
│ 0x01     │ 0x41     │ 'A'                      │ Magic byte 1                                       │
│ 0x02     │ 0x41     │ 'A'                      │ Magic byte 2                                       │
│ 0x03     │ 0x44     │ 'D'                      │ Magic byte 3                                       │
│          │          │                          │ As DWORD: 0x44414142 (USA fixup failed)            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ═════════════════════════════════════ EMPTY/UNINITIALIZED ═════════════════════════════════════════ │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 0x00     │ (null)                   │ Magic byte 0                                       │
│ 0x01     │ 0x00     │ (null)                   │ Magic byte 1                                       │
│ 0x02     │ 0x00     │ (null)                   │ Magic byte 2                                       │
│ 0x03     │ 0x00     │ (null)                   │ Magic byte 3                                       │
│          │          │                          │ As DWORD: 0x00000000 (never used)                  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ═════════════════════════════════════ INDEX RECORD ('INDX') ═══════════════════════════════════════ │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 0x49     │ 'I'                      │ Magic byte 0                                       │
│ 0x01     │ 0x4E     │ 'N'                      │ Magic byte 1                                       │
│ 0x02     │ 0x44     │ 'D'                      │ Magic byte 2                                       │
│ 0x03     │ 0x58     │ 'X'                      │ Magic byte 3                                       │
│          │          │                          │ As DWORD: 0x58444E49 (not MFT, directory index)    │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
```

**Note**: Due to little-endian byte order, code compares against `'ELIF'` not `'FILE'`.

### BaseFileRecordSegment Field (Offset 0x20) - Complete Byte-by-Byte

This 64-bit field is crucial for understanding extension records:

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ BASE_FILE_RECORD_SEGMENT (8 bytes at offset 0x20)                                                   │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ Field                    │ Description                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x20     │ 1 byte   │ BaseFRS[0]               │ Base FRS byte 0 (lowest)                           │
│ 0x21     │ 1 byte   │ BaseFRS[1]               │ Base FRS byte 1                                    │
│ 0x22     │ 1 byte   │ BaseFRS[2]               │ Base FRS byte 2                                    │
│ 0x23     │ 1 byte   │ BaseFRS[3]               │ Base FRS byte 3                                    │
│ 0x24     │ 1 byte   │ BaseFRS[4]               │ Base FRS byte 4                                    │
│ 0x25     │ 1 byte   │ BaseFRS[5]               │ Base FRS byte 5 (highest FRS byte)                 │
│          │          │                          │ Bytes 0-5: 48-bit FRS number of base record        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x26     │ 1 byte   │ BaseSequence[0]          │ Base sequence number low byte                      │
│ 0x27     │ 1 byte   │ BaseSequence[1]          │ Base sequence number high byte                     │
│          │          │                          │ Bytes 6-7: 16-bit sequence number of base record   │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘

If entire field == 0x0000000000000000:
    This IS the base record (attributes belong here)

If entire field != 0x0000000000000000:
    This is an EXTENSION record
    Attributes in this record belong to the base record
    base_frs = BaseFileRecordSegment & 0x0000FFFFFFFFFFFF
```

---

## Update Sequence Array (USA)

### Purpose

NTFS uses the USA to detect **torn writes** - when a power failure occurs mid-write, leaving a sector partially written. The USA ensures data integrity at the sector level.

### How It Works

1. **Before writing**: NTFS replaces the last 2 bytes of each 512-byte sector with a "check value"
2. **Original bytes**: Stored in the USA within the record header
3. **On reading**: Verify check values match, then restore original bytes

### USA Layout for 1024-Byte Record

```
Record Size: 1024 bytes = 2 sectors
USACount: 3 (1 check value + 2 sector fixups)

┌─────────────────────────────────────────────────────────────────────────────┐
│ USA Array (at offset USAOffset, typically 0x30)                             │
├─────────────────────────────────────────────────────────────────────────────┤
│ usa[0]: Check Value (2 bytes)                                               │
│         This value appears at the end of each sector                        │
├─────────────────────────────────────────────────────────────────────────────┤
│ usa[1]: Original bytes from offset 0x1FE (end of sector 1)                  │
│         These 2 bytes were replaced with usa[0] on disk                     │
├─────────────────────────────────────────────────────────────────────────────┤
│ usa[2]: Original bytes from offset 0x3FE (end of sector 2)                  │
│         These 2 bytes were replaced with usa[0] on disk                     │
└─────────────────────────────────────────────────────────────────────────────┘

Sector Layout:
┌─────────────────────────────────────────────────────────────────────────────┐
│ Sector 1 (bytes 0x000 - 0x1FF)                                              │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ ... record data ...                                    │ CHECK VALUE   │ │
│ │                                                        │ (2 bytes)     │ │
│ │                                                        │ @ 0x1FE       │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│ Sector 2 (bytes 0x200 - 0x3FF)                                              │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ ... record data ...                                    │ CHECK VALUE   │ │
│ │                                                        │ (2 bytes)     │ │
│ │                                                        │ @ 0x3FE       │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

### The unfixup() Algorithm

```cpp
bool unfixup(size_t max_size) {
    // Get pointer to USA
    unsigned short* usa = (unsigned short*)((char*)this + USAOffset);
    unsigned short check_value = usa[0];

    bool valid = true;

    // For each sector (i=1 is first sector, i=2 is second, etc.)
    for (unsigned short i = 1; i < USACount; i++) {
        // Calculate offset to last 2 bytes of sector i
        size_t offset = i * 512 - 2;  // 0x1FE, 0x3FE, 0x5FE, ...

        if (offset < max_size) {
            unsigned short* sector_end = (unsigned short*)((char*)this + offset);

            // Verify: sector end should contain check value
            if (*sector_end != check_value) {
                valid = false;  // TORN WRITE DETECTED!
            }

            // Restore: replace check value with original bytes
            *sector_end = usa[i];
        }
    }

    return valid;
}
```

### USA for Different Record Sizes

| Record Size | Sectors | USACount | USA Size |
|-------------|---------|----------|----------|
| 512 bytes | 1 | 2 | 4 bytes |
| 1024 bytes | 2 | 3 | 6 bytes |
| 2048 bytes | 4 | 5 | 10 bytes |
| 4096 bytes | 8 | 9 | 18 bytes |

---

## Attribute Record Header

### Common Header - Complete Byte-by-Byte (16 bytes)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ATTRIBUTE_RECORD_HEADER - Common Header (16 bytes = 0x10)                                           │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ Type[0] (byte 0, lowest)                           │
│ 0x01     │ 1 byte   │ unsigned char            │ Type[1] (byte 1)                                   │
│ 0x02     │ 1 byte   │ unsigned char            │ Type[2] (byte 2)                                   │
│ 0x03     │ 1 byte   │ unsigned char            │ Type[3] (byte 3, highest)                          │
│          │          │                          │ Common values: 0x10, 0x30, 0x80, 0xFFFFFFFF        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x04     │ 1 byte   │ unsigned char            │ Length[0] (byte 0, lowest)                         │
│ 0x05     │ 1 byte   │ unsigned char            │ Length[1] (byte 1)                                 │
│ 0x06     │ 1 byte   │ unsigned char            │ Length[2] (byte 2)                                 │
│ 0x07     │ 1 byte   │ unsigned char            │ Length[3] (byte 3, highest)                        │
│          │          │                          │ Total attribute length including this header       │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ IsNonResident                                      │
│          │          │                          │ 0x00 = Resident (data in MFT record)               │
│          │          │                          │ 0x01 = Non-Resident (data in clusters)             │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x09     │ 1 byte   │ unsigned char            │ NameLength                                         │
│          │          │                          │ Length of attribute name in Unicode characters     │
│          │          │                          │ 0 = unnamed (default stream)                       │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0A     │ 1 byte   │ unsigned char            │ NameOffset[0] (low byte)                           │
│ 0x0B     │ 1 byte   │ unsigned char            │ NameOffset[1] (high byte)                          │
│          │          │                          │ Offset to name from attribute start                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0C     │ 1 byte   │ unsigned char            │ Flags[0] (low byte)                                │
│ 0x0D     │ 1 byte   │ unsigned char            │ Flags[1] (high byte)                               │
│          │          │                          │ 0x0001=Compressed, 0x4000=Encrypted, 0x8000=Sparse │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0E     │ 1 byte   │ unsigned char            │ Instance[0] (low byte)                             │
│ 0x0F     │ 1 byte   │ unsigned char            │ Instance[1] (high byte)                            │
│          │          │                          │ Unique ID within this FILE record                  │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL COMMON HEADER: 16 bytes (0x10)
```

### Attribute Flags (Offset 0x0C) - Bit-by-Bit

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ATTRIBUTE FLAGS (2 bytes = 16 bits)                                                                 │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Bit      │ Mask     │ Name                     │ Description                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ Bit 0    │ 0x0001   │ ATTR_COMPRESSED          │ Attribute data is compressed                       │
│ Bit 1    │ 0x0002   │ (reserved)               │ Reserved                                           │
│ Bit 2    │ 0x0004   │ (reserved)               │ Reserved                                           │
│ Bit 3    │ 0x0008   │ (reserved)               │ Reserved                                           │
│ Bit 4    │ 0x0010   │ (reserved)               │ Reserved                                           │
│ Bit 5    │ 0x0020   │ (reserved)               │ Reserved                                           │
│ Bit 6    │ 0x0040   │ (reserved)               │ Reserved                                           │
│ Bit 7    │ 0x0080   │ (reserved)               │ Reserved                                           │
│ Bit 8    │ 0x0100   │ (reserved)               │ Reserved                                           │
│ Bit 9    │ 0x0200   │ (reserved)               │ Reserved                                           │
│ Bit 10   │ 0x0400   │ (reserved)               │ Reserved                                           │
│ Bit 11   │ 0x0800   │ (reserved)               │ Reserved                                           │
│ Bit 12   │ 0x1000   │ (reserved)               │ Reserved                                           │
│ Bit 13   │ 0x2000   │ (reserved)               │ Reserved                                           │
│ Bit 14   │ 0x4000   │ ATTR_ENCRYPTED           │ Attribute data is encrypted (EFS)                  │
│ Bit 15   │ 0x8000   │ ATTR_SPARSE              │ Attribute data is sparse                           │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
```

### Resident Attribute - Complete Byte-by-Byte (24 bytes header)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ RESIDENT ATTRIBUTE HEADER (24 bytes = 0x18, when IsNonResident == 0)                                │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ Type[0] (byte 0, lowest)                           │
│ 0x01     │ 1 byte   │ unsigned char            │ Type[1] (byte 1)                                   │
│ 0x02     │ 1 byte   │ unsigned char            │ Type[2] (byte 2)                                   │
│ 0x03     │ 1 byte   │ unsigned char            │ Type[3] (byte 3, highest)                          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x04     │ 1 byte   │ unsigned char            │ Length[0] (byte 0, lowest)                         │
│ 0x05     │ 1 byte   │ unsigned char            │ Length[1] (byte 1)                                 │
│ 0x06     │ 1 byte   │ unsigned char            │ Length[2] (byte 2)                                 │
│ 0x07     │ 1 byte   │ unsigned char            │ Length[3] (byte 3, highest)                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ IsNonResident = 0x00 (resident)                    │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x09     │ 1 byte   │ unsigned char            │ NameLength (0 = unnamed)                           │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0A     │ 1 byte   │ unsigned char            │ NameOffset[0] (low byte)                           │
│ 0x0B     │ 1 byte   │ unsigned char            │ NameOffset[1] (high byte)                          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0C     │ 1 byte   │ unsigned char            │ Flags[0] (low byte)                                │
│ 0x0D     │ 1 byte   │ unsigned char            │ Flags[1] (high byte)                               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0E     │ 1 byte   │ unsigned char            │ Instance[0] (low byte)                             │
│ 0x0F     │ 1 byte   │ unsigned char            │ Instance[1] (high byte)                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x10     │ 1 byte   │ unsigned char            │ ValueLength[0] (byte 0, lowest)                    │
│ 0x11     │ 1 byte   │ unsigned char            │ ValueLength[1] (byte 1)                            │
│ 0x12     │ 1 byte   │ unsigned char            │ ValueLength[2] (byte 2)                            │
│ 0x13     │ 1 byte   │ unsigned char            │ ValueLength[3] (byte 3, highest)                   │
│          │          │                          │ Length of attribute value in bytes                 │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x14     │ 1 byte   │ unsigned char            │ ValueOffset[0] (low byte)                          │
│ 0x15     │ 1 byte   │ unsigned char            │ ValueOffset[1] (high byte)                         │
│          │          │                          │ Offset to value from attribute start               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x16     │ 1 byte   │ unsigned char            │ IndexedFlag                                        │
│          │          │                          │ 0x00 = Not indexed, 0x01 = Indexed                 │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x17     │ 1 byte   │ unsigned char            │ Padding (alignment to 8-byte boundary)             │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL RESIDENT HEADER: 24 bytes (0x18)

Following the header:
┌──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ 0x18+    │ variable │ wchar_t[]                │ AttributeName (NameLength * 2 bytes, if named)     │
│ ...      │ variable │ unsigned char[]          │ AttributeValue (ValueLength bytes)                 │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
```

### Non-Resident Attribute - Complete Byte-by-Byte (64-72 bytes header)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ NON-RESIDENT ATTRIBUTE HEADER (64 bytes = 0x40, or 72 bytes = 0x48 if compressed)                   │
│ (when IsNonResident == 1)                                                                           │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ Type[0] (byte 0, lowest)                           │
│ 0x01     │ 1 byte   │ unsigned char            │ Type[1] (byte 1)                                   │
│ 0x02     │ 1 byte   │ unsigned char            │ Type[2] (byte 2)                                   │
│ 0x03     │ 1 byte   │ unsigned char            │ Type[3] (byte 3, highest)                          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x04     │ 1 byte   │ unsigned char            │ Length[0] (byte 0, lowest)                         │
│ 0x05     │ 1 byte   │ unsigned char            │ Length[1] (byte 1)                                 │
│ 0x06     │ 1 byte   │ unsigned char            │ Length[2] (byte 2)                                 │
│ 0x07     │ 1 byte   │ unsigned char            │ Length[3] (byte 3, highest)                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ IsNonResident = 0x01 (non-resident)                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x09     │ 1 byte   │ unsigned char            │ NameLength (0 = unnamed)                           │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0A     │ 1 byte   │ unsigned char            │ NameOffset[0] (low byte)                           │
│ 0x0B     │ 1 byte   │ unsigned char            │ NameOffset[1] (high byte)                          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0C     │ 1 byte   │ unsigned char            │ Flags[0] (low byte)                                │
│ 0x0D     │ 1 byte   │ unsigned char            │ Flags[1] (high byte)                               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0E     │ 1 byte   │ unsigned char            │ Instance[0] (low byte)                             │
│ 0x0F     │ 1 byte   │ unsigned char            │ Instance[1] (high byte)                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x10     │ 1 byte   │ unsigned char            │ LowestVCN[0] (byte 0, lowest)                      │
│ 0x11     │ 1 byte   │ unsigned char            │ LowestVCN[1] (byte 1)                              │
│ 0x12     │ 1 byte   │ unsigned char            │ LowestVCN[2] (byte 2)                              │
│ 0x13     │ 1 byte   │ unsigned char            │ LowestVCN[3] (byte 3)                              │
│ 0x14     │ 1 byte   │ unsigned char            │ LowestVCN[4] (byte 4)                              │
│ 0x15     │ 1 byte   │ unsigned char            │ LowestVCN[5] (byte 5)                              │
│ 0x16     │ 1 byte   │ unsigned char            │ LowestVCN[6] (byte 6)                              │
│ 0x17     │ 1 byte   │ unsigned char            │ LowestVCN[7] (byte 7, highest)                     │
│          │          │                          │ Starting Virtual Cluster Number (usually 0)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x18     │ 1 byte   │ unsigned char            │ HighestVCN[0] (byte 0, lowest)                     │
│ 0x19     │ 1 byte   │ unsigned char            │ HighestVCN[1] (byte 1)                             │
│ 0x1A     │ 1 byte   │ unsigned char            │ HighestVCN[2] (byte 2)                             │
│ 0x1B     │ 1 byte   │ unsigned char            │ HighestVCN[3] (byte 3)                             │
│ 0x1C     │ 1 byte   │ unsigned char            │ HighestVCN[4] (byte 4)                             │
│ 0x1D     │ 1 byte   │ unsigned char            │ HighestVCN[5] (byte 5)                             │
│ 0x1E     │ 1 byte   │ unsigned char            │ HighestVCN[6] (byte 6)                             │
│ 0x1F     │ 1 byte   │ unsigned char            │ HighestVCN[7] (byte 7, highest)                    │
│          │          │                          │ Ending Virtual Cluster Number                      │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x20     │ 1 byte   │ unsigned char            │ MappingPairsOffset[0] (low byte)                   │
│ 0x21     │ 1 byte   │ unsigned char            │ MappingPairsOffset[1] (high byte)                  │
│          │          │                          │ Offset to data run list from attribute start       │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x22     │ 1 byte   │ unsigned char            │ CompressionUnit                                    │
│          │          │                          │ Log2 of compression unit size in clusters          │
│          │          │                          │ 0 = not compressed, 4 = 16 clusters                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x23     │ 1 byte   │ unsigned char            │ Reserved[0]                                        │
│ 0x24     │ 1 byte   │ unsigned char            │ Reserved[1]                                        │
│ 0x25     │ 1 byte   │ unsigned char            │ Reserved[2]                                        │
│ 0x26     │ 1 byte   │ unsigned char            │ Reserved[3]                                        │
│ 0x27     │ 1 byte   │ unsigned char            │ Reserved[4]                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x28     │ 1 byte   │ unsigned char            │ AllocatedSize[0] (byte 0, lowest)                  │
│ 0x29     │ 1 byte   │ unsigned char            │ AllocatedSize[1] (byte 1)                          │
│ 0x2A     │ 1 byte   │ unsigned char            │ AllocatedSize[2] (byte 2)                          │
│ 0x2B     │ 1 byte   │ unsigned char            │ AllocatedSize[3] (byte 3)                          │
│ 0x2C     │ 1 byte   │ unsigned char            │ AllocatedSize[4] (byte 4)                          │
│ 0x2D     │ 1 byte   │ unsigned char            │ AllocatedSize[5] (byte 5)                          │
│ 0x2E     │ 1 byte   │ unsigned char            │ AllocatedSize[6] (byte 6)                          │
│ 0x2F     │ 1 byte   │ unsigned char            │ AllocatedSize[7] (byte 7, highest)                 │
│          │          │                          │ Space allocated on disk (cluster-aligned)          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x30     │ 1 byte   │ unsigned char            │ DataSize[0] (byte 0, lowest)                       │
│ 0x31     │ 1 byte   │ unsigned char            │ DataSize[1] (byte 1)                               │
│ 0x32     │ 1 byte   │ unsigned char            │ DataSize[2] (byte 2)                               │
│ 0x33     │ 1 byte   │ unsigned char            │ DataSize[3] (byte 3)                               │
│ 0x34     │ 1 byte   │ unsigned char            │ DataSize[4] (byte 4)                               │
│ 0x35     │ 1 byte   │ unsigned char            │ DataSize[5] (byte 5)                               │
│ 0x36     │ 1 byte   │ unsigned char            │ DataSize[6] (byte 6)                               │
│ 0x37     │ 1 byte   │ unsigned char            │ DataSize[7] (byte 7, highest)                      │
│          │          │                          │ Logical file size (what user sees)                 │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x38     │ 1 byte   │ unsigned char            │ InitializedSize[0] (byte 0, lowest)                │
│ 0x39     │ 1 byte   │ unsigned char            │ InitializedSize[1] (byte 1)                        │
│ 0x3A     │ 1 byte   │ unsigned char            │ InitializedSize[2] (byte 2)                        │
│ 0x3B     │ 1 byte   │ unsigned char            │ InitializedSize[3] (byte 3)                        │
│ 0x3C     │ 1 byte   │ unsigned char            │ InitializedSize[4] (byte 4)                        │
│ 0x3D     │ 1 byte   │ unsigned char            │ InitializedSize[5] (byte 5)                        │
│ 0x3E     │ 1 byte   │ unsigned char            │ InitializedSize[6] (byte 6)                        │
│ 0x3F     │ 1 byte   │ unsigned char            │ InitializedSize[7] (byte 7, highest)               │
│          │          │                          │ Valid data length (rest is zero-filled)            │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL NON-RESIDENT HEADER (uncompressed): 64 bytes (0x40)

If ATTR_COMPRESSED flag is set, add:
┌──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ 0x40     │ 1 byte   │ unsigned char            │ CompressedSize[0] (byte 0, lowest)                 │
│ 0x41     │ 1 byte   │ unsigned char            │ CompressedSize[1] (byte 1)                         │
│ 0x42     │ 1 byte   │ unsigned char            │ CompressedSize[2] (byte 2)                         │
│ 0x43     │ 1 byte   │ unsigned char            │ CompressedSize[3] (byte 3)                         │
│ 0x44     │ 1 byte   │ unsigned char            │ CompressedSize[4] (byte 4)                         │
│ 0x45     │ 1 byte   │ unsigned char            │ CompressedSize[5] (byte 5)                         │
│ 0x46     │ 1 byte   │ unsigned char            │ CompressedSize[6] (byte 6)                         │
│ 0x47     │ 1 byte   │ unsigned char            │ CompressedSize[7] (byte 7, highest)                │
│          │          │                          │ Actual compressed size on disk                     │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL NON-RESIDENT HEADER (compressed): 72 bytes (0x48)

Following the header:
┌──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ 0x40/48+ │ variable │ wchar_t[]                │ AttributeName (NameLength * 2 bytes, if named)     │
│ ...      │ variable │ unsigned char[]          │ DataRuns (mapping pairs, variable length)          │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
```

---

## Attribute Types

### Complete Attribute Type Table - Byte-by-Byte Type Codes

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ATTRIBUTE TYPE CODES (4 bytes each, little-endian DWORD)                                            │
├──────────┬──────────┬──────────┬──────────┬──────────┬──────────────────────────────────────────────┤
│ Byte 0   │ Byte 1   │ Byte 2   │ Byte 3   │ DWORD    │ Name & Description                           │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x10     │ 0x00     │ 0x00     │ 0x00     │ 0x10     │ $STANDARD_INFORMATION                        │
│          │          │          │          │          │ Resident: Always                             │
│          │          │          │          │          │ Timestamps, file attributes, security ID     │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x20     │ 0x00     │ 0x00     │ 0x00     │ 0x20     │ $ATTRIBUTE_LIST                              │
│          │          │          │          │          │ Resident: Usually                            │
│          │          │          │          │          │ Maps attributes to extension records         │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x30     │ 0x00     │ 0x00     │ 0x00     │ 0x30     │ $FILE_NAME                                   │
│          │          │          │          │          │ Resident: Always                             │
│          │          │          │          │          │ Filename, parent reference, timestamps       │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x40     │ 0x00     │ 0x00     │ 0x00     │ 0x40     │ $OBJECT_ID                                   │
│          │          │          │          │          │ Resident: Always                             │
│          │          │          │          │          │ GUID for distributed link tracking           │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x50     │ 0x00     │ 0x00     │ 0x00     │ 0x50     │ $SECURITY_DESCRIPTOR                         │
│          │          │          │          │          │ Resident: Varies                             │
│          │          │          │          │          │ ACLs (usually in $Secure instead)            │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x60     │ 0x00     │ 0x00     │ 0x00     │ 0x60     │ $VOLUME_NAME                                 │
│          │          │          │          │          │ Resident: Always                             │
│          │          │          │          │          │ Volume label (only in $Volume)               │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x70     │ 0x00     │ 0x00     │ 0x00     │ 0x70     │ $VOLUME_INFORMATION                          │
│          │          │          │          │          │ Resident: Always                             │
│          │          │          │          │          │ NTFS version (only in $Volume)               │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x80     │ 0x00     │ 0x00     │ 0x00     │ 0x80     │ $DATA                                        │
│          │          │          │          │          │ Resident: Varies (small files resident)      │
│          │          │          │          │          │ File content / alternate data streams        │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x90     │ 0x00     │ 0x00     │ 0x00     │ 0x90     │ $INDEX_ROOT                                  │
│          │          │          │          │          │ Resident: Always                             │
│          │          │          │          │          │ B+ tree root for directories                 │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0xA0     │ 0x00     │ 0x00     │ 0x00     │ 0xA0     │ $INDEX_ALLOCATION                            │
│          │          │          │          │          │ Resident: Never                              │
│          │          │          │          │          │ B+ tree nodes for directories                │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0xB0     │ 0x00     │ 0x00     │ 0x00     │ 0xB0     │ $BITMAP                                      │
│          │          │          │          │          │ Resident: Varies                             │
│          │          │          │          │          │ Allocation bitmap                            │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0xC0     │ 0x00     │ 0x00     │ 0x00     │ 0xC0     │ $REPARSE_POINT                               │
│          │          │          │          │          │ Resident: Always                             │
│          │          │          │          │          │ Symlinks, mount points, OneDrive             │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0xD0     │ 0x00     │ 0x00     │ 0x00     │ 0xD0     │ $EA_INFORMATION                              │
│          │          │          │          │          │ Resident: Always                             │
│          │          │          │          │          │ Extended attributes info                     │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0xE0     │ 0x00     │ 0x00     │ 0x00     │ 0xE0     │ $EA                                          │
│          │          │          │          │          │ Resident: Varies                             │
│          │          │          │          │          │ Extended attributes data                     │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0xF0     │ 0x00     │ 0x00     │ 0x00     │ 0xF0     │ $PROPERTY_SET                                │
│          │          │          │          │          │ Resident: Varies                             │
│          │          │          │          │          │ (Obsolete, not used in modern NTFS)          │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0x00     │ 0x01     │ 0x00     │ 0x00     │ 0x100    │ $LOGGED_UTILITY_STREAM                       │
│          │          │          │          │          │ Resident: Varies                             │
│          │          │          │          │          │ EFS, TxF data                                │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────────────────────────────────────────┤
│ 0xFF     │ 0xFF     │ 0xFF     │ 0xFF     │ 0xFFFFFFFF │ AttributeEnd                               │
│          │          │          │          │          │ **END MARKER** - stops attribute iteration   │
│          │          │          │          │          │ Not a real attribute, just a terminator      │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────────────────────────────────────────┘
```

---

## $STANDARD_INFORMATION (0x10)

### Complete Byte-by-Byte Structure (72 bytes for NTFS 3.0+)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ $STANDARD_INFORMATION ATTRIBUTE VALUE (72 bytes = 0x48 for NTFS 3.0+)                               │
│ Note: This is the VALUE portion, after the resident attribute header                                │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ CreationTime[0] (byte 0, lowest)                   │
│ 0x01     │ 1 byte   │ unsigned char            │ CreationTime[1] (byte 1)                           │
│ 0x02     │ 1 byte   │ unsigned char            │ CreationTime[2] (byte 2)                           │
│ 0x03     │ 1 byte   │ unsigned char            │ CreationTime[3] (byte 3)                           │
│ 0x04     │ 1 byte   │ unsigned char            │ CreationTime[4] (byte 4)                           │
│ 0x05     │ 1 byte   │ unsigned char            │ CreationTime[5] (byte 5)                           │
│ 0x06     │ 1 byte   │ unsigned char            │ CreationTime[6] (byte 6)                           │
│ 0x07     │ 1 byte   │ unsigned char            │ CreationTime[7] (byte 7, highest)                  │
│          │          │                          │ File creation time (FILETIME, 100ns since 1601)    │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ LastModificationTime[0] (byte 0, lowest)           │
│ 0x09     │ 1 byte   │ unsigned char            │ LastModificationTime[1] (byte 1)                   │
│ 0x0A     │ 1 byte   │ unsigned char            │ LastModificationTime[2] (byte 2)                   │
│ 0x0B     │ 1 byte   │ unsigned char            │ LastModificationTime[3] (byte 3)                   │
│ 0x0C     │ 1 byte   │ unsigned char            │ LastModificationTime[4] (byte 4)                   │
│ 0x0D     │ 1 byte   │ unsigned char            │ LastModificationTime[5] (byte 5)                   │
│ 0x0E     │ 1 byte   │ unsigned char            │ LastModificationTime[6] (byte 6)                   │
│ 0x0F     │ 1 byte   │ unsigned char            │ LastModificationTime[7] (byte 7, highest)          │
│          │          │                          │ Last content modification time                     │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x10     │ 1 byte   │ unsigned char            │ LastChangeTime[0] (byte 0, lowest)                 │
│ 0x11     │ 1 byte   │ unsigned char            │ LastChangeTime[1] (byte 1)                         │
│ 0x12     │ 1 byte   │ unsigned char            │ LastChangeTime[2] (byte 2)                         │
│ 0x13     │ 1 byte   │ unsigned char            │ LastChangeTime[3] (byte 3)                         │
│ 0x14     │ 1 byte   │ unsigned char            │ LastChangeTime[4] (byte 4)                         │
│ 0x15     │ 1 byte   │ unsigned char            │ LastChangeTime[5] (byte 5)                         │
│ 0x16     │ 1 byte   │ unsigned char            │ LastChangeTime[6] (byte 6)                         │
│ 0x17     │ 1 byte   │ unsigned char            │ LastChangeTime[7] (byte 7, highest)                │
│          │          │                          │ Last MFT record change time (metadata change)      │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x18     │ 1 byte   │ unsigned char            │ LastAccessTime[0] (byte 0, lowest)                 │
│ 0x19     │ 1 byte   │ unsigned char            │ LastAccessTime[1] (byte 1)                         │
│ 0x1A     │ 1 byte   │ unsigned char            │ LastAccessTime[2] (byte 2)                         │
│ 0x1B     │ 1 byte   │ unsigned char            │ LastAccessTime[3] (byte 3)                         │
│ 0x1C     │ 1 byte   │ unsigned char            │ LastAccessTime[4] (byte 4)                         │
│ 0x1D     │ 1 byte   │ unsigned char            │ LastAccessTime[5] (byte 5)                         │
│ 0x1E     │ 1 byte   │ unsigned char            │ LastAccessTime[6] (byte 6)                         │
│ 0x1F     │ 1 byte   │ unsigned char            │ LastAccessTime[7] (byte 7, highest)                │
│          │          │                          │ Last access time (often disabled for performance)  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x20     │ 1 byte   │ unsigned char            │ FileAttributes[0] (byte 0, lowest)                 │
│ 0x21     │ 1 byte   │ unsigned char            │ FileAttributes[1] (byte 1)                         │
│ 0x22     │ 1 byte   │ unsigned char            │ FileAttributes[2] (byte 2)                         │
│ 0x23     │ 1 byte   │ unsigned char            │ FileAttributes[3] (byte 3, highest)                │
│          │          │                          │ FILE_ATTRIBUTE_* flags (see bitmask below)         │
├──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┤
│ ═══════════════════════════════════ NTFS 1.2 ENDS HERE (36 bytes) ═══════════════════════════════   │
│ ═══════════════════════════════════ NTFS 3.0+ CONTINUES BELOW ═══════════════════════════════════   │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ 0x24     │ 1 byte   │ unsigned char            │ MaxVersions[0] (byte 0, lowest)                    │
│ 0x25     │ 1 byte   │ unsigned char            │ MaxVersions[1] (byte 1)                            │
│ 0x26     │ 1 byte   │ unsigned char            │ MaxVersions[2] (byte 2)                            │
│ 0x27     │ 1 byte   │ unsigned char            │ MaxVersions[3] (byte 3, highest)                   │
│          │          │                          │ Maximum allowed versions (usually 0)               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x28     │ 1 byte   │ unsigned char            │ VersionNumber[0] (byte 0, lowest)                  │
│ 0x29     │ 1 byte   │ unsigned char            │ VersionNumber[1] (byte 1)                          │
│ 0x2A     │ 1 byte   │ unsigned char            │ VersionNumber[2] (byte 2)                          │
│ 0x2B     │ 1 byte   │ unsigned char            │ VersionNumber[3] (byte 3, highest)                 │
│          │          │                          │ Current version number (usually 0)                 │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x2C     │ 1 byte   │ unsigned char            │ ClassId[0] (byte 0, lowest)                        │
│ 0x2D     │ 1 byte   │ unsigned char            │ ClassId[1] (byte 1)                                │
│ 0x2E     │ 1 byte   │ unsigned char            │ ClassId[2] (byte 2)                                │
│ 0x2F     │ 1 byte   │ unsigned char            │ ClassId[3] (byte 3, highest)                       │
│          │          │                          │ Class ID (usually 0)                               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x30     │ 1 byte   │ unsigned char            │ OwnerId[0] (byte 0, lowest)                        │
│ 0x31     │ 1 byte   │ unsigned char            │ OwnerId[1] (byte 1)                                │
│ 0x32     │ 1 byte   │ unsigned char            │ OwnerId[2] (byte 2)                                │
│ 0x33     │ 1 byte   │ unsigned char            │ OwnerId[3] (byte 3, highest)                       │
│          │          │                          │ Owner ID for quota tracking                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x34     │ 1 byte   │ unsigned char            │ SecurityId[0] (byte 0, lowest)                     │
│ 0x35     │ 1 byte   │ unsigned char            │ SecurityId[1] (byte 1)                             │
│ 0x36     │ 1 byte   │ unsigned char            │ SecurityId[2] (byte 2)                             │
│ 0x37     │ 1 byte   │ unsigned char            │ SecurityId[3] (byte 3, highest)                    │
│          │          │                          │ Security descriptor ID (index into $Secure)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x38     │ 1 byte   │ unsigned char            │ QuotaCharged[0] (byte 0, lowest)                   │
│ 0x39     │ 1 byte   │ unsigned char            │ QuotaCharged[1] (byte 1)                           │
│ 0x3A     │ 1 byte   │ unsigned char            │ QuotaCharged[2] (byte 2)                           │
│ 0x3B     │ 1 byte   │ unsigned char            │ QuotaCharged[3] (byte 3)                           │
│ 0x3C     │ 1 byte   │ unsigned char            │ QuotaCharged[4] (byte 4)                           │
│ 0x3D     │ 1 byte   │ unsigned char            │ QuotaCharged[5] (byte 5)                           │
│ 0x3E     │ 1 byte   │ unsigned char            │ QuotaCharged[6] (byte 6)                           │
│ 0x3F     │ 1 byte   │ unsigned char            │ QuotaCharged[7] (byte 7, highest)                  │
│          │          │                          │ Bytes charged to owner's quota                     │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x40     │ 1 byte   │ unsigned char            │ USN[0] (byte 0, lowest)                            │
│ 0x41     │ 1 byte   │ unsigned char            │ USN[1] (byte 1)                                    │
│ 0x42     │ 1 byte   │ unsigned char            │ USN[2] (byte 2)                                    │
│ 0x43     │ 1 byte   │ unsigned char            │ USN[3] (byte 3)                                    │
│ 0x44     │ 1 byte   │ unsigned char            │ USN[4] (byte 4)                                    │
│ 0x45     │ 1 byte   │ unsigned char            │ USN[5] (byte 5)                                    │
│ 0x46     │ 1 byte   │ unsigned char            │ USN[6] (byte 6)                                    │
│ 0x47     │ 1 byte   │ unsigned char            │ USN[7] (byte 7, highest)                           │
│          │          │                          │ Update Sequence Number (journal offset)            │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL $STANDARD_INFORMATION: 72 bytes (0x48) for NTFS 3.0+
                             36 bytes (0x24) for NTFS 1.2
```

### File Attributes Bitmask - Complete Bit-by-Bit (32 bits)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ FILE ATTRIBUTES (4 bytes = 32 bits at offset 0x20)                                                  │
├──────────┬──────────────┬────────────────────────────────────┬──────────────────────────────────────┤
│ Bit      │ Mask         │ Name                               │ Description                          │
├──────────┼──────────────┼────────────────────────────────────┼──────────────────────────────────────┤
│ Bit 0    │ 0x00000001   │ FILE_ATTRIBUTE_READONLY            │ Read-only file                       │
│ Bit 1    │ 0x00000002   │ FILE_ATTRIBUTE_HIDDEN              │ Hidden file                          │
│ Bit 2    │ 0x00000004   │ FILE_ATTRIBUTE_SYSTEM              │ System file                          │
│ Bit 3    │ 0x00000008   │ (reserved)                         │ Reserved (was volume label in DOS)   │
│ Bit 4    │ 0x00000010   │ FILE_ATTRIBUTE_DIRECTORY           │ Directory                            │
│ Bit 5    │ 0x00000020   │ FILE_ATTRIBUTE_ARCHIVE             │ Archive flag (needs backup)          │
│ Bit 6    │ 0x00000040   │ FILE_ATTRIBUTE_DEVICE              │ Device (reserved for system use)     │
│ Bit 7    │ 0x00000080   │ FILE_ATTRIBUTE_NORMAL              │ Normal (no other attributes set)     │
│ Bit 8    │ 0x00000100   │ FILE_ATTRIBUTE_TEMPORARY           │ Temporary file                       │
│ Bit 9    │ 0x00000200   │ FILE_ATTRIBUTE_SPARSE_FILE         │ Sparse file                          │
│ Bit 10   │ 0x00000400   │ FILE_ATTRIBUTE_REPARSE_POINT       │ Reparse point (symlink, junction)    │
│ Bit 11   │ 0x00000800   │ FILE_ATTRIBUTE_COMPRESSED          │ Compressed file/directory            │
│ Bit 12   │ 0x00001000   │ FILE_ATTRIBUTE_OFFLINE             │ Offline storage (HSM)                │
│ Bit 13   │ 0x00002000   │ FILE_ATTRIBUTE_NOT_CONTENT_INDEXED │ Not indexed by content indexer       │
│ Bit 14   │ 0x00004000   │ FILE_ATTRIBUTE_ENCRYPTED           │ Encrypted (EFS)                      │
│ Bit 15   │ 0x00008000   │ FILE_ATTRIBUTE_INTEGRITY_STREAM    │ Integrity stream (ReFS)              │
│ Bit 16   │ 0x00010000   │ FILE_ATTRIBUTE_VIRTUAL             │ Virtual file (reserved)              │
│ Bit 17   │ 0x00020000   │ FILE_ATTRIBUTE_NO_SCRUB_DATA       │ No scrub data (ReFS)                 │
│ Bit 18   │ 0x00040000   │ FILE_ATTRIBUTE_EA                  │ Has extended attributes              │
│ Bit 19   │ 0x00080000   │ FILE_ATTRIBUTE_PINNED              │ Pinned (OneDrive, always local)      │
│ Bit 20   │ 0x00100000   │ FILE_ATTRIBUTE_UNPINNED            │ Unpinned (OneDrive, cloud only)      │
│ Bit 21   │ 0x00200000   │ (reserved)                         │ Reserved                             │
│ Bit 22   │ 0x00400000   │ FILE_ATTRIBUTE_RECALL_ON_OPEN      │ Recall on open (cloud files)         │
│ Bit 23   │ 0x00800000   │ FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS│ Recall on data access               │
│ Bit 24   │ 0x01000000   │ (reserved)                         │ Reserved                             │
│ Bit 25   │ 0x02000000   │ (reserved)                         │ Reserved                             │
│ Bit 26   │ 0x04000000   │ (reserved)                         │ Reserved                             │
│ Bit 27   │ 0x08000000   │ (reserved)                         │ Reserved                             │
│ Bit 28   │ 0x10000000   │ (reserved)                         │ Reserved                             │
│ Bit 29   │ 0x20000000   │ (reserved)                         │ Reserved                             │
│ Bit 30   │ 0x40000000   │ (reserved)                         │ Reserved                             │
│ Bit 31   │ 0x80000000   │ (reserved)                         │ Reserved                             │
└──────────┴──────────────┴────────────────────────────────────┴──────────────────────────────────────┘
```

### Timestamp Format

NTFS timestamps are 64-bit values representing **100-nanosecond intervals since January 1, 1601 UTC**.

```cpp
// Convert NTFS timestamp to Windows FILETIME
FILETIME NtfsToFileTime(long long ntfs_time) {
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)(ntfs_time);
    ft.dwHighDateTime = (DWORD)(ntfs_time >> 32);
    return ft;
}

// Convert to Unix timestamp (seconds since 1970)
time_t NtfsToUnix(long long ntfs_time) {
    // 11644473600 = seconds between 1601 and 1970
    return (ntfs_time / 10000000) - 11644473600LL;
}
```

---

## $FILE_NAME (0x30)

### Complete Byte-by-Byte Structure (66+ bytes)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ $FILE_NAME ATTRIBUTE VALUE (66 bytes = 0x42 fixed header + variable filename)                      │
│ Note: This is the VALUE portion, after the resident attribute header                                │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ ParentDirectory[0] (byte 0, lowest)                │
│ 0x01     │ 1 byte   │ unsigned char            │ ParentDirectory[1] (byte 1)                        │
│ 0x02     │ 1 byte   │ unsigned char            │ ParentDirectory[2] (byte 2)                        │
│ 0x03     │ 1 byte   │ unsigned char            │ ParentDirectory[3] (byte 3)                        │
│ 0x04     │ 1 byte   │ unsigned char            │ ParentDirectory[4] (byte 4)                        │
│ 0x05     │ 1 byte   │ unsigned char            │ ParentDirectory[5] (byte 5, FRS ends here)         │
│ 0x06     │ 1 byte   │ unsigned char            │ ParentDirectory[6] (byte 6, seq low)               │
│ 0x07     │ 1 byte   │ unsigned char            │ ParentDirectory[7] (byte 7, seq high)              │
│          │          │                          │ Bits 0-47: Parent FRS, Bits 48-63: Sequence        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ CreationTime[0] (byte 0, lowest)                   │
│ 0x09     │ 1 byte   │ unsigned char            │ CreationTime[1] (byte 1)                           │
│ 0x0A     │ 1 byte   │ unsigned char            │ CreationTime[2] (byte 2)                           │
│ 0x0B     │ 1 byte   │ unsigned char            │ CreationTime[3] (byte 3)                           │
│ 0x0C     │ 1 byte   │ unsigned char            │ CreationTime[4] (byte 4)                           │
│ 0x0D     │ 1 byte   │ unsigned char            │ CreationTime[5] (byte 5)                           │
│ 0x0E     │ 1 byte   │ unsigned char            │ CreationTime[6] (byte 6)                           │
│ 0x0F     │ 1 byte   │ unsigned char            │ CreationTime[7] (byte 7, highest)                  │
│          │          │                          │ File creation time (FILETIME, snapshot at naming)  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x10     │ 1 byte   │ unsigned char            │ LastModificationTime[0] (byte 0, lowest)           │
│ 0x11     │ 1 byte   │ unsigned char            │ LastModificationTime[1] (byte 1)                   │
│ 0x12     │ 1 byte   │ unsigned char            │ LastModificationTime[2] (byte 2)                   │
│ 0x13     │ 1 byte   │ unsigned char            │ LastModificationTime[3] (byte 3)                   │
│ 0x14     │ 1 byte   │ unsigned char            │ LastModificationTime[4] (byte 4)                   │
│ 0x15     │ 1 byte   │ unsigned char            │ LastModificationTime[5] (byte 5)                   │
│ 0x16     │ 1 byte   │ unsigned char            │ LastModificationTime[6] (byte 6)                   │
│ 0x17     │ 1 byte   │ unsigned char            │ LastModificationTime[7] (byte 7, highest)          │
│          │          │                          │ Last modification time (snapshot at naming)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x18     │ 1 byte   │ unsigned char            │ LastChangeTime[0] (byte 0, lowest)                 │
│ 0x19     │ 1 byte   │ unsigned char            │ LastChangeTime[1] (byte 1)                         │
│ 0x1A     │ 1 byte   │ unsigned char            │ LastChangeTime[2] (byte 2)                         │
│ 0x1B     │ 1 byte   │ unsigned char            │ LastChangeTime[3] (byte 3)                         │
│ 0x1C     │ 1 byte   │ unsigned char            │ LastChangeTime[4] (byte 4)                         │
│ 0x1D     │ 1 byte   │ unsigned char            │ LastChangeTime[5] (byte 5)                         │
│ 0x1E     │ 1 byte   │ unsigned char            │ LastChangeTime[6] (byte 6)                         │
│ 0x1F     │ 1 byte   │ unsigned char            │ LastChangeTime[7] (byte 7, highest)                │
│          │          │                          │ MFT record change time (snapshot at naming)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x20     │ 1 byte   │ unsigned char            │ LastAccessTime[0] (byte 0, lowest)                 │
│ 0x21     │ 1 byte   │ unsigned char            │ LastAccessTime[1] (byte 1)                         │
│ 0x22     │ 1 byte   │ unsigned char            │ LastAccessTime[2] (byte 2)                         │
│ 0x23     │ 1 byte   │ unsigned char            │ LastAccessTime[3] (byte 3)                         │
│ 0x24     │ 1 byte   │ unsigned char            │ LastAccessTime[4] (byte 4)                         │
│ 0x25     │ 1 byte   │ unsigned char            │ LastAccessTime[5] (byte 5)                         │
│ 0x26     │ 1 byte   │ unsigned char            │ LastAccessTime[6] (byte 6)                         │
│ 0x27     │ 1 byte   │ unsigned char            │ LastAccessTime[7] (byte 7, highest)                │
│          │          │                          │ Last access time (snapshot at naming)              │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x28     │ 1 byte   │ unsigned char            │ AllocatedLength[0] (byte 0, lowest)                │
│ 0x29     │ 1 byte   │ unsigned char            │ AllocatedLength[1] (byte 1)                        │
│ 0x2A     │ 1 byte   │ unsigned char            │ AllocatedLength[2] (byte 2)                        │
│ 0x2B     │ 1 byte   │ unsigned char            │ AllocatedLength[3] (byte 3)                        │
│ 0x2C     │ 1 byte   │ unsigned char            │ AllocatedLength[4] (byte 4)                        │
│ 0x2D     │ 1 byte   │ unsigned char            │ AllocatedLength[5] (byte 5)                        │
│ 0x2E     │ 1 byte   │ unsigned char            │ AllocatedLength[6] (byte 6)                        │
│ 0x2F     │ 1 byte   │ unsigned char            │ AllocatedLength[7] (byte 7, highest)               │
│          │          │                          │ Allocated size on disk (cluster-aligned)           │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x30     │ 1 byte   │ unsigned char            │ FileSize[0] (byte 0, lowest)                       │
│ 0x31     │ 1 byte   │ unsigned char            │ FileSize[1] (byte 1)                               │
│ 0x32     │ 1 byte   │ unsigned char            │ FileSize[2] (byte 2)                               │
│ 0x33     │ 1 byte   │ unsigned char            │ FileSize[3] (byte 3)                               │
│ 0x34     │ 1 byte   │ unsigned char            │ FileSize[4] (byte 4)                               │
│ 0x35     │ 1 byte   │ unsigned char            │ FileSize[5] (byte 5)                               │
│ 0x36     │ 1 byte   │ unsigned char            │ FileSize[6] (byte 6)                               │
│ 0x37     │ 1 byte   │ unsigned char            │ FileSize[7] (byte 7, highest)                      │
│          │          │                          │ Logical file size (what user sees)                 │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x38     │ 1 byte   │ unsigned char            │ FileAttributes[0] (byte 0, lowest)                 │
│ 0x39     │ 1 byte   │ unsigned char            │ FileAttributes[1] (byte 1)                         │
│ 0x3A     │ 1 byte   │ unsigned char            │ FileAttributes[2] (byte 2)                         │
│ 0x3B     │ 1 byte   │ unsigned char            │ FileAttributes[3] (byte 3, highest)                │
│          │          │                          │ DOS file attributes (same bitmask as above)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x3C     │ 1 byte   │ unsigned char            │ PackedEaSize[0] (low byte)                         │
│ 0x3D     │ 1 byte   │ unsigned char            │ PackedEaSize[1] (high byte)                        │
│          │          │                          │ Size of extended attributes (usually 0)            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x3E     │ 1 byte   │ unsigned char            │ Reserved[0] (low byte)                             │
│ 0x3F     │ 1 byte   │ unsigned char            │ Reserved[1] (high byte)                            │
│          │          │                          │ Reserved for alignment (or ReparsePointTag)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x40     │ 1 byte   │ unsigned char            │ FileNameLength                                     │
│          │          │                          │ Length of filename in Unicode characters (1-255)   │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x41     │ 1 byte   │ unsigned char            │ Flags (Namespace)                                  │
│          │          │                          │ 0x00=POSIX, 0x01=Win32, 0x02=DOS, 0x03=Win32+DOS   │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x42     │ 2 bytes  │ wchar_t                  │ FileName[0] (first Unicode character)              │
│ 0x44     │ 2 bytes  │ wchar_t                  │ FileName[1] (second Unicode character)             │
│ 0x46     │ 2 bytes  │ wchar_t                  │ FileName[2] (third Unicode character)              │
│ ...      │ ...      │ ...                      │ ... continues for FileNameLength characters ...    │
│ 0x42+N*2 │ 2 bytes  │ wchar_t                  │ FileName[N-1] (last Unicode character)             │
│          │          │                          │ where N = FileNameLength                           │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL $FILE_NAME: 66 bytes (0x42) fixed header + (FileNameLength * 2) bytes for filename
Example: "test.txt" (8 chars) = 66 + 16 = 82 bytes
```

### ParentDirectory Field Breakdown (8 bytes at offset 0x00)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ParentDirectory (8 bytes = 64 bits) - FILE_REFERENCE structure                                     │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Byte     │ Bits     │ Field                    │ Description                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 0-7      │ FRS[0]                   │ File Record Segment number (byte 0, lowest)        │
│ 0x01     │ 8-15     │ FRS[1]                   │ File Record Segment number (byte 1)                │
│ 0x02     │ 16-23    │ FRS[2]                   │ File Record Segment number (byte 2)                │
│ 0x03     │ 24-31    │ FRS[3]                   │ File Record Segment number (byte 3)                │
│ 0x04     │ 32-39    │ FRS[4]                   │ File Record Segment number (byte 4)                │
│ 0x05     │ 40-47    │ FRS[5]                   │ File Record Segment number (byte 5, highest)       │
│          │          │                          │ 48-bit FRS: max 281,474,976,710,655 files          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x06     │ 48-55    │ SequenceNumber[0]        │ Sequence number (low byte)                         │
│ 0x07     │ 56-63    │ SequenceNumber[1]        │ Sequence number (high byte)                        │
│          │          │                          │ 16-bit sequence: detects stale references          │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘

// Extract parent FRS (C code):
unsigned long long parent_frs = ParentDirectory & 0x0000FFFFFFFFFFFFULL;

// Extract parent sequence (C code):
unsigned short parent_seq = (unsigned short)((ParentDirectory >> 48) & 0xFFFF);
```

### Filename Namespace Flags (Offset 0x41) - Complete

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ FILENAME NAMESPACE FLAGS (1 byte at offset 0x41)                                                    │
├──────────┬──────────────────────────────────┬───────────────────────────────────────────────────────┤
│ Value    │ Name                             │ Description                                           │
├──────────┼──────────────────────────────────┼───────────────────────────────────────────────────────┤
│ 0x00     │ FILE_NAME_POSIX                  │ Case-sensitive, any Unicode except NUL and /          │
│          │                                  │ Rarely used on Windows                                │
├──────────┼──────────────────────────────────┼───────────────────────────────────────────────────────┤
│ 0x01     │ FILE_NAME_WIN32                  │ Windows long filename (case-insensitive)              │
│          │                                  │ Up to 255 characters, most Unicode allowed            │
├──────────┼──────────────────────────────────┼───────────────────────────────────────────────────────┤
│ 0x02     │ FILE_NAME_DOS                    │ 8.3 short filename only                               │
│          │                                  │ Uppercase, limited character set                      │
│          │                                  │ UFFS SKIPS THIS to avoid duplicates                   │
├──────────┼──────────────────────────────────┼───────────────────────────────────────────────────────┤
│ 0x03     │ FILE_NAME_WIN32_AND_DOS          │ Long name is also valid as 8.3 name                   │
│          │                                  │ Single attribute serves both purposes                 │
└──────────┴──────────────────────────────────┴───────────────────────────────────────────────────────┘
```

**Important**: Files typically have **multiple** $FILE_NAME attributes:
- One with `FILE_NAME_WIN32` (0x01) - long name like "My Document.docx"
- One with `FILE_NAME_DOS` (0x02) - 8.3 short name like "MYDOCU~1.DOC"

**UFFS skips `FILE_NAME_DOS` (0x02)** to avoid duplicate entries in search results.

### Why Timestamps Are Duplicated

$FILE_NAME contains timestamps that are **copies** of $STANDARD_INFORMATION timestamps at the time the name was created. They are NOT updated when the file is modified.

**Use $STANDARD_INFORMATION timestamps** for accurate file times.

---

## $DATA (0x80)

### Overview

The $DATA attribute contains actual file content. A file can have:
- **One unnamed $DATA**: The default data stream (what you see in Explorer)
- **Multiple named $DATA**: Alternate Data Streams (ADS)

### Resident $DATA - Complete Byte-by-Byte (Small Files ≤ ~700 bytes)

Files smaller than ~700 bytes can be stored directly in the MFT record:

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ RESIDENT $DATA ATTRIBUTE (24 bytes header + variable value)                                        │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ Type[0] = 0x80 (byte 0)                            │
│ 0x01     │ 1 byte   │ unsigned char            │ Type[1] = 0x00 (byte 1)                            │
│ 0x02     │ 1 byte   │ unsigned char            │ Type[2] = 0x00 (byte 2)                            │
│ 0x03     │ 1 byte   │ unsigned char            │ Type[3] = 0x00 (byte 3)                            │
│          │          │                          │ Attribute type = 0x00000080 ($DATA)                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x04     │ 1 byte   │ unsigned char            │ Length[0] (byte 0, lowest)                         │
│ 0x05     │ 1 byte   │ unsigned char            │ Length[1] (byte 1)                                 │
│ 0x06     │ 1 byte   │ unsigned char            │ Length[2] (byte 2)                                 │
│ 0x07     │ 1 byte   │ unsigned char            │ Length[3] (byte 3, highest)                        │
│          │          │                          │ Total attribute length (header + value)            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ IsNonResident = 0x00 (resident)                    │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x09     │ 1 byte   │ unsigned char            │ NameLength (0 = default stream, >0 = ADS)          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0A     │ 1 byte   │ unsigned char            │ NameOffset[0] (low byte)                           │
│ 0x0B     │ 1 byte   │ unsigned char            │ NameOffset[1] (high byte)                          │
│          │          │                          │ Offset to stream name (usually 0x18)               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0C     │ 1 byte   │ unsigned char            │ Flags[0] (low byte)                                │
│ 0x0D     │ 1 byte   │ unsigned char            │ Flags[1] (high byte)                               │
│          │          │                          │ 0x0001=Compressed, 0x4000=Encrypted, 0x8000=Sparse │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0E     │ 1 byte   │ unsigned char            │ Instance[0] (low byte)                             │
│ 0x0F     │ 1 byte   │ unsigned char            │ Instance[1] (high byte)                            │
│          │          │                          │ Unique ID within this FILE record                  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x10     │ 1 byte   │ unsigned char            │ ValueLength[0] (byte 0, lowest)                    │
│ 0x11     │ 1 byte   │ unsigned char            │ ValueLength[1] (byte 1)                            │
│ 0x12     │ 1 byte   │ unsigned char            │ ValueLength[2] (byte 2)                            │
│ 0x13     │ 1 byte   │ unsigned char            │ ValueLength[3] (byte 3, highest)                   │
│          │          │                          │ Length of file data in bytes                       │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x14     │ 1 byte   │ unsigned char            │ ValueOffset[0] (low byte)                          │
│ 0x15     │ 1 byte   │ unsigned char            │ ValueOffset[1] (high byte)                         │
│          │          │                          │ Offset to file data from attribute start           │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x16     │ 1 byte   │ unsigned char            │ IndexedFlag = 0x00 (not indexed for $DATA)         │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x17     │ 1 byte   │ unsigned char            │ Padding (alignment)                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x18+    │ variable │ wchar_t[]                │ StreamName (NameLength * 2 bytes, if named ADS)    │
│          │          │                          │ Example: "Zone.Identifier" for downloaded files    │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ...      │ variable │ unsigned char[]          │ FileData (ValueLength bytes)                       │
│          │          │                          │ The actual file content                            │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL RESIDENT $DATA: 24 bytes header + (NameLength * 2) + ValueLength bytes
```

### Non-Resident $DATA - Complete Byte-by-Byte (Larger Files > ~700 bytes)

Files larger than ~700 bytes are stored in clusters outside the MFT:

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ NON-RESIDENT $DATA ATTRIBUTE (64-72 bytes header + variable data runs)                             │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ Type[0] = 0x80 (byte 0)                            │
│ 0x01     │ 1 byte   │ unsigned char            │ Type[1] = 0x00 (byte 1)                            │
│ 0x02     │ 1 byte   │ unsigned char            │ Type[2] = 0x00 (byte 2)                            │
│ 0x03     │ 1 byte   │ unsigned char            │ Type[3] = 0x00 (byte 3)                            │
│          │          │                          │ Attribute type = 0x00000080 ($DATA)                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x04     │ 1 byte   │ unsigned char            │ Length[0] (byte 0, lowest)                         │
│ 0x05     │ 1 byte   │ unsigned char            │ Length[1] (byte 1)                                 │
│ 0x06     │ 1 byte   │ unsigned char            │ Length[2] (byte 2)                                 │
│ 0x07     │ 1 byte   │ unsigned char            │ Length[3] (byte 3, highest)                        │
│          │          │                          │ Total attribute length (header + data runs)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ IsNonResident = 0x01 (non-resident)                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x09     │ 1 byte   │ unsigned char            │ NameLength (0 = default stream, >0 = ADS)          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0A     │ 1 byte   │ unsigned char            │ NameOffset[0] (low byte)                           │
│ 0x0B     │ 1 byte   │ unsigned char            │ NameOffset[1] (high byte)                          │
│          │          │                          │ Offset to stream name (usually 0x40)               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0C     │ 1 byte   │ unsigned char            │ Flags[0] (low byte)                                │
│ 0x0D     │ 1 byte   │ unsigned char            │ Flags[1] (high byte)                               │
│          │          │                          │ 0x0001=Compressed, 0x4000=Encrypted, 0x8000=Sparse │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x0E     │ 1 byte   │ unsigned char            │ Instance[0] (low byte)                             │
│ 0x0F     │ 1 byte   │ unsigned char            │ Instance[1] (high byte)                            │
│          │          │                          │ Unique ID within this FILE record                  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x10     │ 1 byte   │ unsigned char            │ LowestVCN[0] (byte 0, lowest)                      │
│ 0x11     │ 1 byte   │ unsigned char            │ LowestVCN[1] (byte 1)                              │
│ 0x12     │ 1 byte   │ unsigned char            │ LowestVCN[2] (byte 2)                              │
│ 0x13     │ 1 byte   │ unsigned char            │ LowestVCN[3] (byte 3)                              │
│ 0x14     │ 1 byte   │ unsigned char            │ LowestVCN[4] (byte 4)                              │
│ 0x15     │ 1 byte   │ unsigned char            │ LowestVCN[5] (byte 5)                              │
│ 0x16     │ 1 byte   │ unsigned char            │ LowestVCN[6] (byte 6)                              │
│ 0x17     │ 1 byte   │ unsigned char            │ LowestVCN[7] (byte 7, highest)                     │
│          │          │                          │ Starting VCN (usually 0 for base record)           │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x18     │ 1 byte   │ unsigned char            │ HighestVCN[0] (byte 0, lowest)                     │
│ 0x19     │ 1 byte   │ unsigned char            │ HighestVCN[1] (byte 1)                             │
│ 0x1A     │ 1 byte   │ unsigned char            │ HighestVCN[2] (byte 2)                             │
│ 0x1B     │ 1 byte   │ unsigned char            │ HighestVCN[3] (byte 3)                             │
│ 0x1C     │ 1 byte   │ unsigned char            │ HighestVCN[4] (byte 4)                             │
│ 0x1D     │ 1 byte   │ unsigned char            │ HighestVCN[5] (byte 5)                             │
│ 0x1E     │ 1 byte   │ unsigned char            │ HighestVCN[6] (byte 6)                             │
│ 0x1F     │ 1 byte   │ unsigned char            │ HighestVCN[7] (byte 7, highest)                    │
│          │          │                          │ Ending VCN (= total clusters - 1)                  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x20     │ 1 byte   │ unsigned char            │ MappingPairsOffset[0] (low byte)                   │
│ 0x21     │ 1 byte   │ unsigned char            │ MappingPairsOffset[1] (high byte)                  │
│          │          │                          │ Offset to data runs from attribute start           │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x22     │ 1 byte   │ unsigned char            │ CompressionUnit                                    │
│          │          │                          │ 0 = not compressed, 4 = 16 clusters                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x23     │ 1 byte   │ unsigned char            │ Reserved[0]                                        │
│ 0x24     │ 1 byte   │ unsigned char            │ Reserved[1]                                        │
│ 0x25     │ 1 byte   │ unsigned char            │ Reserved[2]                                        │
│ 0x26     │ 1 byte   │ unsigned char            │ Reserved[3]                                        │
│ 0x27     │ 1 byte   │ unsigned char            │ Reserved[4]                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x28     │ 1 byte   │ unsigned char            │ AllocatedSize[0] (byte 0, lowest)                  │
│ 0x29     │ 1 byte   │ unsigned char            │ AllocatedSize[1] (byte 1)                          │
│ 0x2A     │ 1 byte   │ unsigned char            │ AllocatedSize[2] (byte 2)                          │
│ 0x2B     │ 1 byte   │ unsigned char            │ AllocatedSize[3] (byte 3)                          │
│ 0x2C     │ 1 byte   │ unsigned char            │ AllocatedSize[4] (byte 4)                          │
│ 0x2D     │ 1 byte   │ unsigned char            │ AllocatedSize[5] (byte 5)                          │
│ 0x2E     │ 1 byte   │ unsigned char            │ AllocatedSize[6] (byte 6)                          │
│ 0x2F     │ 1 byte   │ unsigned char            │ AllocatedSize[7] (byte 7, highest)                 │
│          │          │                          │ Space allocated on disk (cluster-aligned)          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x30     │ 1 byte   │ unsigned char            │ DataSize[0] (byte 0, lowest)                       │
│ 0x31     │ 1 byte   │ unsigned char            │ DataSize[1] (byte 1)                               │
│ 0x32     │ 1 byte   │ unsigned char            │ DataSize[2] (byte 2)                               │
│ 0x33     │ 1 byte   │ unsigned char            │ DataSize[3] (byte 3)                               │
│ 0x34     │ 1 byte   │ unsigned char            │ DataSize[4] (byte 4)                               │
│ 0x35     │ 1 byte   │ unsigned char            │ DataSize[5] (byte 5)                               │
│ 0x36     │ 1 byte   │ unsigned char            │ DataSize[6] (byte 6)                               │
│ 0x37     │ 1 byte   │ unsigned char            │ DataSize[7] (byte 7, highest)                      │
│          │          │                          │ Logical file size (what user sees)                 │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x38     │ 1 byte   │ unsigned char            │ InitializedSize[0] (byte 0, lowest)                │
│ 0x39     │ 1 byte   │ unsigned char            │ InitializedSize[1] (byte 1)                        │
│ 0x3A     │ 1 byte   │ unsigned char            │ InitializedSize[2] (byte 2)                        │
│ 0x3B     │ 1 byte   │ unsigned char            │ InitializedSize[3] (byte 3)                        │
│ 0x3C     │ 1 byte   │ unsigned char            │ InitializedSize[4] (byte 4)                        │
│ 0x3D     │ 1 byte   │ unsigned char            │ InitializedSize[5] (byte 5)                        │
│ 0x3E     │ 1 byte   │ unsigned char            │ InitializedSize[6] (byte 6)                        │
│ 0x3F     │ 1 byte   │ unsigned char            │ InitializedSize[7] (byte 7, highest)               │
│          │          │                          │ Valid data length (rest is zero-filled)            │
├──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┤
│ ═══════════════════════════════ IF COMPRESSED (Flags & 0x0001) ═══════════════════════════════════  │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ 0x40     │ 1 byte   │ unsigned char            │ CompressedSize[0] (byte 0, lowest)                 │
│ 0x41     │ 1 byte   │ unsigned char            │ CompressedSize[1] (byte 1)                         │
│ 0x42     │ 1 byte   │ unsigned char            │ CompressedSize[2] (byte 2)                         │
│ 0x43     │ 1 byte   │ unsigned char            │ CompressedSize[3] (byte 3)                         │
│ 0x44     │ 1 byte   │ unsigned char            │ CompressedSize[4] (byte 4)                         │
│ 0x45     │ 1 byte   │ unsigned char            │ CompressedSize[5] (byte 5)                         │
│ 0x46     │ 1 byte   │ unsigned char            │ CompressedSize[6] (byte 6)                         │
│ 0x47     │ 1 byte   │ unsigned char            │ CompressedSize[7] (byte 7, highest)                │
│          │          │                          │ Actual compressed size on disk                     │
├──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┤
│ ═══════════════════════════════════════════════════════════════════════════════════════════════════ │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ 0x40/48+ │ variable │ wchar_t[]                │ StreamName (NameLength * 2 bytes, if named ADS)    │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ...      │ variable │ unsigned char[]          │ DataRuns (mapping pairs, see below)                │
│          │          │                          │ Terminated by 0x00 byte                            │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL NON-RESIDENT $DATA: 64 bytes (uncompressed) or 72 bytes (compressed) header
                         + (NameLength * 2) + DataRuns length
```

### Data Run Encoding - Complete Byte-by-Byte

Data runs (mapping pairs) encode where file data is stored on disk:

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ DATA RUN ENCODING (Variable length per run)                                                         │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Byte     │ Size     │ Field                    │ Description                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0        │ 1 byte   │ Header                   │ 0xLO where:                                        │
│          │          │                          │   L = number of bytes for LCN offset (0-8)         │
│          │          │                          │   O = number of bytes for length (0-8)             │
│          │          │                          │ 0x00 = END OF DATA RUNS                            │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 1        │ O bytes  │ Length                   │ Number of clusters in this run (little-endian)    │
│          │          │                          │ Unsigned integer                                   │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 1+O      │ L bytes  │ LCN Offset               │ Signed offset from previous LCN (little-endian)   │
│          │          │                          │ First run: absolute LCN                            │
│          │          │                          │ Subsequent runs: delta from previous LCN           │
│          │          │                          │ L=0 means SPARSE (no clusters allocated)           │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘

EXAMPLE: Header = 0x31 (3 bytes offset, 1 byte length)
┌──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Byte 0   │ 1 byte   │ Header = 0x31            │ L=3 (offset bytes), O=1 (length bytes)             │
│ Byte 1   │ 1 byte   │ Length = 0x05            │ 5 clusters                                         │
│ Byte 2   │ 1 byte   │ LCN[0] = 0x00            │ LCN offset byte 0 (lowest)                         │
│ Byte 3   │ 1 byte   │ LCN[1] = 0x10            │ LCN offset byte 1                                  │
│ Byte 4   │ 1 byte   │ LCN[2] = 0x00            │ LCN offset byte 2 (highest)                        │
│          │          │                          │ LCN = 0x001000 = cluster 4096                      │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
Total run: 5 bytes (1 header + 1 length + 3 offset)

SPARSE RUN EXAMPLE: Header = 0x01 (0 bytes offset, 1 byte length)
┌──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Byte 0   │ 1 byte   │ Header = 0x01            │ L=0 (no offset = sparse), O=1 (length bytes)       │
│ Byte 1   │ 1 byte   │ Length = 0x10            │ 16 sparse clusters (read as zeros)                 │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
Total run: 2 bytes (1 header + 1 length + 0 offset)

END MARKER:
┌──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Byte N   │ 1 byte   │ Header = 0x00            │ End of data runs                                   │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
```

---

## Record Termination

### How Records End

**There is NO delimiter between MFT records.** Records are fixed-size and contiguous.

**Within a record**, attributes are terminated by the `AttributeEnd` marker:

### AttributeEnd Marker - Complete Byte-by-Byte

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ATTRIBUTE_END MARKER (4 bytes)                                                                      │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ EndMarker[0] = 0xFF (byte 0)                       │
│ 0x01     │ 1 byte   │ unsigned char            │ EndMarker[1] = 0xFF (byte 1)                       │
│ 0x02     │ 1 byte   │ unsigned char            │ EndMarker[2] = 0xFF (byte 2)                       │
│ 0x03     │ 1 byte   │ unsigned char            │ EndMarker[3] = 0xFF (byte 3)                       │
│          │          │                          │ Value = 0xFFFFFFFF (AttributeEnd)                  │
│          │          │                          │ Signals end of attribute list                      │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
```

### Complete Record Layout with Termination

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ COMPLETE MFT RECORD LAYOUT (1024 bytes typical)                                                     │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ Content                  │ Description                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x000    │ 48 bytes │ FILE_RECORD_SEGMENT_HDR  │ Fixed header (Magic, USA, LSN, Flags, etc.)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x030    │ 6 bytes  │ Update Sequence Array    │ USA check value + replacement values               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x036    │ 2 bytes  │ Padding                  │ Alignment to 8-byte boundary                       │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x038    │ variable │ Attribute 1              │ First attribute (usually $STANDARD_INFORMATION)   │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ...      │ variable │ Attribute 2              │ Second attribute (usually $FILE_NAME)             │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ...      │ variable │ Attribute N              │ Last attribute                                     │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ...      │ 4 bytes  │ 0xFFFFFFFF               │ AttributeEnd marker                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ ...      │ variable │ Slack Space              │ Unused (may contain remnants of old data)          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x1FE    │ 2 bytes  │ USA Replacement #1       │ Original bytes from sector 1 end (restored)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x3FE    │ 2 bytes  │ USA Replacement #2       │ Original bytes from sector 2 end (restored)        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x400    │ ---      │ NEXT RECORD STARTS       │ No delimiter - records are contiguous              │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
```

### Attribute Iteration Algorithm

```cpp
void iterate_attributes(FILE_RECORD_SEGMENT_HEADER* frsh, size_t record_size) {
    // Calculate end boundary
    void* record_end = (char*)frsh + min(record_size, frsh->BytesInUse);

    // Start at first attribute
    ATTRIBUTE_RECORD_HEADER* attr = (ATTRIBUTE_RECORD_HEADER*)
        ((char*)frsh + frsh->FirstAttributeOffset);

    while (attr < record_end) {
        // Check for end marker
        if (attr->Type == 0xFFFFFFFF) {  // AttributeEnd
            break;  // Done!
        }

        // Check for invalid type
        if (attr->Type == 0) {
            break;  // Invalid, stop
        }

        // Prevent infinite loop
        if (attr->Length == 0) {
            break;  // Corrupted, stop
        }

        // Process this attribute
        process_attribute(attr);

        // Move to next attribute
        attr = (ATTRIBUTE_RECORD_HEADER*)((char*)attr + attr->Length);
    }
}
```

### Termination Conditions

| Condition | Meaning |
|-----------|---------|
| `attr->Type == 0xFFFFFFFF` | Normal end of attributes |
| `attr->Type == 0` | Invalid/corrupted record |
| `attr->Length == 0` | Corrupted attribute (would cause infinite loop) |
| `attr >= record_end` | Ran past end of record |

---

## Extension Records

### When Extension Records Are Needed

A single MFT record (typically 1024 bytes) can only hold so much data. Extension records are needed when:

| Scenario | Why It Overflows |
|----------|------------------|
| **Many hard links** | Each hard link = one $FILE_NAME attribute (~100+ bytes each) |
| **Many alternate data streams** | Each ADS = one $DATA attribute header |
| **Highly fragmented file** | Long data run lists in $DATA attribute |
| **Many extended attributes** | Large $EA attribute |
| **Very long filename** | Unicode filename up to 255 chars = 510 bytes |

### How Extension Records Work

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ BASE RECORD (FRS 1000)                                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│ BaseFileRecordSegment = 0 (this IS the base)                                │
│                                                                             │
│ $STANDARD_INFORMATION (timestamps, attributes)                              │
│ $FILE_NAME (first filename)                                                 │
│ $ATTRIBUTE_LIST (maps attributes to extension records)                      │
│ $DATA (first part of data runs, or resident data)                           │
│ AttributeEnd (0xFFFFFFFF)                                                   │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ $ATTRIBUTE_LIST points to:
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ EXTENSION RECORD 1 (FRS 5000)                                               │
├─────────────────────────────────────────────────────────────────────────────┤
│ BaseFileRecordSegment = 1000 (points to base)                               │
│                                                                             │
│ $FILE_NAME (second hard link)                                               │
│ $FILE_NAME (third hard link)                                                │
│ $DATA (continuation of data runs)                                           │
│ AttributeEnd (0xFFFFFFFF)                                                   │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ EXTENSION RECORD 2 (FRS 5001)                                               │
├─────────────────────────────────────────────────────────────────────────────┤
│ BaseFileRecordSegment = 1000 (points to base)                               │
│                                                                             │
│ $DATA (more data runs for highly fragmented file)                           │
│ AttributeEnd (0xFFFFFFFF)                                                   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Identifying Extension Records

```cpp
void parse_record(size_t frs, FILE_RECORD_SEGMENT_HEADER* frsh) {
    // Extract base FRS from BaseFileRecordSegment field
    unsigned long long base_frs_raw = frsh->BaseFileRecordSegment;
    unsigned long long base_frs = base_frs_raw & 0x0000FFFFFFFFFFFF;

    if (base_frs == 0) {
        // This IS the base record
        // Attributes belong to this record (FRS)
        process_base_record(frs, frsh);
    } else {
        // This is an EXTENSION record
        // Attributes belong to the BASE record
        process_extension_record(base_frs, frsh);
    }
}
```

### UFFS Extension Record Handling

```cpp
// From ntfs_index.hpp
unsigned int const frs_base = frsh->BaseFileRecordSegment
    ? static_cast<unsigned int>(frsh->BaseFileRecordSegment)
    : frs;

auto base_record = this->at(frs_base);

// All attributes in this record are added to base_record
for (auto* ah = frsh->begin(); ah < frsh->end(mft_record_size); ah = ah->next()) {
    switch (ah->Type) {
        case AttributeFileName:
            // Add filename to base_record, not to extension record
            add_filename(base_record, ah);
            break;
        case AttributeData:
            // Add stream info to base_record
            add_stream(base_record, ah);
            break;
        // ...
    }
}
```

---

## $ATTRIBUTE_LIST (0x20)

### Purpose

When a file spans multiple MFT records, the base record contains an $ATTRIBUTE_LIST that maps each attribute to its location (which FRS contains it).

### Complete Byte-by-Byte Structure

The $ATTRIBUTE_LIST is a sequence of variable-length entries. Each entry is at least 26 bytes (0x1A).

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ $ATTRIBUTE_LIST ENTRY (26 bytes = 0x1A minimum + variable name)                                     │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Offset   │ Size     │ C Type                   │ Field & Description                                │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x00     │ 1 byte   │ unsigned char            │ AttributeType[0] (byte 0, lowest)                  │
│ 0x01     │ 1 byte   │ unsigned char            │ AttributeType[1] (byte 1)                          │
│ 0x02     │ 1 byte   │ unsigned char            │ AttributeType[2] (byte 2)                          │
│ 0x03     │ 1 byte   │ unsigned char            │ AttributeType[3] (byte 3, highest)                 │
│          │          │                          │ Type of attribute (0x10, 0x30, 0x80, etc.)         │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x04     │ 1 byte   │ unsigned char            │ RecordLength[0] (low byte)                         │
│ 0x05     │ 1 byte   │ unsigned char            │ RecordLength[1] (high byte)                        │
│          │          │                          │ Length of this entry (including name)              │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x06     │ 1 byte   │ unsigned char            │ NameLength                                         │
│          │          │                          │ Attribute name length in characters (0 = unnamed)  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x07     │ 1 byte   │ unsigned char            │ NameOffset                                         │
│          │          │                          │ Offset to name from entry start (usually 0x1A)     │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x08     │ 1 byte   │ unsigned char            │ LowestVCN[0] (byte 0, lowest)                      │
│ 0x09     │ 1 byte   │ unsigned char            │ LowestVCN[1] (byte 1)                              │
│ 0x0A     │ 1 byte   │ unsigned char            │ LowestVCN[2] (byte 2)                              │
│ 0x0B     │ 1 byte   │ unsigned char            │ LowestVCN[3] (byte 3)                              │
│ 0x0C     │ 1 byte   │ unsigned char            │ LowestVCN[4] (byte 4)                              │
│ 0x0D     │ 1 byte   │ unsigned char            │ LowestVCN[5] (byte 5)                              │
│ 0x0E     │ 1 byte   │ unsigned char            │ LowestVCN[6] (byte 6)                              │
│ 0x0F     │ 1 byte   │ unsigned char            │ LowestVCN[7] (byte 7, highest)                     │
│          │          │                          │ Starting VCN for non-resident attributes           │
│          │          │                          │ 0 for resident attributes                          │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x10     │ 1 byte   │ unsigned char            │ SegmentReference[0] (byte 0, lowest)               │
│ 0x11     │ 1 byte   │ unsigned char            │ SegmentReference[1] (byte 1)                       │
│ 0x12     │ 1 byte   │ unsigned char            │ SegmentReference[2] (byte 2)                       │
│ 0x13     │ 1 byte   │ unsigned char            │ SegmentReference[3] (byte 3)                       │
│ 0x14     │ 1 byte   │ unsigned char            │ SegmentReference[4] (byte 4)                       │
│ 0x15     │ 1 byte   │ unsigned char            │ SegmentReference[5] (byte 5, highest FRS byte)     │
│          │          │                          │ Bytes 0-5: FRS number (48 bits)                    │
│ 0x16     │ 1 byte   │ unsigned char            │ SegmentReference[6] (sequence low byte)            │
│ 0x17     │ 1 byte   │ unsigned char            │ SegmentReference[7] (sequence high byte)           │
│          │          │                          │ Bytes 6-7: Sequence number (16 bits)               │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x18     │ 1 byte   │ unsigned char            │ Instance[0] (low byte)                             │
│ 0x19     │ 1 byte   │ unsigned char            │ Instance[1] (high byte)                            │
│          │          │                          │ Attribute instance number (matches attr header)    │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0x1A+    │ variable │ wchar_t[]                │ AttributeName (NameLength * 2 bytes)               │
│          │          │                          │ Unicode name for named attributes (e.g., ADS)      │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘
TOTAL ENTRY SIZE: 26 bytes (0x1A) + (NameLength * 2) bytes
```

### SegmentReference Field - Complete Byte-by-Byte

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ SEGMENT_REFERENCE (8 bytes = 64 bits)                                                               │
├──────────┬──────────┬──────────────────────────┬────────────────────────────────────────────────────┤
│ Byte     │ Size     │ Field                    │ Description                                        │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 0        │ 1 byte   │ FRS[0]                   │ FRS number byte 0 (lowest)                         │
│ 1        │ 1 byte   │ FRS[1]                   │ FRS number byte 1                                  │
│ 2        │ 1 byte   │ FRS[2]                   │ FRS number byte 2                                  │
│ 3        │ 1 byte   │ FRS[3]                   │ FRS number byte 3                                  │
│ 4        │ 1 byte   │ FRS[4]                   │ FRS number byte 4                                  │
│ 5        │ 1 byte   │ FRS[5]                   │ FRS number byte 5 (highest)                        │
│          │          │                          │ 48-bit FRS = bytes 0-5 (max 281 trillion records)  │
├──────────┼──────────┼──────────────────────────┼────────────────────────────────────────────────────┤
│ 6        │ 1 byte   │ SequenceNumber[0]        │ Sequence number low byte                           │
│ 7        │ 1 byte   │ SequenceNumber[1]        │ Sequence number high byte                          │
│          │          │                          │ 16-bit sequence (0-65535, wraps around)            │
└──────────┴──────────┴──────────────────────────┴────────────────────────────────────────────────────┘

// Extract FRS from SegmentReference:
unsigned long long frs = SegmentReference & 0x0000FFFFFFFFFFFF;  // Mask off sequence

// Extract sequence number:
unsigned short seq = (unsigned short)(SegmentReference >> 48);
```

### Example $ATTRIBUTE_LIST

```
File with 3 hard links and fragmented data:

$ATTRIBUTE_LIST entries:
┌─────────────────────────────────────────────────────────────────────────────┐
│ Entry 1: $STANDARD_INFORMATION in FRS 1000 (base)                           │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Type=0x10 │ RecordLen=0x1A │ NameLen=0 │ VCN=0 │ FRS=1000 │ Inst=0     │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│ Entry 2: $FILE_NAME #1 in FRS 1000 (base)                                   │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Type=0x30 │ RecordLen=0x1A │ NameLen=0 │ VCN=0 │ FRS=1000 │ Inst=1     │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│ Entry 3: $FILE_NAME #2 in FRS 5000 (extension)                              │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Type=0x30 │ RecordLen=0x1A │ NameLen=0 │ VCN=0 │ FRS=5000 │ Inst=2     │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│ Entry 4: $FILE_NAME #3 in FRS 5000 (extension)                              │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Type=0x30 │ RecordLen=0x1A │ NameLen=0 │ VCN=0 │ FRS=5000 │ Inst=3     │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│ Entry 5: $DATA (VCN 0-999) in FRS 1000 (base)                               │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Type=0x80 │ RecordLen=0x1A │ NameLen=0 │ VCN=0 │ FRS=1000 │ Inst=4     │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│ Entry 6: $DATA (VCN 1000-1999) in FRS 5001 (extension)                      │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Type=0x80 │ RecordLen=0x1A │ NameLen=0 │ VCN=1000 │ FRS=5001 │ Inst=4  │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Why UFFS Doesn't Parse $ATTRIBUTE_LIST

UFFS takes a simpler approach:

1. **Scan ALL MFT records** sequentially
2. **For each record**, check `BaseFileRecordSegment`
3. **If non-zero**, redirect attributes to the base record

This works because:
- We're reading the entire MFT anyway
- Extension records always point back to their base
- No need to follow $ATTRIBUTE_LIST pointers

```cpp
// UFFS approach: simple and effective
for (size_t frs = 0; frs < num_records; frs++) {
    FILE_RECORD_SEGMENT_HEADER* frsh = get_record(frs);

    // Determine where attributes belong
    unsigned int base_frs = frsh->BaseFileRecordSegment
        ? (unsigned int)frsh->BaseFileRecordSegment
        : frs;

    Record* target = &records[base_frs];

    // Parse attributes into target record
    for (auto* attr = frsh->begin(); ...) {
        add_attribute_to_record(target, attr);
    }
}
```

---

## Complete Parsing Algorithm

### Step-by-Step Process

```cpp
void parse_mft(unsigned char* mft_data, size_t mft_size, size_t record_size) {
    size_t num_records = mft_size / record_size;

    for (size_t frs = 0; frs < num_records; frs++) {
        unsigned char* record_ptr = mft_data + (frs * record_size);
        FILE_RECORD_SEGMENT_HEADER* frsh = (FILE_RECORD_SEGMENT_HEADER*)record_ptr;

        // ═══════════════════════════════════════════════════════════════════
        // STEP 1: Validate Magic Number
        // ═══════════════════════════════════════════════════════════════════
        if (frsh->MultiSectorHeader.Magic != 'ELIF') {  // 'FILE' in little-endian
            continue;  // Not a valid record (empty, corrupted, or already marked BAAD)
        }

        // ═══════════════════════════════════════════════════════════════════
        // STEP 2: Apply USA Fixup
        // ═══════════════════════════════════════════════════════════════════
        if (!frsh->MultiSectorHeader.unfixup(record_size)) {
            frsh->MultiSectorHeader.Magic = 'DAAB';  // Mark as corrupted
            continue;  // Torn write detected
        }

        // ═══════════════════════════════════════════════════════════════════
        // STEP 3: Check If Record Is In Use
        // ═══════════════════════════════════════════════════════════════════
        if (!(frsh->Flags & FRH_IN_USE)) {
            continue;  // Deleted file, skip
        }

        // ═══════════════════════════════════════════════════════════════════
        // STEP 4: Determine Base Record (Handle Extensions)
        // ═══════════════════════════════════════════════════════════════════
        unsigned int base_frs;
        if (frsh->BaseFileRecordSegment != 0) {
            // Extension record - attributes belong to base
            base_frs = (unsigned int)(frsh->BaseFileRecordSegment & 0x0000FFFFFFFFFFFF);
        } else {
            // Base record
            base_frs = frs;
        }

        Record* target_record = &records[base_frs];

        // ═══════════════════════════════════════════════════════════════════
        // STEP 5: Iterate Attributes Until End Marker
        // ═══════════════════════════════════════════════════════════════════
        void* record_end = (char*)frsh + min(record_size, frsh->BytesInUse);

        for (ATTRIBUTE_RECORD_HEADER* attr = frsh->begin();
             attr < record_end;
             attr = attr->next())
        {
            // Check for end marker
            if (attr->Type == 0xFFFFFFFF) {  // AttributeEnd
                break;
            }

            // Check for invalid/corrupted
            if (attr->Type == 0 || attr->Length == 0) {
                break;
            }

            // ═══════════════════════════════════════════════════════════════
            // STEP 6: Process Each Attribute Type
            // ═══════════════════════════════════════════════════════════════
            switch (attr->Type) {
                case 0x10:  // $STANDARD_INFORMATION
                    parse_standard_info(attr, target_record);
                    break;

                case 0x30:  // $FILE_NAME
                    parse_filename(attr, target_record, frs);
                    break;

                case 0x80:  // $DATA
                case 0x90:  // $INDEX_ROOT
                case 0xA0:  // $INDEX_ALLOCATION
                case 0xB0:  // $BITMAP
                    parse_stream(attr, target_record);
                    break;

                case 0xC0:  // $REPARSE_POINT
                    parse_reparse(attr, target_record);
                    break;

                // Other attributes can be ignored for file search purposes
            }
        }
    }
}
```

---

## Summary

### Key Points

1. **Fixed-Size Records**: MFT records are fixed-size (typically 1024 bytes), contiguous, with NO delimiters between them.

2. **Record Termination**: Attributes within a record end with `0xFFFFFFFF` (AttributeEnd marker).

3. **USA Fixup**: Critical for data integrity - must be applied before reading any data.

4. **Extension Records**: When a file's attributes don't fit in one record, extension records are used. They point back to the base record via `BaseFileRecordSegment`.

5. **$ATTRIBUTE_LIST**: Maps attributes to their containing FRS. UFFS doesn't need to parse this because it scans all records and uses `BaseFileRecordSegment` to redirect.

### What UFFS Extracts

| From | Data Extracted |
|------|----------------|
| **Record Header** | FRS number, sequence number, flags (in-use, directory) |
| **$STANDARD_INFORMATION** | Created/Modified/Accessed times, file attributes |
| **$FILE_NAME** | Filename, parent FRS, namespace (skip DOS names) |
| **$DATA** | Stream name, logical size, allocated size |
| **Extension Records** | Merged into base record automatically |

### Record Size Calculation

```cpp
// From boot sector
unsigned int record_size = boot_sector.ClustersPerFileRecordSegment >= 0
    ? boot_sector.ClustersPerFileRecordSegment * cluster_size
    : 1U << (-boot_sector.ClustersPerFileRecordSegment);

// Typical values:
// ClustersPerFileRecordSegment = -10 → record_size = 1024 bytes
// ClustersPerFileRecordSegment = -12 → record_size = 4096 bytes
```

---

## References

- [Microsoft NTFS Documentation](https://docs.microsoft.com/en-us/windows/win32/fileio/master-file-table)
- [NTFS-3G Project](https://github.com/tuxera/ntfs-3g)
- [The NTFS Documentation Project](https://flatcap.github.io/linux-ntfs/)
- UFFS Source: `src/core/ntfs_types.hpp`, `src/index/ntfs_index.hpp`

