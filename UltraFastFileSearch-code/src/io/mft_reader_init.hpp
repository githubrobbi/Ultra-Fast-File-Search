/**
 * @file mft_reader_init.hpp
 * @brief Initialization methods for the MFT reader.
 *
 * This file contains the initialization logic for the MFT reading pipeline:
 * - operator(): Main entry point called when payload is posted to IOCP
 * - initialize_volume_info(): Stores NTFS volume parameters
 * - build_bitmap_chunk_list(): Creates chunk list for $MFT::$BITMAP
 * - build_data_chunk_list(): Creates chunk list for $MFT::$DATA
 * - build_chunk_list_from_extents(): Converts extent map to I/O chunks
 *
 * ## Initialization Flow
 *
 * When the MFT reader payload is posted to the IOCP, operator() is called:
 *
 * 1. Initialize NtfsIndex (if not already done)
 * 2. Query NTFS Volume Data (FSCTL_GET_NTFS_VOLUME_DATA)
 * 3. Store Volume Information (cluster_size, mft_record_size, mft_capacity)
 * 4. Associate Volume with IOCP (enables async I/O)
 * 5. Build Chunk Lists ($MFT::$BITMAP and $MFT::$DATA extents)
 * 6. Start I/O Pipeline (queue_next() N times)
 *
 * ## Error Handling
 *
 * Errors during initialization are caught and stored in the SetFinished guard.
 * The index is marked as finished with the error code when the guard destructs.
 *
 * @note This file is included by mft_reader_impl.hpp.
 *       Do not include this file directly.
 *
 * @see mft_reader_read_operation.hpp for ReadOperation class
 * @see mft_reader_pipeline.hpp for pipeline management
 * @see io_completion_port.hpp for IOCP integration
 */

#ifndef UFFS_MFT_READER_INIT_HPP
#define UFFS_MFT_READER_INIT_HPP

// This file should only be included from mft_reader_impl.hpp
#ifndef UFFS_MFT_READER_IMPL_HPP
#error "Do not include mft_reader_init.hpp directly. Include mft_reader.hpp instead."
#endif

// ============================================================================
// IMPLEMENTATION: operator() - Pipeline Initialization
// ============================================================================

/**
 * @brief Initializes the MFT reading pipeline.
 *
 * This is the entry point called when the payload is posted to the IOCP.
 * It performs all initialization and starts the async I/O pipeline.
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
    index->set_cluster_size(static_cast<unsigned int>(volume_info.BytesPerCluster));

    // Store MFT record size (typically 1024 bytes)
    index->set_mft_record_size(volume_info.BytesPerFileRecordSegment);

    // Calculate MFT capacity (number of records)
    index->set_mft_capacity(static_cast<unsigned int>(
        volume_info.MftValidDataLength.QuadPart / volume_info.BytesPerFileRecordSegment));

    // Store MFT zone information
    index->set_mft_zone_start(volume_info.MftZoneStart.QuadPart);
    index->set_mft_zone_end(volume_info.MftZoneEnd.QuadPart);

    // Suppress MFT zone inclusion in "size on disk" calculation
    index->set_mft_zone_end(index->mft_zone_start());

    // Store reserved cluster count
    index->set_reserved_clusters(
        volume_info.TotalReserved.QuadPart + index->mft_zone_end() - index->mft_zone_start());
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
        index_->mft_record_size());

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
        index->mft_record_size());

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

#endif // UFFS_MFT_READER_INIT_HPP

