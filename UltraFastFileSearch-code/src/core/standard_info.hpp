#pragma once
// standard_info.hpp - NTFS $STANDARD_INFORMATION attribute representation
// Extracted from ntfs_index.hpp for reusability

#include <Windows.h>
#include "file_attributes_ext.hpp"

namespace uffs {

/// Compact representation of NTFS $STANDARD_INFORMATION attribute
/// Uses bitfields to pack file attributes efficiently
struct StandardInfo
{
	unsigned long long
		created,
		written,
		accessed           : 0x40 - 6,
		is_readonly        : 1,
		is_archive         : 1,
		is_system          : 1,
		is_hidden          : 1,
		is_offline         : 1,
		is_notcontentidx   : 1,
		is_noscrubdata     : 1,
		is_integretystream : 1,
		is_pinned          : 1,
		is_unpinned        : 1,
		is_directory       : 1,
		is_compressed      : 1,
		is_encrypted       : 1,
		is_sparsefile      : 1,
		is_reparsepoint    : 1;

	/// Get file attributes as a Windows FILE_ATTRIBUTE_* bitmask
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

	/// Set file attributes from a Windows FILE_ATTRIBUTE_* bitmask
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

// Backward compatibility - expose at global scope
using uffs::StandardInfo;

