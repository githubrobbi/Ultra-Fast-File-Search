#pragma once
/**
 * @file ntfs_record_types.hpp
 * @brief NTFS MFT record component types for the in-memory file index
 *
 * @details
 * This file defines the core data structures used to represent NTFS file
 * system entries in memory. These structures are optimized for:
 *
 * 1. **Memory efficiency** - Using packed structs and small integer types
 * 2. **Fast traversal** - Linked lists via indices instead of pointers
 * 3. **Complete NTFS support** - Hardlinks, alternate data streams, directories
 *
 * ## Data Structure Overview
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                         NtfsIndex                                   │
 * │  ┌─────────────────────────────────────────────────────────────┐   │
 * │  │ records_data: vector<Record>                                 │   │
 * │  │   [0] Record ──┬── first_name ──► LinkInfo ──► LinkInfo     │   │
 * │  │   [1] Record   │   first_stream ──► StreamInfo ──► ...      │   │
 * │  │   ...          │   first_child ──► (index into childinfos)  │   │
 * │  └────────────────┴────────────────────────────────────────────┘   │
 * │  ┌─────────────────────────────────────────────────────────────┐   │
 * │  │ childinfos: vector<ChildInfo>                                │   │
 * │  │   Linked list of directory children                          │   │
 * │  └─────────────────────────────────────────────────────────────┘   │
 * │  ┌─────────────────────────────────────────────────────────────┐   │
 * │  │ nameinfos: vector<LinkInfo>                                  │   │
 * │  │   Additional hardlink names (beyond first_name)              │   │
 * │  └─────────────────────────────────────────────────────────────┘   │
 * │  ┌─────────────────────────────────────────────────────────────┐   │
 * │  │ streaminfos: vector<StreamInfo>                              │   │
 * │  │   Additional streams (beyond first_stream)                   │   │
 * │  └─────────────────────────────────────────────────────────────┘   │
 * │  ┌─────────────────────────────────────────────────────────────┐   │
 * │  │ names: vector<char>                                          │   │
 * │  │   Packed filename storage (ASCII or UTF-16)                  │   │
 * │  └─────────────────────────────────────────────────────────────┘   │
 * └─────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Linked List Pattern
 *
 * Instead of using pointers, linked lists use indices into vectors:
 * - `next_entry = ~0` (all bits set) means "end of list"
 * - This saves memory (4 bytes vs 8 bytes on 64-bit) and enables
 *   efficient serialization
 *
 * ## Memory Layout
 *
 * All structures use `#pragma pack(push, 1)` to eliminate padding:
 *
 * | Struct     | Size   | Description                              |
 * |------------|--------|------------------------------------------|
 * | NameInfo   | 5 bytes| Offset into names buffer + length        |
 * | LinkInfo   | 13 bytes| Hardlink: next + name + parent FRS      |
 * | StreamInfo | 17 bytes| ADS: size info + next + name + flags    |
 * | ChildInfo  | 10 bytes| Directory child: next + FRS + name idx  |
 * | Record     | 51 bytes| Full MFT record representation          |
 *
 * @see NtfsIndex - The main index class that uses these structures
 * @see StandardInfo - File timestamps and attributes
 * @see SizeInfo - File size information
 */

#include <climits>
#include "../io/overlapped.hpp"  // for negative_one
#include "packed_file_size.hpp"
#include "standard_info.hpp"     // for StandardInfo

namespace uffs {

/**
 * @brief Small type optimization for size_t
 *
 * @details
 * On most platforms, we can use `unsigned int` (4 bytes) instead of
 * `size_t` (8 bytes on 64-bit) for indices, since we won't have more
 * than 4 billion files on a single volume.
 *
 * This saves 4 bytes per index, which adds up significantly when
 * storing millions of file records.
 */
template <class = void>
struct small_t
{
	typedef unsigned int type; ///< 4-byte unsigned integer for indices
};

#pragma pack(push, 1)

/**
 * @brief Name information - offset into names buffer with ASCII flag
 *
 * @details
 * Stores a reference to a filename in the packed names buffer.
 * Uses bit-packing to store both the offset and an ASCII flag in a single
 * 4-byte integer:
 *
 * ```
 * _offset layout (32 bits):
 * ┌────────────────────────────────────────────────────────┬───┐
 * │                    offset (31 bits)                    │ A │
 * └────────────────────────────────────────────────────────┴───┘
 *                                                           └── ASCII flag (1 bit)
 * ```
 *
 * - **Bit 0**: ASCII flag (1 = ASCII, 0 = UTF-16)
 * - **Bits 1-31**: Offset into the names buffer (divided by 2)
 *
 * The `length` field stores the character count (not byte count).
 * For ASCII names, byte count = length. For UTF-16, byte count = length * 2.
 *
 * @note Offset of ~0 (all bits set) indicates "no name" / invalid.
 */
struct NameInfo
{
	small_t<size_t>::type _offset; ///< Packed offset (bits 1-31) + ASCII flag (bit 0)

