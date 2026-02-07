/**
 * @file mft_reader.hpp
 * @brief Asynchronous NTFS MFT (Master File Table) reader using I/O Completion Ports
 *
 * @details
 * This file implements high-performance asynchronous reading of the NTFS Master File
 * Table (MFT). The MFT is the central data structure of NTFS, containing one record
 * for every file and directory on the volume.
 *
 * ## Architecture Overview
 *
 * The reader uses a two-phase pipeline with Windows I/O Completion Ports (IOCP):
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                         MFT Reading Pipeline                            │
 * │                                                                         │
 * │  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐            │
 * │  │   Phase 1    │     │   Sync       │     │   Phase 2    │            │
 * │  │ Read $BITMAP │────▶│   Point      │────▶│ Read $DATA   │            │
 * │  │  (parallel)  │     │              │     │  (parallel)  │            │
 * │  └──────────────┘     └──────────────┘     └──────────────┘            │
 * │         │                    │                    │                     │
 * │         ▼                    ▼                    ▼                     │
 * │  Count valid records   Calculate skip      Parse MFT records           │
 * │  in each chunk         ranges for each     and build index             │
 * │                        data chunk                                       │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Why Two Phases?
 *
 * 1. **$MFT::$BITMAP** - One bit per MFT record (1 = in-use, 0 = free)
 *    - Reading this first tells us which records are active
 *    - Allows us to skip reading unused regions of $DATA
 *
 * 2. **$MFT::$DATA** - The actual MFT records (typically 1024 bytes each)
 *    - Can be gigabytes in size
 *    - Often fragmented across the disk
 *    - Skip optimization can save significant I/O
 *
 * ## Key Concepts
 *
 * - **VCN (Virtual Cluster Number)**: Logical position within a file
 * - **LCN (Logical Cluster Number)**: Physical position on disk
 * - **Extent**: A contiguous run of clusters (VCN range → LCN mapping)
 * - **Chunk**: A portion of an extent sized for efficient I/O (~1 MB)
 * - **Skip Range**: Clusters at chunk start/end with no in-use records
 *
 * ## Thread Safety
 *
 * - Multiple I/O operations run concurrently via IOCP
 * - Bitmap processing uses atomic operations for counters
 * - Data chunk parsing uses lock() for index updates
 *
 * @see NtfsIndex for the index that receives parsed records
 * @see IoCompletionPort for the async I/O infrastructure
 *
 * @note This is a header-only implementation using inline functions.
 */

#ifndef UFFS_MFT_READER_HPP
#define UFFS_MFT_READER_HPP

// ============================================================================
// INCLUDES
// ============================================================================

// I/O infrastructure
#include "overlapped.hpp"
#include "io_completion_port.hpp"

// Utility headers
#include "../util/atomic_compat.hpp"
#include "../util/intrusive_ptr.hpp"
#include "../util/lock_ptr.hpp"
#include "../util/error_utils.hpp"      // CheckAndThrow, CppRaiseException, CStructured_Exception
#include "../util/volume_utils.hpp"     // get_retrieval_pointers
#include "../util/handle.hpp"           // Handle class

// Standard library
#include <vector>
#include <ctime>
#include <climits>
#include <algorithm>
#include <stdexcept>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

/// Forward declaration for NtfsIndex (defined in ntfs_index.hpp)
/// We only need forward declaration here since we use pointers/references
class NtfsIndex;

// ============================================================================
// CONSTANTS
// ============================================================================

