#pragma once
/**
 * @file standard_info.hpp
 * @brief NTFS $STANDARD_INFORMATION attribute representation
 *
 * @details
 * This file contains `StandardInfo`, a compact representation of the NTFS
 * $STANDARD_INFORMATION attribute. This attribute is present in every MFT
 * record and contains:
 *
 * - File timestamps (created, modified, accessed)
 * - File attributes (readonly, hidden, system, etc.)
 *
 * ## NTFS $STANDARD_INFORMATION On-Disk Format
 *
 * The on-disk format is defined in `ntfs_types.hpp` as `StandardInformation`:
 *
 * ```
 * Offset  Size   Field
 * ------  -----  ------------------
 * 0       8      CreationTime (FILETIME)
 * 8       8      LastModificationTime (FILETIME)
 * 16      8      LastChangeTime (FILETIME)
 * 24      8      LastAccessTime (FILETIME)
 * 32      4      FileAttributes (DWORD)
 * ...     ...    (additional fields in NTFS 3.0+)
 * ```
 *
 * ## Memory-Optimized Format
 *
 * `StandardInfo` uses bitfields to pack the data more efficiently:
 *
 * ```
 * Offset  Size   Field
 * ------  -----  ------------------
 * 0       8      created (FILETIME)
 * 8       8      written (FILETIME)
 * 16      8      accessed (58 bits) + attribute flags (15 bits)
 * ------  -----  ------------------
 * Total:  24 bytes (vs 36+ bytes on disk)
 * ```
 *
 * The `accessed` timestamp uses only 58 bits (still ~9 million years of
 * precision), leaving 6 bits in that word plus 9 bits in the next word
 * for attribute flags.
 *
 * ## Attribute Flags
 *
 * The following attributes are stored as single-bit flags:
 *
 * | Flag              | FILE_ATTRIBUTE_*        |
 * |-------------------|-------------------------|
 * | is_readonly       | READONLY                |
 * | is_archive        | ARCHIVE                 |
 * | is_system         | SYSTEM                  |
 * | is_hidden         | HIDDEN                  |
 * | is_offline        | OFFLINE                 |
 * | is_notcontentidx  | NOT_CONTENT_INDEXED     |
 * | is_noscrubdata    | NO_SCRUB_DATA           |
 * | is_integretystream| INTEGRITY_STREAM        |
 * | is_pinned         | PINNED                  |
 * | is_unpinned       | UNPINNED                |
 * | is_directory      | DIRECTORY               |
 * | is_compressed     | COMPRESSED              |
 * | is_encrypted      | ENCRYPTED               |
 * | is_sparsefile     | SPARSE_FILE             |
 * | is_reparsepoint   | REPARSE_POINT           |
 *
 * @see ntfs::StandardInformation - The on-disk NTFS format
 * @see file_attributes_ext.hpp - Extended attribute constants
 * @see Record - Uses StandardInfo for file metadata
 */

#include <Windows.h>
#include "file_attributes_ext.hpp"

namespace uffs {

/**
 * @brief Compact representation of NTFS $STANDARD_INFORMATION attribute
 *
 * @details
 * Uses bitfields to pack file timestamps and attributes into 24 bytes.
 * The `accessed` timestamp shares its 64-bit word with attribute flags.
 *
 * ## Bitfield Layout
 *
 * ```
 * Word 0 (64 bits): created timestamp
 * Word 1 (64 bits): written timestamp
 * Word 2 (64 bits):
 *   [0:57]  accessed timestamp (58 bits)
 *   [58]    is_readonly
 *   [59]    is_archive
 *   [60]    is_system
 *   [61]    is_hidden
 *   [62]    is_offline
 *   [63]    is_notcontentidx
 * Word 3 (partial, 9 bits):
 *   [0]     is_noscrubdata
 *   [1]     is_integretystream
 *   [2]     is_pinned
 *   [3]     is_unpinned
 *   [4]     is_directory
 *   [5]     is_compressed
 *   [6]     is_encrypted
 *   [7]     is_sparsefile
 *   [8]     is_reparsepoint
 * ```
 *
 * @note Timestamps are in Windows FILETIME format (100-nanosecond intervals since 1601)
 */
struct StandardInfo
{
	// ========================================================================
	// Timestamps and Attribute Bitfields
	// ========================================================================
	// The accessed timestamp is truncated to 58 bits to make room for flags.
	// 58 bits of 100ns intervals = ~9.1 million years, more than enough.
	// ========================================================================
	unsigned long long
		created,                       ///< Creation time (FILETIME, 64 bits)
		written,                       ///< Last write time (FILETIME, 64 bits)
		accessed           : 0x40 - 6, ///< Last access time (58 bits)
		is_readonly        : 1,        ///< FILE_ATTRIBUTE_READONLY
		is_archive         : 1,        ///< FILE_ATTRIBUTE_ARCHIVE
		is_system          : 1,        ///< FILE_ATTRIBUTE_SYSTEM
		is_hidden          : 1,        ///< FILE_ATTRIBUTE_HIDDEN
		is_offline         : 1,        ///< FILE_ATTRIBUTE_OFFLINE
		is_notcontentidx   : 1,        ///< FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
		is_noscrubdata     : 1,        ///< FILE_ATTRIBUTE_NO_SCRUB_DATA
		is_integretystream : 1,        ///< FILE_ATTRIBUTE_INTEGRITY_STREAM
		is_pinned          : 1,        ///< FILE_ATTRIBUTE_PINNED
		is_unpinned        : 1,        ///< FILE_ATTRIBUTE_UNPINNED
		is_directory       : 1,        ///< FILE_ATTRIBUTE_DIRECTORY
		is_compressed      : 1,        ///< FILE_ATTRIBUTE_COMPRESSED
		is_encrypted       : 1,        ///< FILE_ATTRIBUTE_ENCRYPTED
		is_sparsefile      : 1,        ///< FILE_ATTRIBUTE_SPARSE_FILE
		is_reparsepoint    : 1;        ///< FILE_ATTRIBUTE_REPARSE_POINT

