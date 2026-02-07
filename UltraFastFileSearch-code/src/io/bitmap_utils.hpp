/**
 * @file bitmap_utils.hpp
 * @brief Bitmap manipulation utilities for MFT processing.
 *
 * This header contains functions for working with MFT bitmaps:
 * - Population count (counting set bits)
 * - Finding first/last set bits in a range
 *
 * These are pure functions with no side effects, making them
 * easy to test and reuse.
 *
 * @note Separated from mft_reader.hpp for single-responsibility and testability.
 */

#ifndef UFFS_BITMAP_UTILS_HPP
#define UFFS_BITMAP_UTILS_HPP

#include "mft_reader_constants.hpp"
#include <cstddef>
#include <vector>

namespace bitmap_utils {

/**
 * @brief Counts set bits (1s) in a buffer using nibble-based popcount.
 *
 * Algorithm:
 * - Split each byte into two 4-bit nibbles
 * - Look up popcount for each nibble in precomputed table
 * - Sum all popcounts
 *
 * This is faster than bit-by-bit counting and works on any platform.
 *
 * @param buffer  Pointer to bitmap data
 * @param size    Number of bytes to process
 * @return Total number of set bits
 */
[[nodiscard]] inline unsigned int count_bits_in_buffer(
    const void* buffer,
    size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(buffer);
    unsigned int count = 0;

    for (size_t i = 0; i < size; ++i)
    {
        const unsigned char byte = bytes[i];

        // Split byte into high and low nibbles
        const unsigned char low_nibble = byte & 0x0F;
        const unsigned char high_nibble = (byte >> 4) & 0x0F;

        // Look up popcount for each nibble
        count += mft_reader_constants::kNibblePopCount[low_nibble];
        count += mft_reader_constants::kNibblePopCount[high_nibble];
    }

    return count;
}

/**
 * @brief Finds the first set bit in a bitmap range.
 *
 * Scans from the start of the range until finding a set bit.
 *
 * @param bitmap        The bitmap (one bit per record)
 * @param first_record  First record index in range
 * @param record_count  Number of records in range
 * @return Offset of first set bit (0 if first bit is set),
 *         or record_count if no bits are set
 */
[[nodiscard]] inline size_t find_first_set_bit(
    const std::vector<unsigned char>& bitmap,
    size_t first_record,
    size_t record_count)
{
    constexpr size_t kBitsPerByte = mft_reader_constants::kBitsPerByte;

    for (size_t i = 0; i < record_count; ++i)
    {
        const size_t record_index = first_record + i;
        const size_t byte_index = record_index / kBitsPerByte;
        const size_t bit_index = record_index % kBitsPerByte;

        if (byte_index < bitmap.size() && (bitmap[byte_index] & (1 << bit_index)))
        {
            return i;  // Found first set bit
        }
    }

    return record_count;  // No bits set
}

/**
 * @brief Finds the last set bit in a bitmap range.
 *
 * Scans from the end of the range until finding a set bit.
 *
 * @param bitmap              The bitmap (one bit per record)
 * @param first_record        First record index in range
 * @param record_count        Number of records in range
 * @param skip_records_begin  Records already skipped at start (optimization)
 * @return Offset from end of first set bit (0 if last bit is set),
 *         or remaining count if no bits are set
 */
[[nodiscard]] inline size_t find_last_set_bit(
    const std::vector<unsigned char>& bitmap,
    size_t first_record,
    size_t record_count,
    size_t skip_records_begin)
{
    constexpr size_t kBitsPerByte = mft_reader_constants::kBitsPerByte;

    // Don't scan past the first used record
    const size_t max_skip = record_count - skip_records_begin;

    for (size_t i = 0; i < max_skip; ++i)
    {
        const size_t record_index = first_record + record_count - 1 - i;
        const size_t byte_index = record_index / kBitsPerByte;
        const size_t bit_index = record_index % kBitsPerByte;

        if (byte_index < bitmap.size() && (bitmap[byte_index] & (1 << bit_index)))
        {
            return i;  // Found last set bit
        }
    }

    return max_skip;  // No bits set in remaining range
}

}  // namespace bitmap_utils

#endif // UFFS_BITMAP_UTILS_HPP