namespace mft_reader_constants {

/// Default maximum bytes to read in a single I/O operation.
/// 1 MB is a good balance between:
/// - Large enough to amortize I/O overhead
/// - Small enough to maintain concurrency
/// - Aligned with typical disk cache sizes
static constexpr unsigned long long kDefaultReadBlockSize = 1ULL << 20;  // 1 MB

/// Number of concurrent I/O operations to maintain.
/// Two operations allows one to be in-flight while another completes.
/// Higher values may improve throughput on SSDs but increase memory usage.
static constexpr int kIoConcurrencyLevel = 2;

/// Number of bits per byte (platform-independent).
/// Used for bitmap calculations.
static constexpr size_t kBitsPerByte = CHAR_BIT;

/// Lookup table for counting set bits in a 4-bit nibble.
/// Used for fast population count when processing the MFT bitmap.
/// Index is the nibble value (0-15), result is the number of 1-bits.
static constexpr unsigned char kNibblePopCount[16] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

}  // namespace mft_reader_constants

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
protected:
    // ========================================================================
    // NESTED TYPE: ChunkDescriptor (formerly RetPtr)
    // ========================================================================

    /**
     * @struct ChunkDescriptor
     * @brief Describes a chunk of the MFT to be read from disk.
     *
     * Each chunk represents a contiguous region of clusters that will be
     * read in a single I/O operation. The chunk maps a virtual position
     * (VCN) to a physical position (LCN) on disk.
     *
     * ## Skip Optimization
     *
     * After the bitmap is read, we calculate how many clusters at the
     * beginning and end of each chunk contain only unused MFT records.
     * These clusters can be skipped to reduce I/O.
     *
     * ```
     * Chunk: [====USED====----FREE----]
     *         ^                       ^
     *         |                       |
     *         skip_begin=0            skip_end=4 clusters
     * ```
     */
    struct ChunkDescriptor
    {
        /// Virtual Cluster Number - logical position within the MFT file.
        /// Used to calculate which MFT records this chunk contains.
        unsigned long long vcn;

        /// Number of clusters in this chunk (before skip optimization).
        unsigned long long cluster_count;

        /// Logical Cluster Number - physical position on disk.
        /// This is where the data actually resides.
        long long lcn;

        /// Number of clusters to skip at the START of this chunk.
        /// These clusters contain only unused MFT records.
        /// Atomic because it's written by bitmap processing and read by data reading.
        atomic_namespace::atomic<unsigned long long> skip_begin;

        /// Number of clusters to skip at the END of this chunk.
        /// These clusters contain only unused MFT records.
        atomic_namespace::atomic<unsigned long long> skip_end;

        /// Constructs a chunk descriptor with no skip optimization.
        ChunkDescriptor(
            unsigned long long vcn_,
            unsigned long long cluster_count_,
            long long lcn_)
            : vcn(vcn_)
            , cluster_count(cluster_count_)
            , lcn(lcn_)
            , skip_begin(0)
            , skip_end(0)
        {}

        /// Copy constructor (required because of atomic members).
        ChunkDescriptor(const ChunkDescriptor& other)
            : vcn(other.vcn)
            , cluster_count(other.cluster_count)
            , lcn(other.lcn)
            , skip_begin(other.skip_begin.load(atomic_namespace::memory_order_relaxed))
            , skip_end(other.skip_end.load(atomic_namespace::memory_order_relaxed))
        {}

        /// Copy assignment (required because of atomic members).
        ChunkDescriptor& operator=(const ChunkDescriptor& other)
        {
            vcn = other.vcn;
            cluster_count = other.cluster_count;
            lcn = other.lcn;
            skip_begin.store(other.skip_begin.load(atomic_namespace::memory_order_relaxed),
                             atomic_namespace::memory_order_relaxed);
            skip_end.store(other.skip_end.load(atomic_namespace::memory_order_relaxed),
                           atomic_namespace::memory_order_relaxed);
            return *this;
        }
    };

    // Legacy typedef for backward compatibility
    using RetPtr = ChunkDescriptor;

    // ========================================================================
    // TYPE ALIASES
    // ========================================================================

    /// List of chunks to read (for bitmap or data).
    using ChunkList = std::vector<ChunkDescriptor>;
    using RetPtrs = ChunkList;  // Legacy alias

    /// MFT bitmap: one bit per record (1 = in-use, 0 = free).
    using MftBitmap = std::vector<unsigned char>;
    using Bitmap = MftBitmap;  // Legacy alias

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
// CLASS: ReadOperation (nested in OverlappedNtfsMftReadPayload)
// ============================================================================

/**
 * @class OverlappedNtfsMftReadPayload::ReadOperation
 * @brief Represents a single async I/O operation for reading MFT chunks.
 *
 * Each ReadOperation handles one chunk of either $BITMAP or $DATA.
 * The operation includes:
 * - The I/O buffer (allocated immediately after the object)
 * - Metadata about the chunk (virtual offset, skip ranges)
 * - Completion handling logic
 *
 * ## Memory Layout
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────┐
 * │  ReadOperation object  │  I/O Buffer (variable size)       │
 * │  (fixed size)          │  (allocated via placement new)    │
 * └─────────────────────────────────────────────────────────────┘
 *                          ^
 *                          |
 *                          this + 1 (buffer pointer)
 * ```
 *
 * ## Memory Recycling
 *
 * To avoid allocation overhead during high-frequency I/O, completed
 * operations are recycled rather than freed. The recycled pool is
 * searched for a suitable buffer on each allocation.
 *
 * ## Completion Flow
 *
 * When I/O completes, operator() is called:
 * 1. Queue the next I/O operation (maintain pipeline)
 * 2. Process the completed data:
 *    - Bitmap: Count valid records, copy to master bitmap
 *    - Data: Parse MFT records, update index
 * 3. Report speed statistics
 */
class OverlappedNtfsMftReadPayload::ReadOperation : public Overlapped
{
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================

    /// Virtual offset in the MFT file (bytes).
    /// Used to calculate FRS (File Record Segment) numbers.
    unsigned long long virtual_offset_;

    /// Bytes skipped at the start of this chunk (due to unused records).
    unsigned long long skipped_bytes_begin_;

    /// Bytes skipped at the end of this chunk (due to unused records).
    unsigned long long skipped_bytes_end_;

    /// Timestamp when this operation was issued (for speed calculation).
    clock_t issue_time_;

    /// True if this is a bitmap chunk, false if data chunk.
    bool is_bitmap_chunk_;

    /// Back-pointer to the parent payload (for accessing shared state).
    intrusive_ptr<OverlappedNtfsMftReadPayload volatile> parent_;

    // Legacy member name aliases
    unsigned long long& _voffset = virtual_offset_;
    unsigned long long& _skipped_begin = skipped_bytes_begin_;
    unsigned long long& _skipped_end = skipped_bytes_end_;
    clock_t& _time = issue_time_;
    bool& _is_bitmap = is_bitmap_chunk_;
    intrusive_ptr<OverlappedNtfsMftReadPayload volatile>& q = parent_;

    // ========================================================================
    // MEMORY RECYCLING (Static)
    // ========================================================================

    /// Mutex protecting the recycled buffer pool.
    static atomic_namespace::recursive_mutex recycled_mutex;

    /// Pool of recycled buffers: (size, pointer) pairs.
    /// Buffers are reused to avoid allocation overhead.
    static std::vector<std::pair<size_t, void*>> recycled;

