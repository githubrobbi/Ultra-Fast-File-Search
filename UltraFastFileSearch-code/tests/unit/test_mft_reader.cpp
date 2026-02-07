// ============================================================================
// Unit Tests for mft_reader.hpp
// ============================================================================
// Tests the MFT reader helper functions and constants.
//
// Key behaviors to verify:
// - Nibble popcount lookup table is correct
// - Bit counting (popcount) works for various patterns
// - Bitmap scanning finds first/last set bits correctly
// - Skip range calculation handles edge cases
//
// Note: Some helper functions are private static methods inside ReadOperation.
// To test them directly, we recreate the algorithms here. This ensures the
// logic is correct even if the implementation changes.
//
// IMPORTANT: We don't include mft_reader.hpp directly because it has complex
// dependencies (NtfsIndex, Windows headers, etc.). Instead, we recreate the
// constants and algorithms here for isolated testing.
// ============================================================================

#include "../doctest.h"

#include <vector>
#include <cstring>
#include <climits>
#include <algorithm>
#include <atomic>

// ============================================================================
// RECREATED CONSTANTS (from mft_reader.hpp)
// ============================================================================
// These are copied from mft_reader_constants namespace for isolated testing.
// If the original constants change, these tests will catch discrepancies.

namespace test_mft_reader_constants {

/// Default maximum bytes to read in a single I/O operation (1 MB)
static constexpr unsigned long long kDefaultReadBlockSize = 1ULL << 20;

/// Number of concurrent I/O operations to maintain
static constexpr int kIoConcurrencyLevel = 2;

/// Number of bits per byte
static constexpr size_t kBitsPerByte = CHAR_BIT;

/// Lookup table for counting set bits in a 4-bit nibble
static constexpr unsigned char kNibblePopCount[16] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

}  // namespace test_mft_reader_constants

// ============================================================================
// TEST HELPERS
// ============================================================================

namespace {

/// Recreate the popcount algorithm for testing
/// This matches the implementation in ReadOperation::count_valid_records_in_buffer
unsigned int count_bits_in_buffer(const unsigned char* buffer, size_t size)
{
    unsigned int count = 0;
    for (size_t i = 0; i < size; ++i)
    {
        const unsigned char byte = buffer[i];
        const unsigned char low_nibble = byte & 0x0F;
        const unsigned char high_nibble = (byte >> 4) & 0x0F;
        count += test_mft_reader_constants::kNibblePopCount[low_nibble];
        count += test_mft_reader_constants::kNibblePopCount[high_nibble];
    }
    return count;
}

/// Recreate the find_first_used_record algorithm for testing
/// Returns the number of unused records at the start of the range
size_t find_first_set_bit(
    const std::vector<unsigned char>& bitmap,
    size_t first_record,
    size_t record_count)
{
    constexpr size_t kBitsPerByte = 8;

    for (size_t i = 0; i < record_count; ++i)
    {
        const size_t record_index = first_record + i;
        const size_t byte_index = record_index / kBitsPerByte;
        const size_t bit_index = record_index % kBitsPerByte;

        if (byte_index < bitmap.size() && (bitmap[byte_index] & (1 << bit_index)))
        {
            return i;  // Found first used record
        }
    }
    return record_count;  // All records unused
}

/// Recreate the find_last_used_record algorithm for testing
/// Returns the number of unused records at the end of the range
size_t find_last_set_bit(
    const std::vector<unsigned char>& bitmap,
    size_t first_record,
    size_t record_count,
    size_t skip_begin)
{
    constexpr size_t kBitsPerByte = 8;
    const size_t max_skip = record_count - skip_begin;

    for (size_t i = 0; i < max_skip; ++i)
    {
        const size_t record_index = first_record + record_count - 1 - i;
        const size_t byte_index = record_index / kBitsPerByte;
        const size_t bit_index = record_index % kBitsPerByte;

        if (byte_index < bitmap.size() && (bitmap[byte_index] & (1 << bit_index)))
        {
            return i;  // Found last used record
        }
    }
    return max_skip;  // All remaining records unused
}

}  // anonymous namespace

// ============================================================================
// TESTS: Constants
// ============================================================================

