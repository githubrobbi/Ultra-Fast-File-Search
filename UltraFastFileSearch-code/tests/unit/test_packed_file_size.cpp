// ============================================================================
// Unit Tests for packed_file_size.hpp
// ============================================================================
// Tests the 48-bit file_size_type (6 bytes: 4-byte low + 2-byte high)
// Key behaviors to verify:
// - Correct packing into 6 bytes (memory efficiency)
// - Correct handling of 32-bit boundary (low/high split)
// - Arithmetic with carry across the boundary
// ============================================================================

#include "../doctest.h"
#include "../../src/core/packed_file_size.hpp"

TEST_SUITE("file_size_type") {

    TEST_CASE("size is exactly 6 bytes - the whole point of this class") {
        // This is THE critical property - if this fails, the class is useless
        // A regular uint64_t is 8 bytes; we save 25% memory per file record
        CHECK(sizeof(uffs::file_size_type) == 6);
    }

    TEST_CASE("operator! correctly detects zero vs non-zero") {
        uffs::file_size_type zero;
        CHECK(!zero == true);

        uffs::file_size_type one(1ULL);
        CHECK(!one == false);

        // Edge case: value only in high bits should still be non-zero
        uffs::file_size_type high_only(0x100000000ULL);  // 4GB, only in high bits
        CHECK(!high_only == false);
    }

    TEST_CASE("32-bit boundary: values crossing low/high split") {
        // The class stores: low (32 bits) + high (16 bits)
        // Values at the boundary are where bugs hide

        // Just below boundary
        uffs::file_size_type below(0xFFFFFFFFULL);
        // Just above boundary
        uffs::file_size_type above(0x100000000ULL);
        // Spanning boundary
        uffs::file_size_type spanning(0x1FFFFFFFFULL);

        // Verify they're different (not truncated)
        CHECK(static_cast<unsigned long long>(below) != static_cast<unsigned long long>(above));
        CHECK(static_cast<unsigned long long>(above) < static_cast<unsigned long long>(spanning));

        // Verify exact values
        CHECK(static_cast<unsigned long long>(above) == 0x100000000ULL);
        CHECK(static_cast<unsigned long long>(spanning) == 0x1FFFFFFFFULL);
    }

    TEST_CASE("operator+= handles carry from low to high") {
        // Start just below 32-bit boundary
        uffs::file_size_type size(0xFFFFFFF0ULL);

        // Add enough to cross the boundary
        size += uffs::file_size_type(0x20ULL);

        // Result should be in the "high" portion
        unsigned long long result = static_cast<unsigned long long>(size);
        CHECK(result == 0x100000010ULL);
        CHECK(result > 0xFFFFFFFFULL);  // Proves high bits are set
    }

    TEST_CASE("operator-= handles borrow from high to low") {
        // Start just above 32-bit boundary
        uffs::file_size_type size(0x100000010ULL);

        // Subtract enough to cross back
        size -= uffs::file_size_type(0x20ULL);

        // Result should be back in "low" portion only
        unsigned long long result = static_cast<unsigned long long>(size);
        CHECK(result == 0xFFFFFFF0ULL);
        CHECK(result < 0x100000000ULL);  // Proves high bits are clear
    }

    TEST_CASE("maximum 48-bit value (256 TB - 1 byte)") {
        // This is the max file size the class can represent
        unsigned long long max48 = 0xFFFFFFFFFFFFULL;  // 281,474,976,710,655 bytes
        uffs::file_size_type size(max48);

        // Verify no truncation
        CHECK(static_cast<unsigned long long>(size) == max48);
    }

    TEST_CASE("values beyond 48 bits are truncated") {
        // This documents the limitation - values > 48 bits lose data
        unsigned long long too_big = 0x1000000000000ULL;  // 49 bits
        uffs::file_size_type size(too_big);

        // The high 16 bits can only hold 0xFFFF, so bit 49+ is lost
        CHECK(static_cast<unsigned long long>(size) != too_big);
        CHECK(static_cast<unsigned long long>(size) == 0ULL);  // Wraps to 0
    }
}

TEST_SUITE("SizeInfo") {
    TEST_CASE("contains three file_size_type fields") {
        // Verify the struct layout is as expected
        uffs::SizeInfo info{};

        // Set different values to each field
        info.length = 1000ULL;
        info.allocated = 4096ULL;
        info.bulkiness = 8192ULL;

        // Verify they're independent (no aliasing)
        CHECK(static_cast<unsigned long long>(info.length) == 1000ULL);
        CHECK(static_cast<unsigned long long>(info.allocated) == 4096ULL);
        CHECK(static_cast<unsigned long long>(info.bulkiness) == 8192ULL);
    }
}