    /**
     * @brief Custom allocator that reuses buffers from the recycled pool.
     *
     * Algorithm:
     * 1. Search for smallest buffer that fits the request
     * 2. If found, remove from pool and return
     * 3. If not found, allocate new buffer with malloc
     *
     * @param n  Number of bytes needed
     * @return Pointer to allocated memory
     */
    static void* operator new(size_t n)
    {
        void* ptr = nullptr;

        // Try to find a suitable recycled buffer
        {
            atomic_namespace::unique_lock<atomic_namespace::recursive_mutex> guard(recycled_mutex);

            size_t best_fit_index = recycled.size();  // Invalid index = not found

            // Linear search for smallest buffer >= n
            for (size_t i = 0; i < recycled.size(); ++i)
            {
                const size_t buffer_size = recycled[i].first;
                const bool fits = (buffer_size >= n);
                const bool is_better = (best_fit_index >= recycled.size()) ||
                                       (buffer_size <= recycled[best_fit_index].first);

                if (fits && is_better)
                {
                    best_fit_index = i;
                }
            }

            // Use recycled buffer if found
            if (best_fit_index < recycled.size())
            {
                ptr = recycled[best_fit_index].second;
                recycled.erase(recycled.begin() + static_cast<ptrdiff_t>(best_fit_index));
            }
        }

        // Allocate new buffer if no suitable recycled buffer found
        // Using malloc() so we can query size with _msize() later
        if (!ptr)
        {
            ptr = malloc(n);
        }

        return ptr;
    }

public:
    /**
     * @brief Placement new for allocating object + I/O buffer together.
     *
     * @param n  Size of ReadOperation object
     * @param m  Size of I/O buffer to append
     * @return Pointer to allocated memory
     */
    static void* operator new(size_t n, size_t m)
    {
        return operator new(n + m);
    }

    /**
     * @brief Custom deallocator that adds buffer to recycled pool.
     *
     * Instead of freeing memory, we save it for reuse.
     * This avoids allocation overhead during high-frequency I/O.
     *
     * @param ptr  Pointer to memory to recycle
     */
    static void operator delete(void* ptr)
    {
        atomic_namespace::unique_lock<atomic_namespace::recursive_mutex> guard(recycled_mutex);
        recycled.push_back(std::make_pair(_msize(ptr), ptr));
    }

    /// Placement delete (required to match placement new).
    static void operator delete(void* ptr, size_t /*m*/)
    {
        operator delete(ptr);
    }

    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    /**
     * @brief Constructs a read operation for a chunk.
     *
     * @param parent     The parent payload (for shared state access)
     * @param is_bitmap  True for bitmap chunk, false for data chunk
     */
    explicit ReadOperation(
        intrusive_ptr<OverlappedNtfsMftReadPayload volatile> const& parent,
        bool is_bitmap)
        : Overlapped()
        , virtual_offset_(0)
        , skipped_bytes_begin_(0)
        , skipped_bytes_end_(0)
        , issue_time_(clock())
        , is_bitmap_chunk_(is_bitmap)
        , parent_(parent)
    {}

    // ========================================================================
    // ACCESSORS
    // ========================================================================

    /// Gets the virtual offset (position in MFT file).
    [[nodiscard]] unsigned long long voffset() const noexcept
    {
        return virtual_offset_;
    }

    /// Sets the virtual offset.
    void voffset(unsigned long long value) noexcept
    {
        virtual_offset_ = value;
    }

    /// Gets the number of bytes skipped at chunk start.
    [[nodiscard]] unsigned long long skipped_begin() const noexcept
    {
        return skipped_bytes_begin_;
    }

    /// Sets the number of bytes skipped at chunk start.
    void skipped_begin(unsigned long long value) noexcept
    {
        skipped_bytes_begin_ = value;
    }

    /// Gets the number of bytes skipped at chunk end.
    [[nodiscard]] unsigned long long skipped_end() const noexcept
    {
        return skipped_bytes_end_;
    }

    /// Sets the number of bytes skipped at chunk end.
    void skipped_end(unsigned long long value) noexcept
    {
        skipped_bytes_end_ = value;
    }

    /// Gets the timestamp when this operation was issued.
    [[nodiscard]] clock_t time() const noexcept
    {
        return issue_time_;
    }

    /// Sets the issue timestamp.
    void time(clock_t value) noexcept
    {
        issue_time_ = value;
    }

    // ========================================================================
    // COMPLETION HANDLER
    // ========================================================================

    /**
     * @brief Handles I/O completion for this chunk.
     *
     * This is called by the IOCP when the async read completes.
     *
     * ## For Bitmap Chunks:
     * 1. Count valid (in-use) records using popcount
     * 2. Copy bitmap data to master bitmap
     * 3. If this is the LAST bitmap chunk:
     *    - Pre-allocate index based on total valid records
     *    - Calculate skip ranges for all data chunks
     *
     * ## For Data Chunks:
     * 1. Call preload_concurrent() for parallel pre-processing
     * 2. Call load() to parse records and update index
     *
     * @param size  Number of bytes read
     * @param key   Unused (IOCP completion key)
     * @return -1 to indicate this operation is complete
     */
    int operator()(size_t size, uintptr_t /*key*/) override
    {
        // Get non-volatile pointer to parent for easier access
        OverlappedNtfsMftReadPayload* const parent =
            const_cast<OverlappedNtfsMftReadPayload*>(
                static_cast<OverlappedNtfsMftReadPayload volatile*>(parent_.get()));

        // Check for cancellation before processing
        if (parent->index_->cancelled())
        {
            return -1;
        }

        // Maintain the I/O pipeline by queuing the next operation
        parent_->queue_next();

        // Get pointer to the I/O buffer (immediately after this object)
        void* const buffer = this + 1;

        if (is_bitmap_chunk_)
        {
            process_bitmap_chunk(parent, buffer, size);
        }
        else
        {
            process_data_chunk(parent, buffer, size);
        }

        // Report I/O speed statistics
        lock(parent->index_)->report_speed(size, issue_time_, clock());

        return -1;
    }

private:
    // ========================================================================
    // BITMAP PROCESSING HELPERS
    // ========================================================================

