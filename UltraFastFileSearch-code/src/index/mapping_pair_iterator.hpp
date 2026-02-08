#pragma once
/**
 * @file mapping_pair_iterator.hpp
 * @brief NTFS mapping pairs parser for non-resident attribute data runs
 *
 * @details
 * This file contains `mapping_pair_iterator`, an iterator for parsing NTFS
 * "mapping pairs" - the run-length encoded data that describes where a
 * non-resident attribute's data is stored on disk.
 *
 * ## NTFS Non-Resident Attributes
 *
 * When a file's data is too large to fit in the MFT record, NTFS stores it
 * in "data runs" (also called "extents") scattered across the disk. The
 * locations of these runs are encoded as "mapping pairs" in the attribute
 * record header.
 *
 * ## Mapping Pair Encoding
 *
 * Each mapping pair encodes a (VCN delta, LCN delta) pair in a compact format:
 *
 * ```
 * Byte 0: Header byte
 *   ┌───────────────┬───────────────┐
 *   │  L (4 bits)   │  V (4 bits)   │
 *   └───────────────┴───────────────┘
 *   L = number of bytes for LCN delta (0-15)
 *   V = number of bytes for VCN delta (0-15)
 *
 * Bytes 1..V: VCN delta (little-endian, signed)
 * Bytes V+1..V+L: LCN delta (little-endian, signed)
 * ```
 *
 * The deltas are signed integers stored in variable-length format:
 * - If the high bit of the last byte is set, the value is negative
 * - The value is sign-extended to 64 bits
 *
 * ## VCN and LCN
 *
 * - **VCN (Virtual Cluster Number)**: Logical offset within the attribute data
 * - **LCN (Logical Cluster Number)**: Physical location on disk
 *
 * The mapping pairs describe a sequence of extents:
 *
 * ```
 * Extent 0: VCN [0, next_vcn_0)     at LCN lcn_0
 * Extent 1: VCN [next_vcn_0, next_vcn_1) at LCN lcn_1
 * ...
 * ```
 *
 * ## Sparse Files
 *
 * If L=0 (LCN delta bytes = 0), the extent is a "hole" (sparse region).
 * The LCN remains unchanged, indicating no physical storage.
 *
 * ## Usage Example
 *
 * ```cpp
 * // Parse mapping pairs from a non-resident attribute
 * auto* attr = get_data_attribute(record);
 * if (attr->IsNonResident) {
 *     for (mapping_pair_iterator it(attr); !it.is_final(); ++it) {
 *         auto vcn_end = it->next_vcn;
 *         auto lcn = it->current_lcn;
 *         // Process extent: VCN range [prev_vcn, vcn_end) at LCN lcn
 *     }
 * }
 * ```
 *
 * @see ntfs::AttributeRecordHeader - Contains the mapping pairs data
 * @see MftReader - Uses this to read non-resident MFT data
 */

#include <climits>
#include <cstddef>
#include "io/winnt_types.hpp"

namespace uffs {

/**
 * @brief Iterator for parsing NTFS mapping pairs (run-length encoded VCN/LCN data)
 *
 * @details
 * This iterator decodes the compact mapping pair format used by NTFS to
 * describe non-resident attribute data runs. Each increment advances to
 * the next extent in the run list.
 *
 * The iterator maintains the current LCN as state, since mapping pairs
 * encode LCN deltas (differences from the previous LCN), not absolute values.
 *
 * @note This is a forward-only iterator (no operator--)
 * @note The iterator is valid until `is_final()` returns true
 */
struct mapping_pair_iterator
{
	typedef mapping_pair_iterator this_type;
	typedef long long vcn_type, lcn_type; ///< 64-bit cluster numbers

	// ========================================================================
	// Value Type
	// ========================================================================

	/**
	 * @brief The value returned by dereferencing the iterator
	 *
	 * Contains the end VCN of the current extent and its physical LCN.
	 * The extent covers VCN range [previous next_vcn, this next_vcn).
	 */
	struct value_type
	{
		value_type(vcn_type const next_vcn, lcn_type const current_lcn) noexcept
			: next_vcn(next_vcn), current_lcn(current_lcn) {}

		vcn_type next_vcn;    ///< VCN where this extent ends (exclusive)
		lcn_type current_lcn; ///< Physical cluster number on disk
	};

	// ========================================================================
	// Constructor
	// ========================================================================

	/**
	 * @brief Construct an iterator from a non-resident attribute header
	 *
	 * @param ah Pointer to the attribute record header
	 * @param max_length Maximum bytes to read (for bounds checking)
	 * @param current_lcn Starting LCN (for continuation from previous extent list)
	 *
	 * @details
	 * The iterator starts at the beginning of the mapping pairs data,
	 * which is located at offset `ah->NonResident.MappingPairsOffset`
	 * from the start of the attribute header.
	 *
	 * The initial VCN is set to `ah->NonResident.LowestVCN`, which is
	 * typically 0 for the first attribute record, but may be non-zero
	 * for continuation records in the attribute list.
	 */
	explicit mapping_pair_iterator(::ntfs::ATTRIBUTE_RECORD_HEADER const* const ah,
		size_t const max_length = ~size_t(),
		lcn_type const current_lcn = lcn_type()) noexcept
		: mapping_pairs(reinterpret_cast<unsigned char const*>(ah) +
			static_cast<ptrdiff_t>(ah->NonResident.MappingPairsOffset)),
		  ah_end(reinterpret_cast<unsigned char const*>(ah) +
			(ah->Length < max_length ? ah->Length : static_cast<ptrdiff_t>(max_length))),
		  j(0),
		  value(ah->NonResident.LowestVCN, current_lcn) {}

