/**
 * @file mft_reader_pipeline.hpp
 * @brief Pipeline management for concurrent MFT I/O operations.
 *
 * This file contains the methods that manage the async I/O pipeline:
 * - queue_next(): Issues the next pending I/O operation
 * - try_issue_bitmap_read(): Attempts to issue a bitmap chunk read
 * - try_issue_data_read(): Attempts to issue a data chunk read
 * - issue_chunk_read(): Issues an async read for a specific chunk
 *
 * ## Pipeline Architecture
 *
 * The MFT reader maintains a pipeline of concurrent I/O operations to maximize
 * disk throughput. The pipeline depth is controlled by kIoConcurrencyLevel.
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                        I/O PIPELINE MANAGEMENT                               │
 * │                                                                              │
 * │  Initial State:                                                              │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │ operator() calls queue_next() N times (N = kIoConcurrencyLevel)     │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                                                                              │
 * │  Steady State:                                                               │
 * │  ┌─────────────────────────────────────────────────────────────────────┐    │
 * │  │ Each I/O completion calls queue_next() to maintain pipeline depth   │    │
 * │  └─────────────────────────────────────────────────────────────────────┘    │
 * │                                                                              │
 * │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐                     │
 * │  │ Chunk 0  │  │ Chunk 1  │  │ Chunk 2  │  │ Chunk 3  │  ...                │
 * │  │ (active) │  │ (active) │  │ (pending)│  │ (pending)│                     │
 * │  └────┬─────┘  └────┬─────┘  └──────────┘  └──────────┘                     │
 * │       │             │                                                        │
 * │       ▼             ▼                                                        │
 * │  ┌──────────────────────────────────────────────────────────────────────┐   │
 * │  │                    IOCP (Windows Kernel)                              │   │
 * │  │              Manages async I/O completion queue                       │   │
 * │  └──────────────────────────────────────────────────────────────────────┘   │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Chunk Priority
 *
 * Bitmap chunks are processed before data chunks because:
 * 1. Bitmap is needed to calculate skip ranges for data chunks
 * 2. Bitmap is much smaller (1 bit per record vs 1024 bytes per record)
 *
 * ```
 * queue_next() Priority:
 *   1. Try bitmap chunk (if any remain)
 *   2. Try data chunk (if any remain)
 *   3. No-op (all chunks claimed)
 * ```
 *
 * ## Thread Safety
 *
 * Multiple IOCP threads may call queue_next() concurrently. Thread safety is
 * achieved through atomic fetch_add on chunk indices:
 *
 * ```
 * Thread A: index = fetch_add(1) → gets 0
 * Thread B: index = fetch_add(1) → gets 1
 * Thread C: index = fetch_add(1) → gets 2
 * ```
 *
 * Each thread gets a unique index, ensuring no chunk is processed twice.
 *
 * @note This file is included by mft_reader_impl.hpp.
 *       Do not include this file directly.
 *
 * @see mft_reader_read_operation.hpp for ReadOperation class
 * @see mft_reader_init.hpp for initialization methods
 * @see io_completion_port.hpp for IOCP integration
 */

#ifndef UFFS_MFT_READER_PIPELINE_HPP
#define UFFS_MFT_READER_PIPELINE_HPP

// This file should only be included from mft_reader_impl.hpp
#ifndef UFFS_MFT_READER_IMPL_HPP
#error "Do not include mft_reader_pipeline.hpp directly. Include mft_reader.hpp instead."
#endif

// ============================================================================
// IMPLEMENTATION: queue_next()
// ============================================================================

/**
 * @brief Issues the next pending I/O operation to maintain the pipeline.
 *
 * This method is called:
 * 1. Initially (N times) to start concurrent I/O operations
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
 * This method:
 * 1. Reads skip ranges from the chunk descriptor (atomic)
 * 2. Calculates actual bytes to read (excluding skipped regions)
 * 3. Allocates a ReadOperation with appended I/O buffer
 * 4. Sets up physical and virtual offsets
 * 5. Issues the async read via IOCP
 *
 * ## Skip Range Optimization
 *
 * Skip ranges are calculated from the bitmap to avoid reading unused records:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                         CHUNK WITH SKIP RANGES                               │
 * │                                                                              │
 * │  ┌──────────────┬────────────────────────────────┬──────────────┐           │
 * │  │ skip_begin   │      ACTUAL DATA TO READ       │  skip_end    │           │
 * │  │ (unused)     │      (in-use records)          │  (unused)    │           │
 * │  └──────────────┴────────────────────────────────┴──────────────┘           │
 * │                                                                              │
 * │  Physical offset = LCN + skip_begin                                          │
 * │  Virtual offset  = VCN + skip_begin                                          │
 * │  Bytes to read   = (cluster_count - skip_begin - skip_end) * cluster_size    │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
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


#endif // UFFS_MFT_READER_PIPELINE_HPP