    /**
     * @brief Processes a completed bitmap chunk.
     *
     * Steps:
     * 1. Count valid records in this chunk (popcount)
     * 2. Copy bitmap data to master bitmap
     * 3. If last chunk, trigger skip range calculation
     *
     * @param parent  Parent payload (for shared state)
     * @param buffer  Raw bitmap data from disk
     * @param size    Number of bytes read
     */
    void process_bitmap_chunk(
        OverlappedNtfsMftReadPayload* parent,
        void* buffer,
        size_t size)
    {
        // Number of bits per bitmap byte (for indexing)
        constexpr size_t kBitsPerBitmapByte = mft_reader_constants::kBitsPerByte;

        // Check if this chunk is within valid MFT capacity
        const unsigned long long bitmap_bit_offset = voffset() * kBitsPerBitmapByte;
        if (bitmap_bit_offset > parent->index_->mft_capacity)
        {
            // This chunk is beyond MFT capacity, skip processing
            check_if_last_bitmap_chunk(parent);
            return;
        }

        // Calculate how many bytes to process (may be truncated at MFT end)
        size_t bytes_to_process = size;
        const unsigned long long max_bitmap_offset = parent->index_->mft_capacity / kBitsPerBitmapByte;
        if (voffset() + bytes_to_process > max_bitmap_offset)
        {
            bytes_to_process = static_cast<size_t>(max_bitmap_offset - voffset());
        }

        // Count valid records using nibble-based popcount
        const unsigned int valid_count = count_valid_records_in_buffer(buffer, bytes_to_process);

        // Copy bitmap data to master bitmap
        const auto* src = static_cast<const unsigned char*>(buffer);
        auto dest_offset = static_cast<ptrdiff_t>(voffset());
        std::copy(src, src + bytes_to_process, parent->mft_bitmap_.begin() + dest_offset);

        // Atomically add to total valid record count
        parent->total_valid_records_.fetch_add(valid_count, atomic_namespace::memory_order_acq_rel);

        // Check if this was the last bitmap chunk
        check_if_last_bitmap_chunk(parent);
    }

    /**
     * @brief Counts valid (in-use) MFT records in a bitmap buffer.
     *
     * Uses nibble-based popcount for efficiency:
     * - Split each byte into two 4-bit nibbles
     * - Look up popcount for each nibble in table
     * - Sum all popcounts
     *
     * @param buffer  Raw bitmap data
     * @param size    Number of bytes
     * @return Number of set bits (valid records)
     */
    [[nodiscard]] static unsigned int count_valid_records_in_buffer(
        const void* buffer,
        size_t size)
    {
        const auto* bytes = static_cast<const unsigned char*>(buffer);
        unsigned int count = 0;

        for (size_t i = 0; i < size; ++i)
        {
            const unsigned char byte = bytes[i];

            // Split byte into high and low nibbles
            const unsigned char low_nibble = byte & 0x0F;
            const unsigned char high_nibble = (byte >> 4) & 0x0F;

            // Look up popcount for each nibble
            count += mft_reader_constants::kNibblePopCount[low_nibble];
            count += mft_reader_constants::kNibblePopCount[high_nibble];
        }

        return count;
    }

    /**
     * @brief Checks if this was the last bitmap chunk and triggers skip calculation.
     *
     * When the last bitmap chunk completes:
     * 1. Pre-allocate index based on total valid records
     * 2. Calculate skip ranges for all data chunks
     *
     * @param parent  Parent payload (for shared state)
     */
    void check_if_last_bitmap_chunk(OverlappedNtfsMftReadPayload* parent)
    {
        // Atomically decrement remaining count; if we were the last one...
        if (parent->bitmap_chunks_remaining_.fetch_sub(1, atomic_namespace::memory_order_acq_rel) == 1)
        {
            // Exchange to 0 ensures this only happens once
            const unsigned int total_valid =
                parent->total_valid_records_.exchange(0, atomic_namespace::memory_order_acq_rel);

            // Pre-allocate index for better performance
            lock(parent->index_)->reserve(total_valid);

            // Calculate skip ranges for all data chunks
            calculate_all_skip_ranges(parent);
        }
    }

