#pragma once
/**
 * @file ntfs_key_type.hpp
 * @brief Packed key type for NTFS index entries
 *
 * @details
 * This file contains `key_type_internal`, a memory-optimized key structure used
 * to uniquely identify entries in the NTFS file index. The key combines:
 *
 * 1. **File Record Segment (FRS) number** - The MFT record number (32 bits)
 * 2. **Name info index** - Which hardlink name to use (10 bits, max 1023)
 * 3. **Stream info index** - Which alternate data stream (13 bits, max 8191)
 * 4. **Result index** - Position in sorted search results (9 bits, max 511)
 *
 * ## Memory Layout
 *
 * The structure uses `#pragma pack(push, 1)` to ensure no padding:
 *
 * ```
 * Offset  Size   Field
 * ------  -----  ------------------
 * 0       4      _frs (FRS number)
 * 4       4      Bitfield word:
 *                  [0:9]   _name_info   (10 bits)
 *                  [10:22] _stream_info (13 bits)
 *                  [23:31] _index       (9 bits)
 * ------  -----  ------------------
 * Total:  8 bytes
 * ```
 *
 * ## Why Bitfields?
 *
 * NTFS files can have:
 * - Up to 1024 hardlinks (we use 10 bits = 1023 max, with all-1s as sentinel)
 * - Many alternate data streams (we use 13 bits = 8191 max)
 * - Search results need indexing (9 bits = 511 max per batch)
 *
 * By packing these into bitfields, we reduce memory usage significantly when
 * storing millions of search result keys.
 *
 * ## Sentinel Values
 *
 * All-1s in a bitfield indicates "invalid" or "not set":
 * - `name_info() == ~0` means no name info
 * - `stream_info() == ~0` means no stream info
 * - `index() == ~0` means not yet indexed in results
 *
 * ## Usage Example
 *
 * ```cpp
 * // Create a key for FRS 12345, first hardlink, main data stream
 * key_type_internal key(12345, 0, 0);
 *
 * // Access components
 * auto frs = key.frs();           // 12345
 * auto name = key.name_info();    // 0
 * auto stream = key.stream_info(); // 0
 *
 * // Set result index after sorting
 * key.index(42);
 * ```
 *
 * @see NtfsIndex - Uses key_type_internal for search result keys
 * @see Record - The full MFT record structure that keys reference
 */

#ifndef UFFS_NTFS_KEY_TYPE_HPP
#define UFFS_NTFS_KEY_TYPE_HPP

#include <climits>
#include "io/overlapped.hpp"  // for negative_one

namespace uffs {

#pragma pack(push, 1)

/**
 * @brief Packed key for NTFS index entries
 *
 * @details
 * A compact 8-byte structure that uniquely identifies a file/stream in the
 * NTFS index. Uses bitfields to pack multiple indices into minimal space.
 *
 * The key supports comparison for equality, allowing efficient lookup in
 * hash tables or sorted containers.
 *
 * @note This struct is packed to exactly 8 bytes with no padding.
 * @note All-1s values in bitfields are treated as "invalid" sentinels.
 */
struct key_type_internal
{
	// ========================================================================
	// Bitfield Size Constants
	// ========================================================================
	// These constants define the bit allocation for each packed field.
	// The total must equal 32 bits (sizeof(unsigned int) * CHAR_BIT).
	//
	// Allocation:
	//   name_info:   10 bits -> max 1023 hardlinks (all-1s = sentinel)
	//   stream_info: 13 bits -> max 8191 streams (all-1s = sentinel)
	//   index:        9 bits -> max 511 results per batch (all-1s = sentinel)
	// ========================================================================
	enum
	{
		name_info_bits = 10,   ///< Bits for hardlink name index (max 1023)
		stream_info_bits = 13, ///< Bits for stream index (max 8191)
		index_bits = sizeof(unsigned int) * CHAR_BIT - (name_info_bits + stream_info_bits) ///< Remaining bits for result index (9 bits = max 511)
	};