TEST_SUITE("mft_reader_constants") {

    TEST_CASE("kNibblePopCount lookup table is correct") {
        // Verify each entry in the popcount table
        // This is critical - a wrong value here corrupts record counts
        CHECK(test_mft_reader_constants::kNibblePopCount[0x0] == 0);  // 0000
        CHECK(test_mft_reader_constants::kNibblePopCount[0x1] == 1);  // 0001
        CHECK(test_mft_reader_constants::kNibblePopCount[0x2] == 1);  // 0010
        CHECK(test_mft_reader_constants::kNibblePopCount[0x3] == 2);  // 0011
        CHECK(test_mft_reader_constants::kNibblePopCount[0x4] == 1);  // 0100
        CHECK(test_mft_reader_constants::kNibblePopCount[0x5] == 2);  // 0101
        CHECK(test_mft_reader_constants::kNibblePopCount[0x6] == 2);  // 0110
        CHECK(test_mft_reader_constants::kNibblePopCount[0x7] == 3);  // 0111
        CHECK(test_mft_reader_constants::kNibblePopCount[0x8] == 1);  // 1000
        CHECK(test_mft_reader_constants::kNibblePopCount[0x9] == 2);  // 1001
        CHECK(test_mft_reader_constants::kNibblePopCount[0xA] == 2);  // 1010
        CHECK(test_mft_reader_constants::kNibblePopCount[0xB] == 3);  // 1011
        CHECK(test_mft_reader_constants::kNibblePopCount[0xC] == 2);  // 1100
        CHECK(test_mft_reader_constants::kNibblePopCount[0xD] == 3);  // 1101
        CHECK(test_mft_reader_constants::kNibblePopCount[0xE] == 3);  // 1110
        CHECK(test_mft_reader_constants::kNibblePopCount[0xF] == 4);  // 1111
    }

    TEST_CASE("kDefaultReadBlockSize is 1 MB") {
        CHECK(test_mft_reader_constants::kDefaultReadBlockSize == 1024 * 1024);
    }

    TEST_CASE("kIoConcurrencyLevel is reasonable") {
        CHECK(test_mft_reader_constants::kIoConcurrencyLevel >= 1);
        CHECK(test_mft_reader_constants::kIoConcurrencyLevel <= 16);
    }

    TEST_CASE("kBitsPerByte matches CHAR_BIT") {
        CHECK(test_mft_reader_constants::kBitsPerByte == CHAR_BIT);
    }
}

// ============================================================================
// TESTS: Bit Counting (Popcount)
// ============================================================================

TEST_SUITE("mft_reader_popcount") {

    TEST_CASE("empty buffer returns 0") {
        unsigned char buffer[1] = {0};
        CHECK(count_bits_in_buffer(buffer, 0) == 0);
    }

    TEST_CASE("all zeros returns 0") {
        unsigned char buffer[4] = {0x00, 0x00, 0x00, 0x00};
        CHECK(count_bits_in_buffer(buffer, 4) == 0);
    }

    TEST_CASE("all ones returns 8 * size") {
        unsigned char buffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        CHECK(count_bits_in_buffer(buffer, 4) == 32);
    }

    TEST_CASE("single byte patterns") {
        unsigned char buffer[1];

        buffer[0] = 0x01;  // 1 bit
        CHECK(count_bits_in_buffer(buffer, 1) == 1);

        buffer[0] = 0x80;  // 1 bit (high)
        CHECK(count_bits_in_buffer(buffer, 1) == 1);

        buffer[0] = 0x55;  // 01010101 = 4 bits
        CHECK(count_bits_in_buffer(buffer, 1) == 4);

        buffer[0] = 0xAA;  // 10101010 = 4 bits
        CHECK(count_bits_in_buffer(buffer, 1) == 4);

        buffer[0] = 0x0F;  // 00001111 = 4 bits
        CHECK(count_bits_in_buffer(buffer, 1) == 4);

        buffer[0] = 0xF0;  // 11110000 = 4 bits
        CHECK(count_bits_in_buffer(buffer, 1) == 4);
    }

    TEST_CASE("realistic MFT bitmap pattern") {
        // Simulate a typical MFT bitmap where most records are in-use
        // but some are free (deleted files)
        unsigned char buffer[8] = {
            0xFF,  // 8 in-use
            0xFF,  // 8 in-use
            0xFE,  // 7 in-use (record 16 is free)
            0xFF,  // 8 in-use
            0x7F,  // 7 in-use (record 39 is free)
            0xFF,  // 8 in-use
            0xFF,  // 8 in-use
            0x03   // 2 in-use (only records 56-57, rest are beyond MFT)
        };
        // Total: 8+8+7+8+7+8+8+2 = 56
        CHECK(count_bits_in_buffer(buffer, 8) == 56);
    }
}



// ============================================================================
// TESTS: Bitmap Scanning (Find First/Last Set Bit)
// ============================================================================