    /**
     * @brief Calculates skip ranges for all data chunks based on bitmap.
     *
     * For each data chunk, scans the bitmap to find:
     * - How many records at the START are unused (skip_begin)
     * - How many records at the END are unused (skip_end)
     *
     * These are converted to cluster counts for I/O optimization.
     *
     * @param parent  Parent payload (for shared state)
     */
    void calculate_all_skip_ranges(OverlappedNtfsMftReadPayload* parent)
    {
        // Number of bits per bitmap byte
        constexpr size_t kBitsPerByte = mft_reader_constants::kBitsPerByte;

        for (auto& chunk : parent->data_chunks_)
        {
            // Calculate which MFT records this chunk covers
            const size_t first_record =
                static_cast<size_t>(chunk.vcn * parent->cluster_size_ / parent->index_->mft_record_size);
            const size_t record_count =
                static_cast<size_t>(chunk.cluster_count * parent->cluster_size_ / parent->index_->mft_record_size);

            // Find first in-use record from start
            const size_t skip_records_begin =
                find_first_used_record(parent->mft_bitmap_, first_record, record_count);

            // Find first in-use record from end
            const size_t skip_records_end =
                find_last_used_record(parent->mft_bitmap_, first_record, record_count, skip_records_begin);

            // Convert record counts to cluster counts
            const size_t skip_clusters_begin = static_cast<size_t>(
                static_cast<unsigned long long>(skip_records_begin) *
                parent->index_->mft_record_size / parent->cluster_size_);

            const size_t skip_clusters_end = static_cast<size_t>(
                static_cast<unsigned long long>(skip_records_end) *
                parent->index_->mft_record_size / parent->cluster_size_);

            // Sanity check: can't skip more clusters than exist
            if (skip_clusters_begin + skip_clusters_end > chunk.cluster_count)
            {
                throw std::logic_error(
                    "Skip range calculation error: skipping more clusters than chunk contains");
            }

            // Store skip ranges (atomic for thread safety)
            chunk.skip_begin.store(skip_clusters_begin, atomic_namespace::memory_order_release);
            chunk.skip_end.store(skip_clusters_end, atomic_namespace::memory_order_release);
        }
    }

    /**
     * @brief Finds the first in-use record in a range.
     *
     * Scans the bitmap from the start of the range until finding a set bit.
     *
     * @param bitmap        The MFT bitmap
     * @param first_record  First record index in range
     * @param record_count  Number of records in range
     * @return Number of unused records at start (0 if first is used)
     */
    [[nodiscard]] static size_t find_first_used_record(
        const MftBitmap& bitmap,
        size_t first_record,
        size_t record_count)
    {
        constexpr size_t kBitsPerByte = mft_reader_constants::kBitsPerByte;

        for (size_t i = 0; i < record_count; ++i)
        {
            const size_t record_index = first_record + i;
            const size_t byte_index = record_index / kBitsPerByte;
            const size_t bit_index = record_index % kBitsPerByte;

            if (bitmap[byte_index] & (1 << bit_index))
            {
                return i;  // Found first used record
            }
        }

        return record_count;  // All records unused
    }

    /**
     * @brief Finds the last in-use record in a range.
     *
     * Scans the bitmap from the end of the range until finding a set bit.
     *
     * @param bitmap              The MFT bitmap
     * @param first_record        First record index in range
     * @param record_count        Number of records in range
     * @param skip_records_begin  Records already skipped at start
     * @return Number of unused records at end (0 if last is used)
     */
    [[nodiscard]] static size_t find_last_used_record(
        const MftBitmap& bitmap,
        size_t first_record,
        size_t record_count,
        size_t skip_records_begin)
    {
        constexpr size_t kBitsPerByte = mft_reader_constants::kBitsPerByte;

        // Don't scan past the first used record
        const size_t max_skip = record_count - skip_records_begin;

        for (size_t i = 0; i < max_skip; ++i)
        {
            const size_t record_index = first_record + record_count - 1 - i;
            const size_t byte_index = record_index / kBitsPerByte;
            const size_t bit_index = record_index % kBitsPerByte;

            if (bitmap[byte_index] & (1 << bit_index))
            {
                return i;  // Found last used record
            }
        }

        return max_skip;  // All remaining records unused
    }

    // ========================================================================
    // DATA PROCESSING HELPERS
    // ========================================================================

    /**
     * @brief Processes a completed data chunk.
     *
     * Steps:
     * 1. Call preload_concurrent() for parallel pre-processing
     * 2. Call load() to parse MFT records and update index
     *
     * @param parent  Parent payload (for shared state)
     * @param buffer  Raw MFT record data from disk
     * @param size    Number of bytes read
     */
    void process_data_chunk(
        OverlappedNtfsMftReadPayload* parent,
        void* buffer,
        size_t size)
    {
        const unsigned long long chunk_virtual_offset = voffset();

        // Pre-process in parallel (e.g., validate record signatures)
        parent->index_->preload_concurrent(chunk_virtual_offset, buffer, size);

        // Parse records and update index (requires lock)
        lock(parent->index_)->load(
            chunk_virtual_offset,
            buffer,
            size,
            skipped_begin(),
            skipped_end());
    }
};


// ============================================================================
// STATIC MEMBER DEFINITIONS
// ============================================================================

/// Mutex for thread-safe access to the recycled buffer pool.
inline atomic_namespace::recursive_mutex OverlappedNtfsMftReadPayload::ReadOperation::recycled_mutex;

/// Pool of recycled I/O buffers for reuse.
inline std::vector<std::pair<size_t, void*>> OverlappedNtfsMftReadPayload::ReadOperation::recycled;

// ============================================================================
// IMPLEMENTATION: queue_next()
// ============================================================================

