/**
 * @file mft_reader_constants.hpp
 * @brief Constants for MFT (Master File Table) reading operations.
 *
 * This header contains compile-time constants used by the MFT reader:
 * - I/O sizing parameters
 * - Bitmap processing lookup tables
 * - Platform-independent bit manipulation constants
 *
 * @note Separated from mft_reader.hpp for single-responsibility and faster compilation.
 */

#ifndef UFFS_MFT_READER_CONSTANTS_HPP
#define UFFS_MFT_READER_CONSTANTS_HPP

#include <climits>
#include <cstddef>

namespace mft_reader_constants {

/// Default maximum bytes to read in a single I/O operation.
/// 1 MB is a good balance between:
/// - Large enough to amortize I/O overhead
/// - Small enough to maintain concurrency
/// - Aligned with typical disk cache sizes
static constexpr unsigned long long kDefaultReadBlockSize = 1ULL << 20;  // 1 MB

/// Number of concurrent I/O operations to maintain.
/// Two operations allows one to be in-flight while another completes.
/// Higher values may improve throughput on SSDs but increase memory usage.
static constexpr int kIoConcurrencyLevel = 2;

/// Number of bits per byte (platform-independent).
/// Used for bitmap calculations.
static constexpr size_t kBitsPerByte = CHAR_BIT;

/// Lookup table for counting set bits in a 4-bit nibble.
/// Used for fast population count when processing the MFT bitmap.
/// Index is the nibble value (0-15), result is the number of 1-bits.
///
/// Example usage:
/// @code
/// unsigned char byte = 0xAB;  // 10101011
/// int count = kNibblePopCount[byte & 0x0F] +      // low nibble: 1011 = 3
///             kNibblePopCount[(byte >> 4) & 0x0F]; // high nibble: 1010 = 2
/// // count = 5
/// @endcode
static constexpr unsigned char kNibblePopCount[16] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

}  // namespace mft_reader_constants

#endif // UFFS_MFT_READER_CONSTANTS_HPP

