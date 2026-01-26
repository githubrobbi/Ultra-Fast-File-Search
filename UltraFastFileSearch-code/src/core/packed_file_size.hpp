#pragma once
// packed_file_size.hpp - Compact file size types for NTFS indexing
// Extracted from ntfs_index.hpp for reusability

#include <climits>
#include "../io/overlapped.hpp"  // for value_initialized

namespace uffs {

/// Packed 6-byte file size type (48-bit, supports up to 256 TB)
/// Uses #pragma pack to ensure compact storage in index structures
#pragma pack(push, 1)
class file_size_type
{
	unsigned int low;
	unsigned short high;
	typedef file_size_type this_type;
public:
	file_size_type() noexcept : low(), high() {}

	file_size_type(unsigned long long const value) noexcept
		: low(static_cast<unsigned int>(value))
		, high(static_cast<unsigned short>(value >> (sizeof(low) * CHAR_BIT))) {}

	[[nodiscard]] operator unsigned long long() const noexcept
	{
		return static_cast<unsigned long long>(this->low) |
			(static_cast<unsigned long long>(this->high) << (sizeof(this->low) * CHAR_BIT));
	}

	file_size_type operator+=(this_type const value) noexcept
	{
		return *this = static_cast<unsigned long long>(*this) + static_cast<unsigned long long>(value);
	}

	file_size_type operator-=(this_type const value) noexcept
	{
		return *this = static_cast<unsigned long long>(*this) - static_cast<unsigned long long>(value);
	}

	[[nodiscard]] bool operator!() const noexcept
	{
		return !this->low && !this->high;
	}
};
#pragma pack(pop)

/// Size information for files and streams
struct SizeInfo
{
	file_size_type length, allocated, bulkiness;
	value_initialized<unsigned int>::type treesize;
};

} // namespace uffs

// Backward compatibility - expose at global scope
using uffs::file_size_type;
using uffs::SizeInfo;

