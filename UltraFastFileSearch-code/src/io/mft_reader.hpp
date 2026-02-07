/**
 * @file mft_reader.hpp
 * @brief Asynchronous NTFS MFT (Master File Table) reader using I/O Completion Ports.
 *
 * This is the main header for MFT reading. It includes:
 * - mft_reader_constants.hpp: I/O sizing and lookup tables
 * - mft_reader_types.hpp: ChunkDescriptor and type aliases
 * - bitmap_utils.hpp: Popcount and bit scanning functions
 *
 * @see mft_reader_constants.hpp for configuration constants
 * @see mft_reader_types.hpp for data structures
 * @see bitmap_utils.hpp for bitmap manipulation utilities
 */

#ifndef UFFS_MFT_READER_HPP
#define UFFS_MFT_READER_HPP

// ============================================================================
// INCLUDES
// ============================================================================

// MFT reader components (split for single-responsibility)
#include "mft_reader_constants.hpp"
#include "mft_reader_types.hpp"
#include "bitmap_utils.hpp"

// I/O infrastructure
#include "overlapped.hpp"
#include "io_completion_port.hpp"

// Utility headers
#include "../util/atomic_compat.hpp"
#include "../util/intrusive_ptr.hpp"
#include "../util/lock_ptr.hpp"
#include "../util/error_utils.hpp"
#include "../util/volume_utils.hpp"
#include "../util/handle.hpp"

// Standard library
#include <vector>
#include <ctime>
#include <algorithm>
#include <stdexcept>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class NtfsIndex;

// ============================================================================
// CLASS: OverlappedNtfsMftReadPayload
// ============================================================================

/**
 * @class OverlappedNtfsMftReadPayload
 * @brief Orchestrates asynchronous reading of the NTFS MFT using IOCP.
 *
 * This class manages the complete MFT reading pipeline:
 * 1. Initialization: Query volume info, build extent maps
 * 2. Phase 1: Read $MFT::$BITMAP chunks in parallel
 * 3. Sync Point: Calculate skip ranges when all bitmap chunks complete
 * 4. Phase 2: Read $MFT::$DATA chunks in parallel (with skip optimization)
 *
 * ## Usage Example
 * ```cpp
 * IoCompletionPort iocp;
 * intrusive_ptr<NtfsIndex> index(new NtfsIndex(L"C:\\"));
 * Handle closing_event;
 *
 * intrusive_ptr<OverlappedNtfsMftReadPayload> payload(
 *     new OverlappedNtfsMftReadPayload(iocp, index, closing_event));
 *
 * // Post to IOCP to start the pipeline
 * iocp.post(0, 0, payload);
 *
 * // Wait for completion
 * WaitForSingleObject(index->finished_event(), INFINITE);
 * ```
 *
 * ## Memory Model
 *
 * The class uses atomic operations for thread-safe coordination:
 * - `next_bitmap_chunk_index_`: Tracks which bitmap chunk to read next
 * - `bitmap_chunks_remaining_`: Counts down to trigger skip calculation
 * - `next_data_chunk_index_`: Tracks which data chunk to read next
 * - `total_valid_records_`: Accumulates count for pre-allocation
 *
 * @note Inherits from Overlapped for IOCP integration
 */
class OverlappedNtfsMftReadPayload : public Overlapped
{
public:
    // ========================================================================
    // TYPE ALIASES (from mft_reader_types.hpp)
    // ========================================================================

    using ChunkDescriptor = mft_reader_types::ChunkDescriptor;
    using ChunkList = mft_reader_types::ChunkList;
    using MftBitmap = mft_reader_types::MftBitmap;

    // Legacy aliases for backward compatibility
    using RetPtr = ChunkDescriptor;
    using RetPtrs = ChunkList;
    using Bitmap = MftBitmap;

protected:
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    /// I/O Completion Port for async operations.
    IoCompletionPort volatile* iocp_;

    /// Event signaled when reading should be cancelled.
    Handle closing_event_;

    /// Chunks for reading $MFT::$BITMAP.
    ChunkList bitmap_chunks_;

    /// Chunks for reading $MFT::$DATA.
    ChunkList data_chunks_;

    /// Bytes per cluster on this volume (typically 4096).
    unsigned int cluster_size_;

    /// Maximum bytes per I/O operation (default: 1 MB).
    unsigned long long max_read_size_;

    /// Index of next bitmap chunk to read (atomic for thread safety).
    atomic_namespace::atomic<ChunkList::size_type> next_bitmap_chunk_index_;

    /// Number of bitmap chunks still being processed.
    /// When this reaches 0, we calculate skip ranges for data chunks.
    atomic_namespace::atomic<ChunkList::size_type> bitmap_chunks_remaining_;

    /// Index of next data chunk to read (atomic for thread safety).
    atomic_namespace::atomic<ChunkList::size_type> next_data_chunk_index_;

    /// Running count of valid (in-use) MFT records found in bitmap.
    /// Used to pre-allocate the index for better performance.
    atomic_namespace::atomic<unsigned int> total_valid_records_;

