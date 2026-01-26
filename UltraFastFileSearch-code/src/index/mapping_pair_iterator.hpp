#pragma once
// mapping_pair_iterator - NTFS mapping pairs parser
// Extracted from ntfs_index.hpp for reusability
// Parses NTFS non-resident attribute mapping pairs (VCN/LCN runs)

#include <climits>
#include <cstddef>
#include "../io/winnt_types.hpp"

namespace uffs {

/// Iterator for parsing NTFS mapping pairs (run-length encoded VCN/LCN data)
/// Used to decode non-resident attribute data runs
struct mapping_pair_iterator
{
	typedef mapping_pair_iterator this_type;
	typedef long long vcn_type, lcn_type;

	struct value_type
	{
		value_type(vcn_type const next_vcn, lcn_type const current_lcn) noexcept
			: next_vcn(next_vcn), current_lcn(current_lcn) {}

		vcn_type next_vcn;
		lcn_type current_lcn;
	};

	explicit mapping_pair_iterator(::ntfs::ATTRIBUTE_RECORD_HEADER const* const ah,
		size_t const max_length = ~size_t(),
		lcn_type const current_lcn = lcn_type()) noexcept
		: mapping_pairs(reinterpret_cast<unsigned char const*>(ah) +
			static_cast<ptrdiff_t>(ah->NonResident.MappingPairsOffset)),
		  ah_end(reinterpret_cast<unsigned char const*>(ah) +
			(ah->Length < max_length ? ah->Length : static_cast<ptrdiff_t>(max_length))),
		  j(0),
		  value(ah->NonResident.LowestVCN, current_lcn) {}

	[[nodiscard]] bool is_final() const noexcept
	{
		return !(mapping_pairs[j] && &mapping_pairs[j] < ah_end);
	}

	[[nodiscard]] value_type const& operator*() const noexcept
	{
		return this->value;
	}

	[[nodiscard]] value_type const* operator->() const noexcept
	{
		return &**this;
	}

	this_type& operator++() noexcept
	{
		unsigned char const lv = mapping_pairs[j++];

		// Parse VCN delta
		{
			unsigned char v = static_cast<unsigned char>(lv & ((1U << (CHAR_BIT / 2)) - 1));
			vcn_type delta = v && (mapping_pairs[j + v - 1] >> (CHAR_BIT - 1))
				? static_cast<vcn_type>(~static_cast<unsigned long long>(0) << (v * CHAR_BIT))
				: vcn_type();
			for (unsigned char k = 0; k != v; ++k)
			{
				delta |= static_cast<vcn_type>(mapping_pairs[j++]) << (CHAR_BIT * k);
			}
			value.next_vcn += delta;
		}

		// Parse LCN delta
		{
			unsigned char l = static_cast<unsigned char>(lv >> (CHAR_BIT / 2));
			lcn_type delta = l && (mapping_pairs[j + l - 1] >> (CHAR_BIT - 1))
				? static_cast<lcn_type>(~static_cast<unsigned long long>(0) << (l * CHAR_BIT))
				: lcn_type();
			for (unsigned char k = 0; k != l; ++k)
			{
				delta |= static_cast<lcn_type>(mapping_pairs[j++]) << (CHAR_BIT * k);
			}
			value.current_lcn += delta;
		}

		return *this;
	}

private:
	unsigned char const* mapping_pairs;
	unsigned char const* ah_end;
	size_t j;
	value_type value;
};

} // namespace uffs

// Bring into global namespace for backward compatibility
using uffs::mapping_pair_iterator;

