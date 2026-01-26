#pragma once
/**
 * @file ntfs_key_type.hpp
 * @brief Packed key type for NTFS index entries
 * 
 * Contains key_type_internal - a compact representation of an NTFS index key
 * using bitfields to minimize memory usage while supporting:
 * - Up to 1023 hardlinks per file (10 bits)
 * - Up to 4107 streams per file (13 bits)
 * - Index into sorted results (9 bits)
 * 
 * Extracted from ntfs_index.hpp for reusability.
 */

#ifndef UFFS_NTFS_KEY_TYPE_HPP
#define UFFS_NTFS_KEY_TYPE_HPP

#include <climits>
#include "../io/overlapped.hpp"  // for negative_one

namespace uffs {

#pragma pack(push, 1)

/**
 * @brief Packed key for NTFS index entries
 * 
 * Uses bitfields to pack FRS number, name info index, stream info index,
 * and result index into a compact representation.
 */
struct key_type_internal
{
	// max 1023 hardlinks
	// max 4107 streams
	enum
	{
		name_info_bits = 10,
		stream_info_bits = 13,
		index_bits = sizeof(unsigned int) * CHAR_BIT - (name_info_bits + stream_info_bits)
	};

	typedef unsigned int frs_type;
	typedef unsigned short name_info_type;
	typedef unsigned short stream_info_type;
	typedef unsigned short index_type;

	[[nodiscard]] frs_type frs() const noexcept
	{
		frs_type const result = this->_frs;
		return result;
	}

	[[nodiscard]] name_info_type name_info() const noexcept
	{
		name_info_type const result = this->_name_info;
		return result == ((static_cast<name_info_type>(1) << name_info_bits) - 1) ? ~name_info_type() : result;
	}

	[[nodiscard]] name_info_type stream_info() const noexcept
	{
		stream_info_type const result = this->_stream_info;
		return result == ((static_cast<stream_info_type>(1) << stream_info_bits) - 1) ? ~stream_info_type() : result;
	}

	void stream_info(name_info_type const value) noexcept
	{
		this->_stream_info = value;
	}

	[[nodiscard]] index_type index() const noexcept
	{
		index_type const result = this->_index;
		return result == ((static_cast<index_type>(1) << index_bits) - 1) ? ~index_type() : result;
	}

	void index(name_info_type const value) noexcept
	{
		this->_index = value;
	}

	explicit key_type_internal(
		frs_type const frs,
		name_info_type const name_info,
		stream_info_type const stream_info) noexcept
		: _frs(frs), _name_info(name_info), _stream_info(stream_info), _index(negative_one) {}

	[[nodiscard]] bool operator==(key_type_internal const& other) const noexcept
	{
		return this->_frs == other._frs &&
		       this->_name_info == other._name_info &&
		       this->_stream_info == other._stream_info;
	}

private:
	frs_type _frs;
	name_info_type _name_info : name_info_bits;
	stream_info_type _stream_info : stream_info_bits;
	index_type _index : index_bits;
};

#pragma pack(pop)

} // namespace uffs

// Backward compatibility
using uffs::key_type_internal;

#endif // UFFS_NTFS_KEY_TYPE_HPP