    /// The MFT bitmap data (one bit per record).
    /// Initialized to all 1s so we read everything if bitmap is unavailable.
    MftBitmap mft_bitmap_;

    /// The index being built from MFT records.
    intrusive_ptr<NtfsIndex volatile> index_;

    // Legacy member name aliases for backward compatibility
    IoCompletionPort volatile*& iocp = iocp_;
    Handle& closing_event = closing_event_;
    ChunkList& bitmap_ret_ptrs = bitmap_chunks_;
    ChunkList& data_ret_ptrs = data_chunks_;
    unsigned int& cluster_size = cluster_size_;
    unsigned long long& read_block_size = max_read_size_;
    atomic_namespace::atomic<ChunkList::size_type>& jbitmap = next_bitmap_chunk_index_;
    atomic_namespace::atomic<ChunkList::size_type>& nbitmap_chunks_left = bitmap_chunks_remaining_;
    atomic_namespace::atomic<ChunkList::size_type>& jdata = next_data_chunk_index_;
    atomic_namespace::atomic<unsigned int>& valid_records = total_valid_records_;
    MftBitmap& mft_bitmap = mft_bitmap_;
    intrusive_ptr<NtfsIndex volatile>& p = index_;

public:
    // Forward declaration of nested class
    class ReadOperation;

    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================

    /**
     * @brief Constructs an MFT reader payload.
     *
     * @param iocp           The I/O Completion Port for async operations
     * @param index          The NtfsIndex to populate with MFT records
     * @param closing_event  Event to signal cancellation (optional)
     */
    OverlappedNtfsMftReadPayload(
        IoCompletionPort volatile& iocp,
        intrusive_ptr<NtfsIndex volatile> index,
        Handle const& closing_event)
        : Overlapped()
        , iocp_(&iocp)
        , closing_event_(closing_event)
        , cluster_size_(0)
        , max_read_size_(mft_reader_constants::kDefaultReadBlockSize)
        , next_bitmap_chunk_index_(0)
        , bitmap_chunks_remaining_(0)
        , next_data_chunk_index_(0)
        , total_valid_records_(0)
    {
        using std::swap;
        swap(index, index_);
    }

    /// Virtual destructor for proper cleanup in derived classes.
    ~OverlappedNtfsMftReadPayload() override = default;

    // ========================================================================
    // PUBLIC INTERFACE
    // ========================================================================

    /**
     * @brief Issues the next pending I/O operation.
     *
     * This method is called:
     * 1. Initially to start the pipeline (called twice for concurrency)
     * 2. After each I/O completion to maintain the pipeline
     *
     * Priority: Bitmap chunks are read before data chunks.
     */
    void queue_next() volatile;

    /**
     * @brief IOCP completion handler - initializes the reading pipeline.
     *
     * Called when this payload is posted to the IOCP. Performs:
     * 1. Volume initialization (query NTFS info)
     * 2. Extent map retrieval for $BITMAP and $DATA
     * 3. Chunk list creation
     * 4. Initial I/O operations
     *
     * @param size  Unused (required by Overlapped interface)
     * @param key   Unused (required by Overlapped interface)
     * @return -1 to indicate this operation is complete
     */
    int operator()(size_t /*size*/, uintptr_t /*key*/) override;

    /**
     * @brief Hook for derived classes to perform pre-open operations.
     * Called before volume I/O begins.
     */
    virtual void preopen() {}

private:
    // ========================================================================
    // PRIVATE HELPER METHODS (implemented after class definition)
    // ========================================================================

    /// Attempts to issue a bitmap chunk read operation.
    bool try_issue_bitmap_read(const OverlappedNtfsMftReadPayload* self) volatile;

    /// Attempts to issue a data chunk read operation.
    bool try_issue_data_read(const OverlappedNtfsMftReadPayload* self) volatile;

    /// Issues an async read operation for a chunk.
    void issue_chunk_read(const OverlappedNtfsMftReadPayload* self,
                          const ChunkDescriptor& chunk,
                          bool is_bitmap) volatile;

    /// Initializes volume information from NTFS query result.
    void initialize_volume_info(NtfsIndex* index, const NTFS_VOLUME_DATA_BUFFER& volume_info);

    /// Builds the chunk list for reading $MFT::$BITMAP.
    void build_bitmap_chunk_list(NtfsIndex* index, const NTFS_VOLUME_DATA_BUFFER& volume_info);

    /// Builds the chunk list for reading $MFT::$DATA.
    void build_data_chunk_list(NtfsIndex* index, const NTFS_VOLUME_DATA_BUFFER& volume_info);

    /// Converts an extent map to a list of I/O chunks.
    void build_chunk_list_from_extents(
        const std::vector<std::pair<unsigned long long, long long>>& extents,
        ChunkList& chunks);
};

// ============================================================================
// IMPLEMENTATION INCLUDE
// ============================================================================

// For header-only usage, pull in the implementation details.
// Consumers should include only this header.
#include "mft_reader_impl.hpp"

#endif // UFFS_MFT_READER_HPP