	// ========================================================================
	// Attribute Accessors
	// ========================================================================

	/**
	 * @brief Get file attributes as a Windows FILE_ATTRIBUTE_* bitmask
	 *
	 * @return Combined attribute flags as a DWORD
	 *
	 * @details
	 * Reconstructs the standard Windows attribute bitmask from the
	 * individual bitfield flags. This can be used with Windows APIs
	 * that expect FILE_ATTRIBUTE_* values.
	 */
	[[nodiscard]] unsigned long attributes() const noexcept
	{
		return (this->is_readonly     ? FILE_ATTRIBUTE_READONLY            : 0U) |
			(this->is_archive         ? FILE_ATTRIBUTE_ARCHIVE             : 0U) |
			(this->is_system          ? FILE_ATTRIBUTE_SYSTEM              : 0U) |
			(this->is_hidden          ? FILE_ATTRIBUTE_HIDDEN              : 0U) |
			(this->is_offline         ? FILE_ATTRIBUTE_OFFLINE             : 0U) |
			(this->is_notcontentidx   ? FILE_ATTRIBUTE_NOT_CONTENT_INDEXED : 0U) |
			(this->is_noscrubdata     ? FILE_ATTRIBUTE_NO_SCRUB_DATA       : 0U) |
			(this->is_integretystream ? FILE_ATTRIBUTE_INTEGRITY_STREAM    : 0U) |
			(this->is_pinned          ? FILE_ATTRIBUTE_PINNED              : 0U) |
			(this->is_unpinned        ? FILE_ATTRIBUTE_UNPINNED            : 0U) |
			(this->is_directory       ? FILE_ATTRIBUTE_DIRECTORY           : 0U) |
			(this->is_compressed      ? FILE_ATTRIBUTE_COMPRESSED          : 0U) |
			(this->is_encrypted       ? FILE_ATTRIBUTE_ENCRYPTED           : 0U) |
			(this->is_sparsefile      ? FILE_ATTRIBUTE_SPARSE_FILE         : 0U) |
			(this->is_reparsepoint    ? FILE_ATTRIBUTE_REPARSE_POINT       : 0U);
	}

	/**
	 * @brief Set file attributes from a Windows FILE_ATTRIBUTE_* bitmask
	 *
	 * @param value Combined attribute flags as a DWORD
	 *
	 * @details
	 * Unpacks the standard Windows attribute bitmask into individual
	 * bitfield flags. Call this when loading data from NTFS.
	 */
	void attributes(unsigned long const value) noexcept
	{
		this->is_readonly        = !!(value & FILE_ATTRIBUTE_READONLY);
		this->is_archive         = !!(value & FILE_ATTRIBUTE_ARCHIVE);
		this->is_system          = !!(value & FILE_ATTRIBUTE_SYSTEM);
		this->is_hidden          = !!(value & FILE_ATTRIBUTE_HIDDEN);
		this->is_offline         = !!(value & FILE_ATTRIBUTE_OFFLINE);
		this->is_notcontentidx   = !!(value & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
		this->is_noscrubdata     = !!(value & FILE_ATTRIBUTE_NO_SCRUB_DATA);
		this->is_integretystream = !!(value & FILE_ATTRIBUTE_INTEGRITY_STREAM);
		this->is_pinned          = !!(value & FILE_ATTRIBUTE_PINNED);
		this->is_unpinned        = !!(value & FILE_ATTRIBUTE_UNPINNED);
		this->is_directory       = !!(value & FILE_ATTRIBUTE_DIRECTORY);
		this->is_compressed      = !!(value & FILE_ATTRIBUTE_COMPRESSED);
		this->is_encrypted       = !!(value & FILE_ATTRIBUTE_ENCRYPTED);
		this->is_sparsefile      = !!(value & FILE_ATTRIBUTE_SPARSE_FILE);
		this->is_reparsepoint    = !!(value & FILE_ATTRIBUTE_REPARSE_POINT);
	}
};

} // namespace uffs

// ============================================================================
// Backward Compatibility
// ============================================================================
// Expose at global scope for existing code that doesn't use uffs::
// ============================================================================
using uffs::StandardInfo;