	// ========================================================================
	// Type Aliases
	// ========================================================================
	typedef unsigned int frs_type;         ///< File Record Segment number (32-bit)
	typedef unsigned short name_info_type; ///< Hardlink name index
	typedef unsigned short stream_info_type; ///< Alternate data stream index
	typedef unsigned short index_type;     ///< Search result index

	// ========================================================================
	// Accessors
	// ========================================================================

	/**
	 * @brief Get the File Record Segment (FRS) number
	 * @return The MFT record number identifying the file
	 */
	[[nodiscard]] frs_type frs() const noexcept
	{
		frs_type const result = this->_frs;
		return result;
	}

	/**
	 * @brief Get the hardlink name index
	 * @return Index into the file's name list, or ~0 if invalid/sentinel
	 * @note Returns ~0 (all bits set) if the stored value is the sentinel
	 */
	[[nodiscard]] name_info_type name_info() const noexcept
	{
		name_info_type const result = this->_name_info;
		// If all bits are set (sentinel), return ~0 for the full type
		return result == ((static_cast<name_info_type>(1) << name_info_bits) - 1) ? ~name_info_type() : result;
	}

	/**
	 * @brief Get the alternate data stream index
	 * @return Index into the file's stream list, or ~0 if invalid/sentinel
	 * @note Returns ~0 (all bits set) if the stored value is the sentinel
	 */
	[[nodiscard]] name_info_type stream_info() const noexcept
	{
		stream_info_type const result = this->_stream_info;
		// If all bits are set (sentinel), return ~0 for the full type
		return result == ((static_cast<stream_info_type>(1) << stream_info_bits) - 1) ? ~stream_info_type() : result;
	}

	/**
	 * @brief Set the alternate data stream index
	 * @param value The stream index to store (will be truncated to 13 bits)
	 */
	void stream_info(name_info_type const value) noexcept
	{
		this->_stream_info = value;
	}

	/**
	 * @brief Get the search result index
	 * @return Position in sorted search results, or ~0 if not yet indexed
	 * @note Returns ~0 (all bits set) if the stored value is the sentinel
	 */
	[[nodiscard]] index_type index() const noexcept
	{
		index_type const result = this->_index;
		// If all bits are set (sentinel), return ~0 for the full type
		return result == ((static_cast<index_type>(1) << index_bits) - 1) ? ~index_type() : result;
	}

	/**
	 * @brief Set the search result index
	 * @param value The result index to store (will be truncated to 9 bits)
	 */
	void index(name_info_type const value) noexcept
	{
		this->_index = value;
	}

	// ========================================================================
	// Constructor
	// ========================================================================

	/**
	 * @brief Construct a key from FRS, name index, and stream index
	 * @param frs The File Record Segment number (MFT record number)
	 * @param name_info Index of the hardlink name (0 for primary name)
	 * @param stream_info Index of the data stream (0 for main $DATA stream)
	 * @note The result index is initialized to sentinel (~0 = not indexed)
	 */
	explicit key_type_internal(
		frs_type const frs,
		name_info_type const name_info,
		stream_info_type const stream_info) noexcept
		: _frs(frs), _name_info(name_info), _stream_info(stream_info), _index(negative_one) {}

	// ========================================================================
	// Comparison
	// ========================================================================

	/**
	 * @brief Compare two keys for equality
	 * @param other The key to compare against
	 * @return true if FRS, name_info, and stream_info all match
	 * @note The index field is NOT compared (it's result-specific)
	 */
	[[nodiscard]] bool operator==(key_type_internal const& other) const noexcept
	{
		return this->_frs == other._frs &&
		       this->_name_info == other._name_info &&
		       this->_stream_info == other._stream_info;
	}

private:
	// ========================================================================
	// Data Members (packed, no padding)
	// ========================================================================
	frs_type _frs;                            ///< File Record Segment number (4 bytes)
	name_info_type _name_info : name_info_bits;     ///< Hardlink name index (10 bits)
	stream_info_type _stream_info : stream_info_bits; ///< Stream index (13 bits)
	index_type _index : index_bits;           ///< Result index (9 bits)
};

#pragma pack(pop)

} // namespace uffs

// Backward compatibility
using uffs::key_type_internal;

#endif // UFFS_NTFS_KEY_TYPE_HPP

