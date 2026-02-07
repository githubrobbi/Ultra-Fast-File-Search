/**
 * @file mft_reader_impl.hpp
 * @brief Implementation details for the MFT reader.
 *
 * This file contains:
 * - ReadOperation nested class (async I/O operation handler)
 * - Method implementations for OverlappedNtfsMftReadPayload
 *
 * @note This file is included at the end of mft_reader.hpp.
 *       Do not include this file directly.
 *
 * @see mft_reader.hpp for the public API
 */

#ifndef UFFS_MFT_READER_IMPL_HPP
#define UFFS_MFT_READER_IMPL_HPP

// This file should only be included from mft_reader.hpp
#ifndef UFFS_MFT_READER_HPP
#error "Do not include mft_reader_impl.hpp directly. Include mft_reader.hpp instead."
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
                static_cast<size_t>(chunk.vcn * parent->cluster_size_ / parent->index_->mft_record_size);
            const size_t record_count =
                static_cast<size_t>(chunk.cluster_count * parent->cluster_size_ / parent->index_->mft_record_size);

            // Find first in-use record from start (using bitmap_utils)
            const size_t skip_records_begin =
                bitmap_utils::find_first_set_bit(parent->mft_bitmap_, first_record, record_count);

            // Find first in-use record from end (using bitmap_utils)
            const size_t skip_records_end =
                bitmap_utils::find_last_set_bit(parent->mft_bitmap_, first_record, record_count, skip_records_begin);

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
 */
inline void OverlappedNtfsMftReadPayload::queue_next() volatile
{
    // Get non-volatile pointer for member access
    const OverlappedNtfsMftReadPayload* const self =
        const_cast<const OverlappedNtfsMftReadPayload*>(this);

    bool operation_issued = false;

    // PHASE 1: Try to issue a bitmap read operation
    if (!operation_issued)
    {
        operation_issued = try_issue_bitmap_read(self);
    }

    // PHASE 2: Try to issue a data read operation
    if (!operation_issued)
    {
        operation_issued = try_issue_data_read(self);
    }

    // If no operation was issued, all chunks have been claimed
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
 * @param size  Unused (required by Overlapped interface)
 * @param key   Unused (required by Overlapped interface)
 * @return -1 to indicate this operation is complete
 */
inline int OverlappedNtfsMftReadPayload::operator()(size_t /*size*/, uintptr_t /*key*/)
{
    constexpr int kOperationComplete = -1;

    // Get non-volatile pointer to index
    intrusive_ptr<NtfsIndex> index = index_->unvolatile();

    // RAII guard to ensure index is marked finished on exit
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
        // Step 1: Initialize the index if needed
        if (!index->init_called())
        {
            index->init();
        }

        // Step 2: Get volume handle and query NTFS information
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

        // Step 3: Build chunk lists for $BITMAP and $DATA
        build_bitmap_chunk_list(index.get(), volume_info);
        build_data_chunk_list(index.get(), volume_info);

        // Step 4: Start the I/O pipeline
        // Clear the finished guard so we don't mark as finished yet
        finished_guard.index.reset();

        // Issue initial concurrent I/O operations
        for (int i = 0; i < mft_reader_constants::kIoConcurrencyLevel; ++i)
        {
            queue_next();
        }
    }
    catch (CStructured_Exception& ex)
    {
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
    index->mft_zone_end = index->mft_zone_start;

    // Store reserved cluster count
    index->reserved_clusters.store(
        volume_info.TotalReserved.QuadPart + index->mft_zone_end - index->mft_zone_start);
}

/**
 * @brief Builds the chunk list for reading $MFT::$BITMAP.
 *
 * @param index        The index (for root path)
 * @param volume_info  NTFS volume data (for MFT start LCN)
 */
inline void OverlappedNtfsMftReadPayload::build_bitmap_chunk_list(
    NtfsIndex* index,
    const NTFS_VOLUME_DATA_BUFFER& volume_info)
{
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

    // Allocate bitmap buffer (default to all 1s)
    mft_bitmap_.resize(
        static_cast<size_t>(bitmap_size_bytes),
        static_cast<MftBitmap::value_type>(~MftBitmap::value_type()));

    // Initialize remaining chunk counter
    bitmap_chunks_remaining_.store(bitmap_chunks_.size(), atomic_namespace::memory_order_release);
}

/**
 * @brief Builds the chunk list for reading $MFT::$DATA.
 *
 * @param index        The index (for root path)
 * @param volume_info  NTFS volume data (for MFT start LCN)
 */
inline void OverlappedNtfsMftReadPayload::build_data_chunk_list(
    NtfsIndex* index,
    const NTFS_VOLUME_DATA_BUFFER& volume_info)
{
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
 * @param extents  The extent map from get_retrieval_pointers()
 * @param chunks   Output: list of chunks to read
 */
inline void OverlappedNtfsMftReadPayload::build_chunk_list_from_extents(
    const std::vector<std::pair<unsigned long long, long long>>& extents,
    ChunkList& chunks)
{
    // Calculate maximum clusters per chunk
    const unsigned long long max_clusters_per_chunk =
        1 + (max_read_size_ - 1) / cluster_size_;

    unsigned long long current_vcn = 0;

    for (const auto& extent : extents)
    {
        const unsigned long long extent_ending_vcn = extent.first;
        const long long extent_starting_lcn = extent.second;

        // Calculate clusters in this extent
        const long long extent_clusters =
            static_cast<long long>(std::max(extent_ending_vcn, current_vcn) - current_vcn);

        // Split extent into chunks
        for (long long offset = 0; offset < extent_clusters; )
        {
            const unsigned long long chunk_clusters =
                std::min(extent_ending_vcn - current_vcn, max_clusters_per_chunk);

            // Create chunk descriptor
            chunks.emplace_back(
                current_vcn,
                chunk_clusters,
                extent_starting_lcn + offset
            );

            // Advance position
            current_vcn += chunk_clusters;
            offset += static_cast<long long>(chunk_clusters);
        }
    }
}

#endif // UFFS_MFT_READER_IMPL_HPP