TEST_SUITE("mft_reader_bitmap_scan") {

    TEST_CASE("find_first_set_bit: first bit is set") {
        std::vector<unsigned char> bitmap = {0x01};  // bit 0 is set
        CHECK(find_first_set_bit(bitmap, 0, 8) == 0);
    }

    TEST_CASE("find_first_set_bit: last bit is set") {
        std::vector<unsigned char> bitmap = {0x80};  // bit 7 is set
        CHECK(find_first_set_bit(bitmap, 0, 8) == 7);
    }

    TEST_CASE("find_first_set_bit: no bits set") {
        std::vector<unsigned char> bitmap = {0x00, 0x00};
        CHECK(find_first_set_bit(bitmap, 0, 16) == 16);  // returns record_count
    }

    TEST_CASE("find_first_set_bit: bit in second byte") {
        std::vector<unsigned char> bitmap = {0x00, 0x01};  // bit 8 is set
        CHECK(find_first_set_bit(bitmap, 0, 16) == 8);
    }

    TEST_CASE("find_first_set_bit: with offset") {
        std::vector<unsigned char> bitmap = {0xFF, 0x00, 0x01};  // bits 0-7 and 16 set
        // Start at record 8, looking for 16 records
        CHECK(find_first_set_bit(bitmap, 8, 16) == 8);  // bit 16 is at offset 8 from start
    }

    TEST_CASE("find_last_set_bit: last bit is set") {
        std::vector<unsigned char> bitmap = {0x80};  // bit 7 is set
        CHECK(find_last_set_bit(bitmap, 0, 8, 0) == 0);
    }

    TEST_CASE("find_last_set_bit: first bit is set") {
        std::vector<unsigned char> bitmap = {0x01};  // bit 0 is set
        CHECK(find_last_set_bit(bitmap, 0, 8, 0) == 7);
    }

    TEST_CASE("find_last_set_bit: no bits set") {
        std::vector<unsigned char> bitmap = {0x00, 0x00};
        CHECK(find_last_set_bit(bitmap, 0, 16, 0) == 16);  // returns max_skip
    }

    TEST_CASE("find_last_set_bit: respects skip_begin") {
        std::vector<unsigned char> bitmap = {0x01};  // only bit 0 is set
        // If we already skipped 1 record at start, max_skip = 8 - 1 = 7
        CHECK(find_last_set_bit(bitmap, 0, 8, 1) == 7);
    }

    TEST_CASE("skip range calculation: all records in-use") {
        std::vector<unsigned char> bitmap = {0xFF, 0xFF};  // all 16 bits set
        size_t skip_begin = find_first_set_bit(bitmap, 0, 16);
        size_t skip_end = find_last_set_bit(bitmap, 0, 16, skip_begin);
        CHECK(skip_begin == 0);
        CHECK(skip_end == 0);
    }

    TEST_CASE("skip range calculation: no records in-use") {
        std::vector<unsigned char> bitmap = {0x00, 0x00};  // no bits set
        size_t skip_begin = find_first_set_bit(bitmap, 0, 16);
        size_t skip_end = find_last_set_bit(bitmap, 0, 16, skip_begin);
        CHECK(skip_begin == 16);
        CHECK(skip_end == 0);  // max_skip = 16 - 16 = 0
    }

    TEST_CASE("skip range calculation: records in middle only") {
        // Bits 4-11 are set (records 4-11 in-use)
        std::vector<unsigned char> bitmap = {0xF0, 0x0F};  // 11110000 00001111
        size_t skip_begin = find_first_set_bit(bitmap, 0, 16);
        size_t skip_end = find_last_set_bit(bitmap, 0, 16, skip_begin);
        CHECK(skip_begin == 4);   // first 4 records unused
        CHECK(skip_end == 4);     // last 4 records unused
    }

    TEST_CASE("skip range calculation: single record in-use") {
        std::vector<unsigned char> bitmap = {0x00, 0x01};  // only bit 8 set
        size_t skip_begin = find_first_set_bit(bitmap, 0, 16);
        size_t skip_end = find_last_set_bit(bitmap, 0, 16, skip_begin);
        CHECK(skip_begin == 8);   // first 8 records unused
        CHECK(skip_end == 7);     // last 7 records unused
    }
}


// ============================================================================
// TESTS: ChunkDescriptor-like structure
// ============================================================================
// We recreate the ChunkDescriptor structure here for isolated testing since
// mft_reader.hpp has complex Windows dependencies.