/**
 * @brief Issues the next pending I/O operation to maintain the pipeline.
 *
 * This method is called:
 * 1. Initially (twice) to start concurrent I/O operations
 * 2. After each I/O completion to maintain the pipeline
 *
 * ## Priority Order
 *
 * Bitmap chunks are processed before data chunks because:
 * 1. Bitmap is needed to calculate skip ranges for data chunks
 * 2. Bitmap is much smaller than data (1 bit vs 1024 bytes per record)
 *
 * ## Thread Safety
 *
 * Uses atomic fetch_add to claim the next chunk index. If multiple threads
 * race, each gets a unique index. If index exceeds chunk count, we decrement
 * to avoid overflow.
 *
 * ## I/O Operation Setup
 *
 * For each chunk, we:
 * 1. Read skip ranges (may be 0 if bitmap not yet processed)
 * 2. Calculate actual bytes to read (excluding skipped clusters)
 * 3. Allocate ReadOperation with appended I/O buffer
 * 4. Set physical offset (LCN), virtual offset (VCN), skip info
 * 5. Issue async read via IOCP
 */
inline void OverlappedNtfsMftReadPayload::queue_next() volatile
{
    // Get non-volatile pointer for member access
    const OverlappedNtfsMftReadPayload* const self =
        const_cast<const OverlappedNtfsMftReadPayload*>(this);

    bool operation_issued = false;

    // ========================================================================
    // PHASE 1: Try to issue a bitmap read operation
    // ========================================================================

    if (!operation_issued)
    {
        operation_issued = try_issue_bitmap_read(self);
    }

    // ========================================================================
    // PHASE 2: Try to issue a data read operation
    // ========================================================================

    if (!operation_issued)
    {
        operation_issued = try_issue_data_read(self);
    }

    // If no operation was issued, all chunks have been claimed
    // (either completed or in-flight)
}

/**
 * @brief Attempts to issue a bitmap chunk read operation.
 *
 * @param self  Non-volatile pointer to this object
 * @return true if an operation was issued, false if no bitmap chunks remain
 */
inline bool OverlappedNtfsMftReadPayload::try_issue_bitmap_read(
    const OverlappedNtfsMftReadPayload* self) volatile
{
    // Atomically claim the next bitmap chunk index
    const size_t chunk_index =
        next_bitmap_chunk_index_.fetch_add(1, atomic_namespace::memory_order_acq_rel);

    if (chunk_index < self->bitmap_chunks_.size())
    {
        // Valid index - issue the read
        issue_chunk_read(self, self->bitmap_chunks_[chunk_index], true /* is_bitmap */);
        return true;
    }
    else if (chunk_index > self->bitmap_chunks_.size())
    {
        // Index overflow - decrement to prevent unbounded growth
        // This can happen when multiple threads race past the end
        next_bitmap_chunk_index_.fetch_sub(1, atomic_namespace::memory_order_acq_rel);
    }

    return false;
}

/**
 * @brief Attempts to issue a data chunk read operation.
 *
 * @param self  Non-volatile pointer to this object
 * @return true if an operation was issued, false if no data chunks remain
 */
inline bool OverlappedNtfsMftReadPayload::try_issue_data_read(
    const OverlappedNtfsMftReadPayload* self) volatile
{
    // Atomically claim the next data chunk index
    const size_t chunk_index =
        next_data_chunk_index_.fetch_add(1, atomic_namespace::memory_order_acq_rel);

    if (chunk_index < self->data_chunks_.size())
    {
        // Valid index - issue the read
        issue_chunk_read(self, self->data_chunks_[chunk_index], false /* is_bitmap */);
        return true;
    }
    else if (chunk_index > self->data_chunks_.size())
    {
        // Index overflow - decrement to prevent unbounded growth
        next_data_chunk_index_.fetch_sub(1, atomic_namespace::memory_order_acq_rel);
    }

    return false;
}

/**
 * @brief Issues an async read operation for a chunk.
 *
 * @param self       Non-volatile pointer to this object
 * @param chunk      The chunk descriptor
 * @param is_bitmap  True for bitmap chunk, false for data chunk
 */
inline void OverlappedNtfsMftReadPayload::issue_chunk_read(
    const OverlappedNtfsMftReadPayload* self,
    const ChunkDescriptor& chunk,
    bool is_bitmap) volatile
{
    // Read skip ranges (atomic - may be updated by bitmap processing)
    const unsigned long long skip_clusters_begin =
        chunk.skip_begin.load(atomic_namespace::memory_order_acquire);
    const unsigned long long skip_clusters_end =
        chunk.skip_end.load(atomic_namespace::memory_order_acquire);

    // Calculate actual clusters to read (excluding skipped regions)
    const unsigned long long clusters_to_read =
        chunk.cluster_count - (skip_clusters_begin + skip_clusters_end);

    // Calculate bytes to read
    const unsigned int bytes_to_read =
        static_cast<unsigned int>(clusters_to_read * self->cluster_size_);

    // Allocate ReadOperation with appended I/O buffer
    // Memory layout: [ReadOperation][I/O Buffer of bytes_to_read]
    intrusive_ptr<ReadOperation> operation(
        new(bytes_to_read) ReadOperation(this, is_bitmap));

    // Calculate physical offset on disk (LCN + skip adjustment)
    const long long physical_offset =
        (chunk.lcn + static_cast<long long>(skip_clusters_begin)) *
        static_cast<long long>(self->cluster_size_);

    // Calculate virtual offset in MFT file (VCN + skip adjustment)
    const unsigned long long virtual_offset =
        (chunk.vcn + skip_clusters_begin) * self->cluster_size_;

    // Set operation metadata
    operation->offset(physical_offset);
    operation->voffset(virtual_offset);
    operation->skipped_begin(skip_clusters_begin * self->cluster_size_);
    operation->skipped_end(skip_clusters_end * self->cluster_size_);
    operation->time(clock());

    // Issue async read via IOCP
    // Buffer is immediately after the ReadOperation object
    void* const buffer = operation.get() + 1;
    self->iocp_->read_file(self->index_->volume(), buffer, bytes_to_read, operation);
}


