#pragma once
// ntfs_record_types.hpp - NTFS MFT record component types
// Extracted from ntfs_index.hpp for reusability

#include <climits>
#include "../io/overlapped.hpp"  // for negative_one
#include "packed_file_size.hpp"
#include "standard_info.hpp"     // for StandardInfo

namespace uffs {

/// Small type optimization - uses unsigned int for size_t on most platforms
template <class = void>
struct small_t
{
	typedef unsigned int type;
};

#pragma pack(push, 1)

/// Name information - offset into names buffer with ASCII flag
struct NameInfo
{
	small_t<size_t>::type _offset;

	[[nodiscard]] bool ascii() const noexcept
	{
		return !!(this->_offset & 1U);
	}

	void ascii(bool const value) noexcept
	{
		this->_offset = static_cast<small_t<size_t>::type>(
			(this->_offset & static_cast<small_t<size_t>::type>(~static_cast<small_t<size_t>::type>(1U))) |
			(value ? 1U : small_t<size_t>::type()));
	}

	[[nodiscard]] small_t<size_t>::type offset() const noexcept
	{
		small_t<size_t>::type result = this->_offset >> 1;
		if (result == (static_cast<small_t<size_t>::type>(negative_one) >> 1))
		{
			result = static_cast<small_t<size_t>::type>(negative_one);
		}
		return result;
	}

	void offset(small_t<size_t>::type const value) noexcept
	{
		this->_offset = (value << 1) | (this->_offset & 1U);
	}

	unsigned char length;
};

/// Hard link information - links a file record to a parent directory
struct LinkInfo
{
	LinkInfo() noexcept : next_entry(negative_one)
	{
		this->name.offset(negative_one);
	}

	typedef small_t<size_t>::type next_entry_type;
	next_entry_type next_entry;
	NameInfo name;
	unsigned int parent;
};

/// NTFS stream information - alternate data streams
struct StreamInfo : SizeInfo
{
	StreamInfo() noexcept : SizeInfo(), next_entry(), name(), type_name_id() {}

	typedef small_t<size_t>::type next_entry_type;
	next_entry_type next_entry;
	NameInfo name;
	unsigned char is_sparse : 1;
	unsigned char is_allocated_size_accounted_for_in_main_stream : 1;
	unsigned char type_name_id : CHAR_BIT - 2; // zero if $I30:$INDEX_ROOT or $I30:$INDEX_ALLOCATION
};

/// Child directory entry information
struct ChildInfo
{
	ChildInfo() noexcept : next_entry(negative_one), record_number(negative_one), name_index(negative_one) {}

	typedef small_t<size_t>::type next_entry_type;
	next_entry_type next_entry;
	small_t<size_t>::type record_number;
	unsigned short name_index;
};

/// MFT file record - represents a single file/directory in the NTFS index
/// Contains standard info, name count, stream count, and links to child/name/stream lists
struct Record
{
	StandardInfo stdinfo;
	unsigned short name_count /*<= 1024 < 2048 */, stream_count /*<= 4106?<8192 */;
	ChildInfo::next_entry_type first_child;
	LinkInfo first_name;
	StreamInfo first_stream;

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

// Backward compatibility
using uffs::small_t;
using uffs::NameInfo;
using uffs::LinkInfo;
using uffs::StreamInfo;
using uffs::ChildInfo;
using uffs::Record;

