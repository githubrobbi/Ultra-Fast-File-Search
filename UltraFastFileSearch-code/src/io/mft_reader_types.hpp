/**
 * @file mft_reader_types.hpp
 * @brief Type definitions for MFT (Master File Table) reading operations.
 *
 * This header contains data structures used by the MFT reader:
 * - ChunkDescriptor: Describes a chunk of MFT data to read
 * - Type aliases for chunk lists and bitmaps
 *
 * @note Separated from mft_reader.hpp for single-responsibility and faster compilation.
 */

#ifndef UFFS_MFT_READER_TYPES_HPP
#define UFFS_MFT_READER_TYPES_HPP

#include "../util/atomic_compat.hpp"
#include <vector>

namespace mft_reader_types {

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

/// List of chunks to read (for bitmap or data).
using ChunkList = std::vector<ChunkDescriptor>;

/// MFT bitmap: one bit per record (1 = in-use, 0 = free).
using MftBitmap = std::vector<unsigned char>;

// Legacy type aliases for backward compatibility
using RetPtr = ChunkDescriptor;
using RetPtrs = ChunkList;
using Bitmap = MftBitmap;

}  // namespace mft_reader_types

#endif // UFFS_MFT_READER_TYPES_HPP