// ============================================================================
// IMPLEMENTATION: operator() - Pipeline Initialization
// ============================================================================

/**
 * @brief Initializes the MFT reading pipeline.
 *
 * This is the entry point called when the payload is posted to the IOCP.
 * It performs all initialization and starts the async I/O pipeline.
 *
 * ## Initialization Steps
 *
 * 1. **Index Initialization**: Call init() if not already done
 * 2. **Volume Query**: Get NTFS volume information (cluster size, MFT location)
 * 3. **Extent Map Retrieval**: Get physical locations of $BITMAP and $DATA
 * 4. **Chunk List Creation**: Split extents into I/O-sized chunks
 * 5. **Pipeline Start**: Issue initial concurrent I/O operations
 *
 * ## Error Handling
 *
 * Uses RAII (SetFinished struct) to ensure the index is marked as finished
 * even if an exception occurs. The error code is stored in the result.
 *
 * ## Extent Map Format
 *
 * NTFS returns extent maps as: [(ending_vcn, starting_lcn), ...]
 * Each entry describes a contiguous run of clusters.
 *
 * @param size  Unused (required by Overlapped interface)
 * @param key   Unused (required by Overlapped interface)
 * @return -1 to indicate this operation is complete
 */
inline int OverlappedNtfsMftReadPayload::operator()(size_t /*size*/, uintptr_t /*key*/)
{
    constexpr int kOperationComplete = -1;

    // Get non-volatile pointer to index
    intrusive_ptr<NtfsIndex> index = index_->unvolatile();

    // ------------------------------------------------------------------------
    // RAII guard to ensure index is marked finished on exit
    // ------------------------------------------------------------------------

    /**
     * @struct SetFinished
     * @brief RAII guard that marks the index as finished on destruction.
     *
     * This ensures the index is properly marked even if an exception occurs.
     * The result code indicates success (0) or the error number.
     */
    struct SetFinished
    {
        intrusive_ptr<NtfsIndex> index;
        unsigned int error_code = 0;

        ~SetFinished()
        {
            if (index)
            {
                index->set_finished(error_code);
            }
        }
    } finished_guard = { index, 0 };

    try
    {
        // --------------------------------------------------------------------
        // Step 1: Initialize the index if needed
        // --------------------------------------------------------------------

        if (!index->init_called())
        {
            index->init();
        }

        // --------------------------------------------------------------------
        // Step 2: Get volume handle and query NTFS information
        // --------------------------------------------------------------------

        void* const volume_handle = index->volume();
        if (!volume_handle)
        {
            return kOperationComplete;
        }

        // Allow derived classes to perform pre-open operations
        preopen();

        // Query NTFS volume data (cluster size, MFT location, etc.)
        NTFS_VOLUME_DATA_BUFFER volume_info;
        unsigned long bytes_returned;
        CheckAndThrow(DeviceIoControl(
            volume_handle,
            FSCTL_GET_NTFS_VOLUME_DATA,
            nullptr, 0,
            &volume_info, sizeof(volume_info),
            &bytes_returned,
            nullptr));

        // Store volume information
        initialize_volume_info(index.get(), volume_info);

        // Associate volume with IOCP for async operations
        iocp_->associate(volume_handle, reinterpret_cast<uintptr_t>(&*index));

        // --------------------------------------------------------------------
        // Step 3: Build chunk lists for $BITMAP and $DATA
        // --------------------------------------------------------------------

        build_bitmap_chunk_list(index.get(), volume_info);
        build_data_chunk_list(index.get(), volume_info);

        // --------------------------------------------------------------------
        // Step 4: Start the I/O pipeline
        // --------------------------------------------------------------------

        // Clear the finished guard so we don't mark as finished yet
        // (the pipeline will continue asynchronously)
        finished_guard.index.reset();

        // Issue initial concurrent I/O operations
        for (int i = 0; i < mft_reader_constants::kIoConcurrencyLevel; ++i)
        {
            queue_next();
        }
    }
    catch (CStructured_Exception& ex)
    {
        // Store error code for the finished guard
        finished_guard.error_code = ex.GetSENumber();
    }

    return kOperationComplete;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

/**
 * @brief Initializes volume information from NTFS query result.
 *
 * @param index        The index to update
 * @param volume_info  NTFS volume data from DeviceIoControl
 */
inline void OverlappedNtfsMftReadPayload::initialize_volume_info(
    NtfsIndex* index,
    const NTFS_VOLUME_DATA_BUFFER& volume_info)
{
    // Store cluster size for I/O calculations
    cluster_size_ = static_cast<unsigned int>(volume_info.BytesPerCluster);
    index->cluster_size = volume_info.BytesPerCluster;

    // Store MFT record size (typically 1024 bytes)
    index->mft_record_size = volume_info.BytesPerFileRecordSegment;

    // Calculate MFT capacity (number of records)
    index->mft_capacity = static_cast<unsigned int>(
        volume_info.MftValidDataLength.QuadPart / volume_info.BytesPerFileRecordSegment);

    // Store MFT zone information
    index->mft_zone_start = volume_info.MftZoneStart.QuadPart;
    index->mft_zone_end = volume_info.MftZoneEnd.QuadPart;

    // Suppress MFT zone inclusion in "size on disk" calculation
    // by setting zone_end = zone_start (effectively zero-size zone)
    index->mft_zone_end = index->mft_zone_start;

    // Store reserved cluster count
    index->reserved_clusters.store(
        volume_info.TotalReserved.QuadPart + index->mft_zone_end - index->mft_zone_start);
}

/**
 * @brief Builds the chunk list for reading $MFT::$BITMAP.
 *
 * The bitmap is one bit per MFT record, indicating if the record is in-use.
 * We split the bitmap into chunks for parallel I/O.
 *
 * @param index        The index (for root path)
 * @param volume_info  NTFS volume data (for MFT start LCN)
 */
inline void OverlappedNtfsMftReadPayload::build_bitmap_chunk_list(
    NtfsIndex* index,
    const NTFS_VOLUME_DATA_BUFFER& volume_info)
{
    // Type alias for extent map: [(ending_vcn, starting_lcn), ...]
    using ExtentMap = std::vector<std::pair<unsigned long long, long long>>;

    // Get extent map for $MFT::$BITMAP
    long long bitmap_size_bytes = 0;
    const ExtentMap extents = get_retrieval_pointers(
        (index->root_path() + std::tvstring(_T("$MFT::$BITMAP"))).c_str(),
        &bitmap_size_bytes,
        volume_info.MftStartLcn.QuadPart,
        index_->mft_record_size);

    // Convert extents to chunks
    build_chunk_list_from_extents(extents, bitmap_chunks_);

    // Allocate bitmap buffer
    // Default to all 1s so we read everything if bitmap is unavailable
    mft_bitmap_.resize(
        static_cast<size_t>(bitmap_size_bytes),
        static_cast<MftBitmap::value_type>(~MftBitmap::value_type()));

    // Initialize remaining chunk counter
    bitmap_chunks_remaining_.store(bitmap_chunks_.size(), atomic_namespace::memory_order_release);
}

/**
 * @brief Builds the chunk list for reading $MFT::$DATA.
 *
 * The data stream contains the actual MFT records.
 * We split it into chunks for parallel I/O.
 *
 * @param index        The index (for root path)
 * @param volume_info  NTFS volume data (for MFT start LCN)
 */
inline void OverlappedNtfsMftReadPayload::build_data_chunk_list(
    NtfsIndex* index,
    const NTFS_VOLUME_DATA_BUFFER& volume_info)
{
    // Type alias for extent map
    using ExtentMap = std::vector<std::pair<unsigned long long, long long>>;

    // Get extent map for $MFT::$DATA
    long long data_size_bytes = 0;
    const ExtentMap extents = get_retrieval_pointers(
        (index->root_path() + std::tvstring(_T("$MFT::$DATA"))).c_str(),
        &data_size_bytes,
        volume_info.MftStartLcn.QuadPart,
        index->mft_record_size);

    // Validate that we got extent data
    if (extents.empty())
    {
        CppRaiseException(ERROR_UNRECOGNIZED_VOLUME);
    }

    // Convert extents to chunks
    build_chunk_list_from_extents(extents, data_chunks_);
}

/**
 * @brief Converts an extent map to a list of I/O chunks.
 *
 * NTFS files can be fragmented, with data spread across multiple extents.
 * Each extent may be larger than our desired I/O size, so we split them
 * into smaller chunks.
 *
 * ## Extent Map Format
 *
 * Input: [(ending_vcn_1, starting_lcn_1), (ending_vcn_2, starting_lcn_2), ...]
 *
 * Each entry describes a contiguous run:
 * - ending_vcn: The VCN where this extent ENDS (exclusive)
 * - starting_lcn: The LCN where this extent STARTS
 *
 * ## Chunk Creation
 *
 * For each extent, we create chunks of at most max_read_size_ bytes.
 * This allows parallel I/O and better memory management.
 *
 * @param extents  The extent map from get_retrieval_pointers()
 * @param chunks   Output: list of chunks to read
 */
inline void OverlappedNtfsMftReadPayload::build_chunk_list_from_extents(
    const std::vector<std::pair<unsigned long long, long long>>& extents,
    ChunkList& chunks)
{
    // Calculate maximum clusters per chunk
    // Formula: ceil(max_read_size_ / cluster_size_)
    const unsigned long long max_clusters_per_chunk =
        1 + (max_read_size_ - 1) / cluster_size_;

    unsigned long long current_vcn = 0;

    for (const auto& extent : extents)
    {
        const unsigned long long extent_ending_vcn = extent.first;
        const long long extent_starting_lcn = extent.second;

        // Calculate clusters in this extent
        // Note: extent.first is the ENDING VCN, not the count
        const long long extent_clusters =
            static_cast<long long>(std::max(extent_ending_vcn, current_vcn) - current_vcn);

        // Split extent into chunks
        for (long long offset = 0; offset < extent_clusters; )
        {
            // Calculate chunk size (limited by max_clusters_per_chunk)
            const unsigned long long remaining =
                static_cast<unsigned long long>(extent_clusters - offset);
            const unsigned long long chunk_clusters =
                std::min(extent_ending_vcn - current_vcn, max_clusters_per_chunk);

            // Create chunk descriptor
            chunks.emplace_back(
                current_vcn,                              // VCN (virtual position)
                chunk_clusters,                           // Cluster count
                extent_starting_lcn + offset              // LCN (physical position)
            );

            // Advance position
            current_vcn += chunk_clusters;
            offset += static_cast<long long>(chunk_clusters);
        }
    }
}

#endif // UFFS_MFT_READER_HPP