// ============================================================================
// Unit Tests for packed_file_size.hpp
// ============================================================================

#include "../doctest.h"
#include "../../src/core/packed_file_size.hpp"

TEST_SUITE("file_size_type") {
    TEST_CASE("default construction initializes to zero") {
        uffs::file_size_type size;
        CHECK(static_cast<unsigned long long>(size) == 0ULL);
        CHECK(!size);  // operator! should return true for zero
    }

    TEST_CASE("construction from value") {
        SUBCASE("small value") {
            uffs::file_size_type size(12345ULL);
            CHECK(static_cast<unsigned long long>(size) == 12345ULL);
        }

        SUBCASE("value fitting in 32 bits") {
            uffs::file_size_type size(0xFFFFFFFFULL);
            CHECK(static_cast<unsigned long long>(size) == 0xFFFFFFFFULL);
        }

        SUBCASE("value requiring 48 bits") {
            // 100 TB = 109,951,162,777,600 bytes
            unsigned long long tb100 = 100ULL * 1024 * 1024 * 1024 * 1024;
            uffs::file_size_type size(tb100);
            CHECK(static_cast<unsigned long long>(size) == tb100);
        }

        SUBCASE("maximum 48-bit value") {
            // Maximum value for 48-bit: 0xFFFFFFFFFFFF (281 TB)
            unsigned long long max48 = 0xFFFFFFFFFFFFULL;
            uffs::file_size_type size(max48);
            CHECK(static_cast<unsigned long long>(size) == max48);
        }
    }

    TEST_CASE("operator+=") {
        uffs::file_size_type size(1000ULL);
        size += uffs::file_size_type(500ULL);
        CHECK(static_cast<unsigned long long>(size) == 1500ULL);
    }

    TEST_CASE("operator-=") {
        uffs::file_size_type size(1000ULL);
        size -= uffs::file_size_type(300ULL);
        CHECK(static_cast<unsigned long long>(size) == 700ULL);
    }

    TEST_CASE("operator! returns false for non-zero") {
        uffs::file_size_type size(1ULL);
        CHECK_FALSE(!size);
    }

    TEST_CASE("size is exactly 6 bytes") {
        CHECK(sizeof(uffs::file_size_type) == 6);
    }
}

TEST_SUITE("SizeInfo") {
    TEST_CASE("default construction") {
        uffs::SizeInfo info{};
        CHECK(static_cast<unsigned long long>(info.length) == 0ULL);
        CHECK(static_cast<unsigned long long>(info.allocated) == 0ULL);
        CHECK(static_cast<unsigned long long>(info.bulkiness) == 0ULL);
    }
}

