/**
 * @file mft_reader_impl.hpp
 * @brief Orchestrator for MFT reader implementation components.
 *
 * This file serves as the central orchestrator that includes all MFT reader
 * implementation components. The implementation has been split into focused
 * files for better maintainability and documentation.
 *
 * ## Architecture Overview
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                        MFT READER ARCHITECTURE                               │
 * │                                                                              │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │                    mft_reader.hpp (Public API)                       │    │
 * │  │  - OverlappedNtfsMftReadPayload class declaration                   │    │
 * │  │  - Public interface for MFT reading                                 │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                              │                                               │
 * │                              ▼                                               │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │                mft_reader_impl.hpp (This Orchestrator)               │    │
 * │  │  - Includes all implementation components                           │    │
 * │  │  - Defines include order for proper compilation                     │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                              │                                               │
 * │              ┌───────────────┼───────────────┐                               │
 * │              ▼               ▼               ▼                               │
 * │  ┌──────────────────┐ ┌──────────────┐ ┌──────────────┐                      │
 * │  │ read_operation   │ │  pipeline    │ │    init      │                      │
 * │  │ (~600 lines)     │ │ (~270 lines) │ │ (~280 lines) │                      │
 * │  │                  │ │              │ │              │                      │
 * │  │ - ReadOperation  │ │ - queue_next │ │ - operator() │                      │
 * │  │   nested class   │ │ - try_issue  │ │ - init_vol   │                      │
 * │  │ - Memory recycle │ │ - issue_chunk│ │ - build_*    │                      │
 * │  │ - Completion     │ │              │ │              │                      │
 * │  └──────────────────┘ └──────────────┘ └──────────────┘                      │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Component Files
 *
 * | File                          | Lines | Purpose                              |
 * |-------------------------------|-------|--------------------------------------|
 * | mft_reader_read_operation.hpp | ~600  | ReadOperation class, memory recycling|
 * | mft_reader_pipeline.hpp       | ~270  | Pipeline management, I/O scheduling  |
 * | mft_reader_init.hpp           | ~280  | Initialization, chunk list building  |
 *
 * ## Data Flow
 *
 * 1. **Initialization** (mft_reader_init.hpp)
 *    - operator() is called when payload is posted to IOCP
 *    - Queries NTFS volume data
 *    - Builds chunk lists for $BITMAP and $DATA
 *    - Starts I/O pipeline
 *
 * 2. **Pipeline Management** (mft_reader_pipeline.hpp)
 *    - queue_next() maintains concurrent I/O operations
 *    - try_issue_bitmap_read() / try_issue_data_read() select next chunk
 *    - issue_chunk_read() performs the actual async I/O
 *
 * 3. **Completion Handling** (mft_reader_read_operation.hpp)
 *    - ReadOperation::operator() handles I/O completion
 *    - Processes bitmap chunks (determines which data to skip)
 *    - Processes data chunks (parses MFT records)
 *    - Recycles memory for next operation
 *
 * @note This file is included at the end of mft_reader.hpp.
 *       Do not include this file directly.
 *
 * @see mft_reader.hpp for the public API
 * @see mft_reader_read_operation.hpp for ReadOperation class
 * @see mft_reader_pipeline.hpp for pipeline management
 * @see mft_reader_init.hpp for initialization logic
 */

#ifndef UFFS_MFT_READER_IMPL_HPP
#define UFFS_MFT_READER_IMPL_HPP

// This file should only be included from mft_reader.hpp
#ifndef UFFS_MFT_READER_HPP
#error "Do not include mft_reader_impl.hpp directly. Include mft_reader.hpp instead."
#endif

// ============================================================================
// COMPONENT INCLUDES
// ============================================================================
//
// The order of includes is important:
// 1. read_operation - Defines ReadOperation class (used by pipeline)
// 2. pipeline - Defines queue_next() (used by init and read_operation)
// 3. init - Defines operator() (entry point, uses pipeline)
//

#include "mft_reader_read_operation.hpp"
#include "mft_reader_pipeline.hpp"
#include "mft_reader_init.hpp"

#endif // UFFS_MFT_READER_IMPL_HPP
