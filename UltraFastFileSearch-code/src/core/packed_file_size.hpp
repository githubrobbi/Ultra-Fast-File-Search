#pragma once
/**
 * @file packed_file_size.hpp
 * @brief Compact file size types for NTFS indexing
 *
 * @details
 * This file provides memory-optimized types for storing file sizes in the
 * NTFS index. The key optimization is `file_size_type`, a 6-byte (48-bit)
 * integer that can represent files up to 256 TB while saving 2 bytes per
 * size value compared to a full 64-bit integer.
 *
 * ## Why 48 Bits?
 *
 * - 48 bits = 256 TB maximum file size
 * - NTFS theoretical max is 16 EB, but practical limits are much lower
 * - Windows 10/11 NTFS max file size is 256 TB (matches our 48-bit limit)
 * - Saving 2 bytes per size × 3 sizes per stream × millions of files = significant memory savings
 *
 * ## Memory Layout
 *
 * ```
 * file_size_type (6 bytes, packed):
 * ┌────────────────────────────────┬────────────────┐
 * │         low (32 bits)          │  high (16 bits)│
 * └────────────────────────────────┴────────────────┘
 * Byte:  0    1    2    3    4    5
 * ```
 *
 * ## SizeInfo Structure
 *
 * `SizeInfo` groups three file sizes plus a tree size counter:
 *
 * | Field     | Type           | Description                              |
 * |-----------|----------------|------------------------------------------|
 * | length    | file_size_type | Logical file size (what you see)         |
 * | allocated | file_size_type | Allocated size on disk (may be larger)   |
 * | bulkiness | file_size_type | Compressed/sparse actual disk usage      |
 * | treesize  | unsigned int   | Number of files in subtree (directories) |
 *
 * Total: 6 + 6 + 6 + 4 = 22 bytes
 *
 * @see StreamInfo - Uses SizeInfo for stream size information
 * @see Record - Uses SizeInfo indirectly via StreamInfo
 */

#include <climits>
#include "io/overlapped.hpp"  // for value_initialized

namespace uffs {

/**
 * @brief Packed 6-byte file size type (48-bit, supports up to 256 TB)
 *
 * @details
 * A memory-efficient alternative to `unsigned long long` for storing file
 * sizes. Uses `#pragma pack(push, 1)` to ensure exactly 6 bytes with no
 * padding.
 *
 * ## Supported Range
 *
 * - Minimum: 0
 * - Maximum: 2^48 - 1 = 281,474,976,710,655 bytes = 256 TB
 *
 * ## Arithmetic Operations
 *
 * Supports `+=` and `-=` operators. For other operations, convert to
 * `unsigned long long` first using the implicit conversion operator.
 *
 * ## Usage Example
 *
 * ```cpp
 * file_size_type size = 1024 * 1024 * 1024ULL;  // 1 GB
 * size += 512;  // Add 512 bytes
 *
 * unsigned long long bytes = size;  // Implicit conversion
 * printf("Size: %llu bytes\n", bytes);
 * ```
 */
#pragma pack(push, 1)
class file_size_type
{
	unsigned int low;    ///< Low 32 bits of the file size
	unsigned short high; ///< High 16 bits of the file size
	typedef file_size_type this_type;

public:
	/**
	 * @brief Default constructor - initializes to zero
	 */
	file_size_type() noexcept : low(), high() {}

	/**
	 * @brief Construct from a 64-bit value
	 * @param value The file size (only lower 48 bits are stored)
	 * @warning Values > 256 TB will be truncated!
	 */
	file_size_type(unsigned long long const value) noexcept
		: low(static_cast<unsigned int>(value))
		, high(static_cast<unsigned short>(value >> (sizeof(low) * CHAR_BIT))) {}

	/**
	 * @brief Convert to 64-bit unsigned integer
	 * @return The file size as unsigned long long
	 */
	[[nodiscard]] operator unsigned long long() const noexcept
	{
		return static_cast<unsigned long long>(this->low) |
			(static_cast<unsigned long long>(this->high) << (sizeof(this->low) * CHAR_BIT));
	}

	/**
	 * @brief Add to this file size
	 * @param value The amount to add
	 * @return Reference to this (for chaining)
	 */
	file_size_type operator+=(this_type const value) noexcept
	{
		return *this = static_cast<unsigned long long>(*this) + static_cast<unsigned long long>(value);
	}

	/**
	 * @brief Subtract from this file size
	 * @param value The amount to subtract
	 * @return Reference to this (for chaining)
	 */
	file_size_type operator-=(this_type const value) noexcept
	{
		return *this = static_cast<unsigned long long>(*this) - static_cast<unsigned long long>(value);
	}

	/**
	 * @brief Check if the file size is zero
	 * @return true if zero, false otherwise
	 */
	[[nodiscard]] bool operator!() const noexcept
	{
		return !this->low && !this->high;
	}
};
#pragma pack(pop)

/**
 * @brief Size information for files and streams
 *
 * @details
 * Groups together the various size metrics for a file or stream:
 *
 * - **length**: The logical file size (what applications see)
 * - **allocated**: The allocated size on disk (rounded up to cluster size)
 * - **bulkiness**: The actual disk usage (may be less for compressed/sparse files)
 * - **treesize**: For directories, the count of files in the subtree
 *
 * ## Example
 *
 * A 1 MB file on a volume with 4 KB clusters:
 * - length = 1,048,576 (1 MB)
 * - allocated = 1,048,576 (1 MB, already cluster-aligned)
 * - bulkiness = 1,048,576 (1 MB, not compressed)
 *
 * A 1 MB sparse file with only 100 KB of actual data:
 * - length = 1,048,576 (1 MB logical size)
 * - allocated = 1,048,576 (1 MB reserved)
 * - bulkiness = 102,400 (100 KB actual disk usage)
 */
struct SizeInfo
{
	file_size_type length;    ///< Logical file size
	file_size_type allocated; ///< Allocated size on disk
	file_size_type bulkiness; ///< Actual disk usage (compressed/sparse)
	value_initialized<unsigned int>::type treesize; ///< Files in subtree (directories)
};

} // namespace uffs

// ============================================================================
// Backward Compatibility
// ============================================================================
// Expose at global scope for existing code that doesn't use uffs::
// ============================================================================
using uffs::file_size_type;
using uffs::SizeInfo;

