#pragma once

// ============================================================================
// NtfsIndex Class Documentation
// ============================================================================
// This file documents the NtfsIndex class - the core NTFS file index.
// 
// NOTE: The actual NtfsIndex class is NOT extracted yet because it depends on:
//   - RefCounted<NtfsIndex> (which depends on atomic_namespace)
//   - atomic_namespace::recursive_mutex, atomic_namespace::atomic
//   - intrusive_ptr infrastructure
//   - Various utility templates (vector_with_fast_size, value_initialized, etc.)
//
// The class remains in UltraFastFileSearch.cpp (lines 3613-5573, ~1960 lines)
// until the atomic_namespace and RefCounted dependencies are resolved.
//
// This header is provided for documentation and Rust team reference.
// ============================================================================

namespace uffs {

// ============================================================================
// NtfsIndex Class Overview
// ============================================================================
// 
// NtfsIndex is the core class that holds the parsed MFT data in memory.
// It provides efficient storage and lookup of file/directory information.
//
// Key Features:
// - Reference counted via RefCounted<NtfsIndex>
// - Thread-safe via atomic_namespace::recursive_mutex
// - Compact memory representation using packed structures
// - Supports multiple hardlinks per file
// - Supports multiple data streams per file
// - Hierarchical parent-child relationships
//
// Memory Layout:
// - Uses packed structures (#pragma pack(push, 1)) for minimal memory
// - file_size_type: 6 bytes (48-bit file sizes, supports up to 256 TB)
// - StandardInfo: File attributes and timestamps
// - SizeInfo: File size, allocated size, bulkiness
// - NameInfo: Filename offset and length
// - LinkInfo: Hardlink information
// - StreamInfo: Data stream information (for ADS)
// - Record: Complete file record combining all info
//
// Data Structures:
// - records_data: Vector of Record structs (one per MFT record)
// - records_lookup: Maps MFT record number to records_data index
// - nameinfos: Vector of LinkInfo (hardlinks)
// - streaminfos: Vector of StreamInfo (data streams)
// - childinfos: Vector of ChildInfo (directory children)
// - names: Concatenated filename storage (ASCII/Unicode)
//
// Key Types:
// - key_type: Identifies a specific file/stream combination
//   - frs: File Record Segment number (MFT record number)
//   - name_info: Which hardlink (0-1023)
//   - stream_info: Which data stream (0-4095)
//   - index: Additional index for disambiguation
//
// Public Interface:
// - root_path(): Get the volume root path (e.g., "C:")
// - total_records(): Total number of MFT records
// - records_so_far(): Records processed so far (for progress)
// - finished(): Whether indexing is complete
// - cancelled(): Whether indexing was cancelled
// - cancel(): Request cancellation
// - wait(): Wait for indexing to complete
// - find(): Find files matching a pattern
// - get_path(): Get full path for a file record
// - get_name(): Get filename for a record
// - get_size(): Get file size
// - get_attributes(): Get file attributes
//
// Threading Model:
// - Indexing runs on background thread
// - Progress can be queried from any thread
// - Searches can run concurrently with indexing
// - Uses recursive_mutex for thread safety
//
// Performance Characteristics:
// - Memory: ~100-200 bytes per file (varies with name length)
// - Indexing: ~1-3 seconds for 1M files (SSD)
// - Search: Sub-millisecond for most queries
//
// Location in UltraFastFileSearch.cpp: Lines 3613-5573
// ============================================================================

// ============================================================================
// Key Nested Types (for Rust FFI reference)
// ============================================================================

// file_size_type: 6-byte file size (48 bits, max 256 TB)
// struct file_size_type {
//     uint32_t low;
//     uint16_t high;
// };

// StandardInfo: File attributes and timestamps
// struct StandardInfo {
//     uint64_t created;      // FILETIME
//     uint64_t written;      // FILETIME  
//     uint64_t accessed : 58;// FILETIME (58 bits)
//     uint64_t is_readonly : 1;
//     uint64_t is_archive : 1;
//     // ... more attribute flags
// };

// SizeInfo: File size information
// struct SizeInfo {
//     file_size_type length;     // Logical file size
//     file_size_type allocated;  // Allocated size on disk
//     file_size_type bulkiness;  // Size including slack
//     uint32_t treesize;         // Size of directory tree
// };

// NameInfo: Filename reference
// struct NameInfo {
//     uint32_t _offset;  // Offset into names buffer (bit 0 = ASCII flag)
//     uint8_t length;    // Filename length in characters
// };

// LinkInfo: Hardlink information
// struct LinkInfo {
//     uint32_t next_entry;  // Next hardlink for same file
//     NameInfo name;        // Filename
//     uint32_t parent;      // Parent directory MFT record
// };

// StreamInfo: Data stream information (for ADS)
// struct StreamInfo : SizeInfo {
//     uint32_t next_entry;  // Next stream for same file
//     NameInfo name;        // Stream name (empty for default)
//     uint8_t type_name_id; // Stream type
// };

// Record: Complete file record
// struct Record {
//     StandardInfo stdinfo;
//     uint16_t name_count;    // Number of hardlinks
//     uint16_t stream_count;  // Number of data streams
//     uint32_t first_child;   // First child (if directory)
//     LinkInfo first_name;    // First hardlink
//     StreamInfo first_stream;// First data stream
// };

} // namespace uffs