	/**
	 * @brief Check if the name is stored as ASCII
	 * @return true if ASCII (1 byte per char), false if UTF-16 (2 bytes per char)
	 */
	[[nodiscard]] bool ascii() const noexcept
	{
		return !!(this->_offset & 1U);
	}

	/**
	 * @brief Set the ASCII flag
	 * @param value true for ASCII encoding, false for UTF-16
	 */
	void ascii(bool const value) noexcept
	{
		this->_offset = static_cast<small_t<size_t>::type>(
			(this->_offset & static_cast<small_t<size_t>::type>(~static_cast<small_t<size_t>::type>(1U))) |
			(value ? 1U : small_t<size_t>::type()));
	}

	/**
	 * @brief Get the offset into the names buffer
	 * @return Byte offset, or ~0 if invalid/no name
	 */
	[[nodiscard]] small_t<size_t>::type offset() const noexcept
	{
		small_t<size_t>::type result = this->_offset >> 1;
		if (result == (static_cast<small_t<size_t>::type>(negative_one) >> 1))
		{
			result = static_cast<small_t<size_t>::type>(negative_one);
		}
		return result;
	}

	/**
	 * @brief Set the offset into the names buffer
	 * @param value Byte offset (will be shifted left by 1 to preserve ASCII flag)
	 */
	void offset(small_t<size_t>::type const value) noexcept
	{
		this->_offset = (value << 1) | (this->_offset & 1U);
	}

	unsigned char length; ///< Character count (not byte count)
};

/**
 * @brief Hard link information - links a file record to a parent directory
 *
 * @details
 * NTFS supports multiple hardlinks to the same file. Each hardlink has:
 * - A name (stored in the names buffer)
 * - A parent directory (FRS number)
 *
 * Hardlinks form a linked list via `next_entry`:
 *
 * ```
 * Record.first_name ──► LinkInfo ──► LinkInfo ──► ... ──► (next_entry = ~0)
 *                         │            │
 *                         ▼            ▼
 *                       name         name
 *                       parent       parent
 * ```
 *
 * @note `next_entry = ~0` indicates end of list
 * @note `name.offset() = ~0` indicates no name (invalid entry)
 */
struct LinkInfo
{
	/**
	 * @brief Default constructor - initializes to "no link" state
	 */
	LinkInfo() noexcept : next_entry(negative_one)
	{
		this->name.offset(negative_one);
	}

	typedef small_t<size_t>::type next_entry_type;
	next_entry_type next_entry; ///< Index of next LinkInfo, or ~0 for end of list
	NameInfo name;              ///< Reference to filename in names buffer
	unsigned int parent;        ///< FRS number of parent directory
};

/**
 * @brief NTFS stream information - alternate data streams (ADS)
 *
 * @details
 * NTFS files can have multiple data streams. The main stream is unnamed
 * (accessed as "file.txt"), while alternate streams have names
 * (accessed as "file.txt:streamname").
 *
 * Inherits from SizeInfo to store length, allocated size, and bulkiness.
 *
 * Streams form a linked list via `next_entry`:
 *
 * ```
 * Record.first_stream ──► StreamInfo ──► StreamInfo ──► ... ──► (next_entry = ~0)
 *                            │              │
 *                            ▼              ▼
 *                          name           name
 *                          length         length
 *                          allocated      allocated
 * ```
 *
 * @note The main $DATA stream typically has an empty name
 * @note `type_name_id = 0` for $I30:$INDEX_ROOT or $I30:$INDEX_ALLOCATION
 */
struct StreamInfo : SizeInfo
{
	/**
	 * @brief Default constructor - initializes to empty stream
	 */
	StreamInfo() noexcept : SizeInfo(), next_entry(), name(), type_name_id() {}

