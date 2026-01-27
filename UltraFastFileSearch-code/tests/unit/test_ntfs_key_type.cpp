// ============================================================================
// Unit Tests for ntfs_key_type.hpp
// ============================================================================
// Tests the packed key_type_internal bitfield structure
// ============================================================================

#include "../doctest.h"
#include "../../src/core/ntfs_key_type.hpp"

TEST_SUITE("key_type_internal") {
    TEST_CASE("construction and FRS retrieval") {
        uffs::key_type_internal key(12345, 0, 0);
        CHECK(key.frs() == 12345);
    }

    TEST_CASE("name_info storage and retrieval") {
        SUBCASE("zero value") {
            uffs::key_type_internal key(0, 0, 0);
            CHECK(key.name_info() == 0);
        }

        SUBCASE("maximum valid value (1022)") {
            // 10 bits = max 1023, but 1023 is sentinel for "invalid"
            uffs::key_type_internal key(0, 1022, 0);
            CHECK(key.name_info() == 1022);
        }

        SUBCASE("sentinel value returns ~0") {
            // 1023 (all 1s in 10 bits) should return ~0
            uffs::key_type_internal key(0, 1023, 0);
            CHECK(key.name_info() == static_cast<uffs::key_type_internal::name_info_type>(~0));
        }
    }

    TEST_CASE("stream_info storage and retrieval") {
        SUBCASE("zero value") {
            uffs::key_type_internal key(0, 0, 0);
            CHECK(key.stream_info() == 0);
        }

        SUBCASE("typical value") {
            uffs::key_type_internal key(0, 0, 100);
            CHECK(key.stream_info() == 100);
        }

        SUBCASE("maximum valid value (8190)") {
            // 13 bits = max 8191, but 8191 is sentinel
            uffs::key_type_internal key(0, 0, 8190);
            CHECK(key.stream_info() == 8190);
        }
    }

    TEST_CASE("stream_info setter") {
        uffs::key_type_internal key(100, 5, 10);
        key.stream_info(200);
        CHECK(key.stream_info() == 200);
        // Other fields should be unchanged
        CHECK(key.frs() == 100);
        CHECK(key.name_info() == 5);
    }

    TEST_CASE("index getter and setter") {
        uffs::key_type_internal key(0, 0, 0);
        key.index(42);
        CHECK(key.index() == 42);
    }

    TEST_CASE("equality operator") {
        uffs::key_type_internal key1(100, 5, 10);
        uffs::key_type_internal key2(100, 5, 10);
        uffs::key_type_internal key3(100, 5, 11);  // Different stream_info

        CHECK(key1 == key2);
        CHECK_FALSE(key1 == key3);
    }

    TEST_CASE("equality ignores index field") {
        uffs::key_type_internal key1(100, 5, 10);
        uffs::key_type_internal key2(100, 5, 10);
        key1.index(1);
        key2.index(2);
        // Equality should still hold - index is not part of key identity
        CHECK(key1 == key2);
    }

    TEST_CASE("bitfield constants are correct") {
        CHECK(uffs::key_type_internal::name_info_bits == 10);
        CHECK(uffs::key_type_internal::stream_info_bits == 13);
        // index_bits = 32 - 10 - 13 = 9
        CHECK(uffs::key_type_internal::index_bits == 9);
    }
}