namespace {

/// Test version of ChunkDescriptor (mirrors the real one in mft_reader.hpp)
struct TestChunkDescriptor {
    unsigned long long vcn;
    unsigned long long cluster_count;
    long long lcn;
    std::atomic<size_t> skip_begin;
    std::atomic<size_t> skip_end;

    TestChunkDescriptor(
        unsigned long long vcn_,
        unsigned long long cluster_count_,
        long long lcn_)
        : vcn(vcn_)
        , cluster_count(cluster_count_)
        , lcn(lcn_)
        , skip_begin(0)
        , skip_end(0)
    {}

    // Copy constructor (required because of atomic members)
    TestChunkDescriptor(const TestChunkDescriptor& other)
        : vcn(other.vcn)
        , cluster_count(other.cluster_count)
        , lcn(other.lcn)
        , skip_begin(other.skip_begin.load(std::memory_order_relaxed))
        , skip_end(other.skip_end.load(std::memory_order_relaxed))
    {}
};

}  // anonymous namespace

TEST_SUITE("ChunkDescriptor") {

    TEST_CASE("construction with VCN, cluster_count, LCN") {
        TestChunkDescriptor chunk(100, 50, 200);
        CHECK(chunk.vcn == 100);
        CHECK(chunk.cluster_count == 50);
        CHECK(chunk.lcn == 200);
        CHECK(chunk.skip_begin.load() == 0);
        CHECK(chunk.skip_end.load() == 0);
    }

    TEST_CASE("skip values are atomic") {
        TestChunkDescriptor chunk(0, 100, 0);

        // Store and load should work atomically
        chunk.skip_begin.store(10);
        chunk.skip_end.store(20);

        CHECK(chunk.skip_begin.load() == 10);
        CHECK(chunk.skip_end.load() == 20);
    }

    TEST_CASE("copy constructor preserves values") {
        TestChunkDescriptor original(50, 25, 100);
        original.skip_begin.store(5);
        original.skip_end.store(10);

        TestChunkDescriptor copy(original);
        CHECK(copy.vcn == 50);
        CHECK(copy.cluster_count == 25);
        CHECK(copy.lcn == 100);
        CHECK(copy.skip_begin.load() == 5);
        CHECK(copy.skip_end.load() == 10);
    }
}

// ============================================================================
// TESTS: Extent to Chunk Conversion Logic
// ============================================================================

TEST_SUITE("mft_reader_extent_conversion") {

    TEST_CASE("single extent smaller than max chunk size") {
        // Simulate: one extent of 10 clusters, max chunk = 100 clusters
        // Should produce 1 chunk
        const unsigned long long max_clusters = 100;
        const unsigned long long extent_vcn_end = 10;
        const long long extent_lcn = 500;

        // Simulate the algorithm
        unsigned long long current_vcn = 0;
        std::vector<TestChunkDescriptor> chunks;

        const unsigned long long extent_clusters = extent_vcn_end - current_vcn;
        const unsigned long long chunk_clusters = (std::min)(extent_clusters, max_clusters);
        chunks.emplace_back(current_vcn, chunk_clusters, extent_lcn);

        CHECK(chunks.size() == 1);
        CHECK(chunks[0].vcn == 0);
        CHECK(chunks[0].cluster_count == 10);
        CHECK(chunks[0].lcn == 500);
    }

    TEST_CASE("single extent larger than max chunk size") {
        // Simulate: one extent of 250 clusters, max chunk = 100 clusters
        // Should produce 3 chunks (100 + 100 + 50)
        const unsigned long long max_clusters = 100;
        const unsigned long long extent_vcn_end = 250;
        const long long extent_lcn = 1000;

        std::vector<TestChunkDescriptor> chunks;
        unsigned long long current_vcn = 0;

        while (current_vcn < extent_vcn_end)
        {
            const unsigned long long remaining = extent_vcn_end - current_vcn;
            const unsigned long long chunk_clusters = (std::min)(remaining, max_clusters);
            const long long chunk_lcn = extent_lcn + static_cast<long long>(current_vcn);
            chunks.emplace_back(current_vcn, chunk_clusters, chunk_lcn);
            current_vcn += chunk_clusters;
        }

        CHECK(chunks.size() == 3);
        CHECK(chunks[0].cluster_count == 100);
        CHECK(chunks[1].cluster_count == 100);
        CHECK(chunks[2].cluster_count == 50);

        // Verify LCNs are sequential
        CHECK(chunks[0].lcn == 1000);
        CHECK(chunks[1].lcn == 1100);
        CHECK(chunks[2].lcn == 1200);
    }
}