	typedef small_t<size_t>::type next_entry_type;
	next_entry_type next_entry; ///< Index of next StreamInfo, or ~0 for end of list
	NameInfo name;              ///< Stream name (empty for main $DATA stream)
	unsigned char is_sparse : 1; ///< Stream has sparse regions (unallocated holes)
	unsigned char is_allocated_size_accounted_for_in_main_stream : 1; ///< Size already counted in main stream
	unsigned char type_name_id : CHAR_BIT - 2; ///< Attribute type ID (0 for $I30 index streams)
};

/**
 * @brief Child directory entry information
 *
 * @details
 * For directories, this structure links to child files/subdirectories.
 * Children form a linked list via `next_entry`:
 *
 * ```
 * Record.first_child ──► ChildInfo ──► ChildInfo ──► ... ──► (next_entry = ~0)
 *   (index into           │              │
 *    childinfos)          ▼              ▼
 *                    record_number  record_number
 *                    name_index     name_index
 * ```
 *
 * @note `record_number` is the FRS of the child file/directory
 * @note `name_index` is which hardlink name to use for display
 */
struct ChildInfo
{
	/**
	 * @brief Default constructor - initializes to "no child" state
	 */
	ChildInfo() noexcept : next_entry(negative_one), record_number(negative_one), name_index(negative_one) {}

	typedef small_t<size_t>::type next_entry_type;
	next_entry_type next_entry;            ///< Index of next ChildInfo, or ~0 for end of list
	small_t<size_t>::type record_number;   ///< FRS number of the child file/directory
	unsigned short name_index;             ///< Which hardlink name to use (index into child's names)
};

/**
 * @brief MFT file record - represents a single file/directory in the NTFS index
 *
 * @details
 * This is the main structure representing a file or directory in the in-memory
 * index. It corresponds to an NTFS FILE_RECORD_SEGMENT_HEADER but is optimized
 * for search and traversal.
 *
 * ## Memory Layout (51 bytes, packed)
 *
 * ```
 * Offset  Size   Field
 * ------  -----  ------------------
 * 0       24     stdinfo (StandardInfo - timestamps + attributes)
 * 24      2      name_count
 * 26      2      stream_count
 * 28      4      first_child (index into childinfos, or ~0)
 * 32      13     first_name (embedded LinkInfo)
 * 45      17     first_stream (embedded StreamInfo)
 * ------  -----  ------------------
 * Total:  62 bytes (approximate, depends on StandardInfo packing)
 * ```
 *
 * ## Linked List Heads
 *
 * The Record contains the first element of each linked list inline:
 * - `first_name` - First hardlink (additional links in nameinfos vector)
 * - `first_stream` - First data stream (additional streams in streaminfos vector)
 * - `first_child` - Index to first child (for directories only)
 *
 * This optimization avoids an extra indirection for the common case of
 * files with one name and one stream.
 *
 * ## Usage Example
 *
 * ```cpp
 * // Access file timestamps
 * auto created = record.stdinfo.created;
 * auto attrs = record.stdinfo.attributes();
 *
 * // Iterate hardlinks
 * for (auto* link = &record.first_name; link; link = next_link(link)) {
 *     // link->name, link->parent
 * }
 *
 * // Check if directory
 * if (record.stdinfo.is_directory) {
 *     // Iterate children via first_child
 * }
 * ```
 */
struct Record
{
	StandardInfo stdinfo;                    ///< File timestamps and attributes
	unsigned short name_count;               ///< Number of hardlinks (max ~1024)
	unsigned short stream_count;             ///< Number of data streams (max ~4106)
	ChildInfo::next_entry_type first_child;  ///< Index of first child (directories only), or ~0
	LinkInfo first_name;                     ///< First hardlink (embedded, not a pointer)
	StreamInfo first_stream;                 ///< First data stream (embedded, not a pointer)

	/**
	 * @brief Default constructor - initializes to empty record
	 *
	 * Sets all linked list heads to "empty" state (~0 for indices,
	 * invalid offset for names).
	 */
	Record() noexcept
		: stdinfo()
		, name_count()
		, stream_count()
		, first_child(negative_one)
		, first_name()
		, first_stream()
	{
		this->first_stream.name.offset(negative_one);
		this->first_stream.next_entry = negative_one;
	}
};

#pragma pack(pop)

} // namespace uffs

// ============================================================================
// Backward Compatibility
// ============================================================================
// Expose types at global scope for existing code that doesn't use uffs::
// ============================================================================
using uffs::small_t;
using uffs::NameInfo;
using uffs::LinkInfo;
using uffs::StreamInfo;
using uffs::ChildInfo;
using uffs::Record;

