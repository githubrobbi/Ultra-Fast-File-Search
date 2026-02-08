/**
 * @file ntfs_index_impl.hpp
 * @brief Orchestrator for NTFS file system index implementation.
 *
 * This file serves as the central orchestrator that includes all the
 * implementation components for the NtfsIndex class. The implementation
 * has been split into focused, well-documented files for maintainability.
 *
 * ## Architecture Overview
 *
 * The NtfsIndex class maintains an in-memory representation of the NTFS
 * Master File Table (MFT). The index is built in two phases:
 *
 * 1. **Loading Phase** (preload_concurrent + load):
 *    - Pre-scan MFT records to determine capacity and pre-allocate vectors
 *    - Parse each MFT record, extracting attributes:
 *      - StandardInformation: timestamps, file attributes
 *      - FileName: file/directory names, parent references
 *      - Data: file sizes, stream information
 *    - Build parent-child relationships for directory traversal
 *
 * 2. **Preprocessing Phase** (Preprocessor struct):
 *    - Traverse the directory tree from root (FRS 5)
 *    - Calculate cumulative sizes (treesize, bulkiness) for directories
 *    - Handle compression reparse points (WofCompressedData)
 *
 * ## Data Structure Relationships
 *
 * ```
 * records_lookup[FRS] ──► records_data[slot] ──► Record
 *                                                  │
 *                         ┌────────────────────────┼────────────────────────┐
 *                         ▼                        ▼                        ▼
 *                    first_name              first_stream             first_child
 *                    (LinkInfo)              (StreamInfo)             (ChildInfo)
 *                         │                        │                        │
 *                         ▼                        ▼                        ▼
 *                    nameinfos[]             streaminfos[]            childinfos[]
 *                    (linked list)           (linked list)            (linked list)
 *                         │                        │
 *                         ▼                        ▼
 *                    names[] ◄─────────────────────┘
 *                    (character storage)
 * ```
 *
 * ## Implementation Files
 *
 * This orchestrator includes the following implementation files:
 *
 * | File                        | Content                                    |
 * |-----------------------------|--------------------------------------------|
 * | ntfs_index_accessors.hpp    | Constructor, destructor, accessors         |
 * | ntfs_index_load.hpp         | preload_concurrent(), load(), Preprocessor |
 * | ntfs_index_path.hpp         | ParentIterator - path reconstruction       |
 * | ntfs_index_matcher.hpp      | Matcher template - pattern matching        |
 *
 * ## Key Concepts
 *
 * - **FRS (File Record Segment)**: Unique identifier for each MFT record
 * - **Base FRS**: For files with multiple MFT records, the primary record
 * - **LinkInfo**: Represents a hard link (file name + parent directory)
 * - **StreamInfo**: Represents a data stream (default or named)
 * - **ChildInfo**: Links a parent directory to its children
 *
 * ## Special FRS Values
 *
 * - kRootFRS (0x05): Root directory ($)
 * - kVolumeFRS (0x06): $Volume metadata file
 * - kFirstUserFRS (0x10): First user file (FRS 0-15 are system metadata)
 *
 * @note This file is included at the end of ntfs_index.hpp.
 *       Do not include this file directly.
 *
 * @see ntfs_index.hpp for the public API and class declaration
 * @see mft_reader.hpp for the MFT reading infrastructure
 */

#ifndef UFFS_NTFS_INDEX_IMPL_HPP
#define UFFS_NTFS_INDEX_IMPL_HPP

#ifndef UFFS_NTFS_INDEX_HPP
#error "Do not include ntfs_index_impl.hpp directly. Include ntfs_index.hpp instead."
#endif

// ============================================================================
// SECTION: Implementation Component Includes
// ============================================================================
// The implementation is split into focused files for maintainability.
// Include order matters: accessors first, then load (contains inline
// Preprocessor struct), then path and matcher.

// Accessors, constructor, destructor, lifecycle management
#include "ntfs_index_accessors.hpp"

// Core MFT parsing: preload_concurrent() and load()
// Note: The Preprocessor struct for directory size calculation is defined
// inline within the load() function as an anonymous local struct.
#include "ntfs_index_load.hpp"

// Path reconstruction via ParentIterator state machine
#include "ntfs_index_path.hpp"

// Pattern matching via Matcher template
#include "ntfs_index_matcher.hpp"

#endif // UFFS_NTFS_INDEX_IMPL_HPP