	// ========================================================================
	// Iterator Interface
	// ========================================================================

	/**
	 * @brief Check if the iterator has reached the end of the mapping pairs
	 *
	 * @return true if no more extents to parse, false otherwise
	 *
	 * @details
	 * The mapping pairs list is terminated by a zero byte or by reaching
	 * the end of the attribute record.
	 */
	[[nodiscard]] bool is_final() const noexcept
	{
		return !(mapping_pairs[j] && &mapping_pairs[j] < ah_end);
	}

	/**
	 * @brief Dereference to get the current extent information
	 * @return Reference to the current (next_vcn, current_lcn) pair
	 */
	[[nodiscard]] value_type const& operator*() const noexcept
	{
		return this->value;
	}

	/**
	 * @brief Arrow operator for member access
	 * @return Pointer to the current value
	 */
	[[nodiscard]] value_type const* operator->() const noexcept
	{
		return &**this;
	}

	/**
	 * @brief Advance to the next extent in the run list
	 *
	 * @return Reference to this iterator
	 *
	 * @details
	 * Parses the next mapping pair from the byte stream:
	 *
	 * 1. Read header byte: low nibble = VCN delta bytes, high nibble = LCN delta bytes
	 * 2. Read VCN delta (variable length, signed, little-endian)
	 * 3. Read LCN delta (variable length, signed, little-endian)
	 * 4. Add deltas to current values
	 *
	 * ## Sign Extension
	 *
	 * The deltas are signed integers. If the high bit of the last byte is set,
	 * the value is negative and must be sign-extended:
	 *
	 * ```
	 * If high bit set:
	 *   delta = ~0ULL << (num_bytes * 8)  // Set all high bits to 1
	 *   delta |= parsed_bytes             // OR in the actual bytes
	 * ```
	 *
	 * ## Example
	 *
	 * Header byte: 0x31 (V=1, L=3)
	 * - VCN delta: 1 byte
	 * - LCN delta: 3 bytes
	 *
	 * If VCN delta byte is 0x10 (16), next_vcn += 16
	 * If LCN delta bytes are 0x00 0x10 0x00 (4096), current_lcn += 4096
	 */
	this_type& operator++() noexcept
	{
		// Read header byte: low nibble = V (VCN bytes), high nibble = L (LCN bytes)
		unsigned char const lv = mapping_pairs[j++];

		// ------------------------------------------------------------------------
		// Parse VCN delta (low nibble specifies byte count)
		// ------------------------------------------------------------------------
		{
			// Extract V from low nibble (bits 0-3)
			unsigned char v = static_cast<unsigned char>(lv & ((1U << (CHAR_BIT / 2)) - 1));

			// Sign extension: if high bit of last byte is set, pre-fill with 1s
			vcn_type delta = v && (mapping_pairs[j + v - 1] >> (CHAR_BIT - 1))
				? static_cast<vcn_type>(~static_cast<unsigned long long>(0) << (v * CHAR_BIT))
				: vcn_type();

			// Read V bytes in little-endian order
			for (unsigned char k = 0; k != v; ++k)
			{
				delta |= static_cast<vcn_type>(mapping_pairs[j++]) << (CHAR_BIT * k);
			}
			value.next_vcn += delta;
		}

		// ------------------------------------------------------------------------
		// Parse LCN delta (high nibble specifies byte count)
		// ------------------------------------------------------------------------
		{
			// Extract L from high nibble (bits 4-7)
			unsigned char l = static_cast<unsigned char>(lv >> (CHAR_BIT / 2));

			// Sign extension: if high bit of last byte is set, pre-fill with 1s
			// Note: L=0 means sparse extent (no physical storage), LCN unchanged
			lcn_type delta = l && (mapping_pairs[j + l - 1] >> (CHAR_BIT - 1))
				? static_cast<lcn_type>(~static_cast<unsigned long long>(0) << (l * CHAR_BIT))
				: lcn_type();

			// Read L bytes in little-endian order
			for (unsigned char k = 0; k != l; ++k)
			{
				delta |= static_cast<lcn_type>(mapping_pairs[j++]) << (CHAR_BIT * k);
			}
			value.current_lcn += delta;
		}

		return *this;
	}

private:
	// ========================================================================
	// Data Members
	// ========================================================================
	unsigned char const* mapping_pairs; ///< Pointer to mapping pairs data
	unsigned char const* ah_end;        ///< End of attribute record (bounds check)
	size_t j;                           ///< Current byte offset in mapping_pairs
	value_type value;                   ///< Current (next_vcn, current_lcn) state
};

} // namespace uffs

// Bring into global namespace for backward compatibility
using uffs::mapping_pair_iterator;

