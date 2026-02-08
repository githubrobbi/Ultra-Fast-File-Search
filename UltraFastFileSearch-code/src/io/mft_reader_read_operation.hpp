/**
 * @file mft_reader_read_operation.hpp
 * @brief ReadOperation nested class for async MFT I/O operations.
 *
 * This file contains the ReadOperation class, which handles individual async
 * I/O operations for reading MFT chunks. Each operation manages its own buffer,
 * completion handling, and memory recycling.
 *
 * ## Architecture Overview
 *
 * The MFT reader uses a pipeline of concurrent async I/O operations to maximize
 * disk throughput. Each ReadOperation represents one chunk being read:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                        MFT READING PIPELINE                                  │
 * │                                                                              │
 * │  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                   │
 * │  │ ReadOperation│    │ ReadOperation│    │ ReadOperation│  ... (N concurrent)│
 * │  │   Chunk 0    │    │   Chunk 1    │    │   Chunk 2    │                   │
 * │  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘                   │
 * │         │                   │                   │                            │
 * │         ▼                   ▼                   ▼                            │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                    I/O Completion Port (IOCP)                         │   │
 * │  │              Windows kernel manages async I/O queue                   │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * │                                    │                                         │
 * │                                    ▼                                         │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                         NtfsIndex                                     │   │
 * │  │              Receives parsed MFT records from each chunk              │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Memory Layout
 *
 * Each ReadOperation allocates itself + trailing I/O buffer in one allocation:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                         SINGLE ALLOCATION                                    │
 * │                                                                              │
 * │  ┌─────────────────────────┬────────────────────────────────────────────┐   │
 * │  │   ReadOperation object  │        I/O Buffer (variable size)          │   │
 * │  │   (sizeof(ReadOperation))│        (chunk_size bytes)                  │   │
 * │  │                         │                                             │   │
 * │  │  - virtual_offset_      │  ┌─────────────────────────────────────┐   │   │
 * │  │  - skipped_bytes_begin_ │  │  Raw disk data read by Windows      │   │   │
 * │  │  - skipped_bytes_end_   │  │  (MFT records or bitmap bytes)      │   │   │
 * │  │  - issue_time_          │  └─────────────────────────────────────┘   │   │
 * │  │  - is_bitmap_chunk_     │                                             │   │
 * │  │  - parent_              │                                             │   │
 * │  └─────────────────────────┴────────────────────────────────────────────┘   │
 * │                            ^                                                 │
 * │                            │                                                 │
 * │                      this + 1 (buffer pointer)                               │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Completion Flow
 *
 * When an async I/O operation completes, the IOCP calls operator():
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                         COMPLETION FLOW                                      │
 * │                                                                              │
 * │  ┌─────────────┐                                                            │
 * │  │ I/O Complete│                                                            │
 * │  │ (IOCP calls │                                                            │
 * │  │  operator())│                                                            │
 * │  └──────┬──────┘                                                            │
 * │         │                                                                    │
 * │         ▼                                                                    │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │ 1. queue_next() - Issue next pending I/O to maintain pipeline       │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │         │                                                                    │
 * │         ▼                                                                    │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │ 2. Process chunk data:                                              │    │
 * │  │    - BITMAP: Count valid records, copy to master bitmap             │    │
 * │  │    - DATA: Parse MFT records, update NtfsIndex                      │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │         │                                                                    │
 * │         ▼                                                                    │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │ 3. report_speed() - Record I/O statistics                           │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │         │                                                                    │
 * │         ▼                                                                    │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │ 4. Return -1 (operation complete, recycle buffer)                   │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Memory Recycling Algorithm
 *
 * To avoid allocation overhead during high-frequency I/O, completed operations
 * are recycled rather than freed:
 *
 * ```
 * ALLOCATION (operator new):
 *   1. Lock recycled_mutex
 *   2. Search recycled[] for smallest buffer >= requested size
 *   3. If found: remove from pool, return pointer
 *   4. If not found: malloc() new buffer
 *
 * DEALLOCATION (operator delete):
 *   1. Lock recycled_mutex
 *   2. Add (size, pointer) to recycled[] pool
 *   3. Buffer will be reused by next allocation
 * ```
 *
 * ## Thread Safety
 *
 * - `recycled_mutex`: Protects the recycled buffer pool (recursive mutex)
 * - `parent_`: intrusive_ptr with atomic reference counting
 * - Atomic operations used for cross-thread coordination with parent payload
 * - All bitmap/data processing uses atomic fetch_add/fetch_sub for counters
 *
 * @note This file is included by mft_reader_impl.hpp.
 *       Do not include this file directly.
 *
 * @see mft_reader.hpp for the public API
 * @see mft_reader_pipeline.hpp for queue_next() and I/O issuing
 * @see io_completion_port.hpp for IOCP integration
 * @see bitmap_utils.hpp for bitmap processing helpers
 */

#ifndef UFFS_MFT_READER_READ_OPERATION_HPP
#define UFFS_MFT_READER_READ_OPERATION_HPP

// This file should only be included from mft_reader_impl.hpp
#ifndef UFFS_MFT_READER_IMPL_HPP
#error "Do not include mft_reader_read_operation.hpp directly. Include mft_reader.hpp instead."
#endif

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
 * @see mft_reader_read_operation.hpp file header for detailed architecture
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

    // Legacy member name aliases (for compatibility with existing code)
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
        if (bitmap_bit_offset > parent->index_->mft_capacity())
        {
            // This chunk is beyond MFT capacity, skip processing
            check_if_last_bitmap_chunk(parent);
            return;
        }

        // Calculate how many bytes to process (may be truncated at MFT end)
        size_t bytes_to_process = size;
        const unsigned long long max_bitmap_offset = parent->index_->mft_capacity() / kBitsPerBitmapByte;
        if (voffset() + bytes_to_process > max_bitmap_offset)
        {
            bytes_to_process = static_cast<size_t>(max_bitmap_offset - voffset());
        }

        // Count valid records using nibble-based popcount
        const unsigned int valid_count = bitmap_utils::count_bits_in_buffer(buffer, bytes_to_process);

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
        for (auto& chunk : parent->data_chunks_)
        {
            // Calculate which MFT records this chunk covers
            const size_t first_record =
                static_cast<size_t>(chunk.vcn * parent->cluster_size_ / parent->index_->mft_record_size());
            const size_t record_count =
                static_cast<size_t>(chunk.cluster_count * parent->cluster_size_ / parent->index_->mft_record_size());

            // Find first in-use record from start (using bitmap_utils)
            const size_t skip_records_begin =
                bitmap_utils::find_first_set_bit(parent->mft_bitmap_, first_record, record_count);

            // Find first in-use record from end (using bitmap_utils)
            const size_t skip_records_end =
                bitmap_utils::find_last_set_bit(parent->mft_bitmap_, first_record, record_count, skip_records_begin);

            // Convert record counts to cluster counts
            const size_t skip_clusters_begin = static_cast<size_t>(
                static_cast<unsigned long long>(skip_records_begin) *
                parent->index_->mft_record_size() / parent->cluster_size_);

            const size_t skip_clusters_end = static_cast<size_t>(
                static_cast<unsigned long long>(skip_records_end) *
                parent->index_->mft_record_size() / parent->cluster_size_);

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


#endif // UFFS_MFT_READER_READ_OPERATION_HPP
