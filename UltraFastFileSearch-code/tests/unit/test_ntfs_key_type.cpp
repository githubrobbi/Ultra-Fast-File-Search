// ============================================================================
// Unit Tests for ntfs_key_type.hpp
// ============================================================================
// Tests the packed key_type_internal bitfield structure
//
// This struct packs multiple fields into minimal space:
// - FRS (File Record Segment): 32 bits - identifies the MFT record
// - name_info: 10 bits - index into hardlink list (max 1023 links)
// - stream_info: 13 bits - index into stream list (max 8191 streams)
// - index: 9 bits - result index (max 511)
//
// Key behaviors to verify:
// - Bitfields don't overlap or corrupt each other
// - Sentinel values (~0) are handled correctly
// - Equality ignores the index field (it's not part of identity)
// ============================================================================

#include "../doctest.h"
#include "../../src/core/ntfs_key_type.hpp"

TEST_SUITE("key_type_internal") {

    TEST_CASE("bitfield sizes match NTFS limits") {
        // These constants define the max values for each field
        CHECK(uffs::key_type_internal::name_info_bits == 10);   // max 1023 hardlinks
        CHECK(uffs::key_type_internal::stream_info_bits == 13); // max 8191 streams
        CHECK(uffs::key_type_internal::index_bits == 9);        // max 511 results

        // Total should fit in 32 bits (one unsigned int)
        constexpr int total_bits = 10 + 13 + 9;
        CHECK(total_bits == 32);
    }

    TEST_CASE("fields are independent - modifying one doesn't corrupt others") {
        uffs::key_type_internal key(0x12345678, 500, 4000);

        // Capture original values
        auto orig_frs = key.frs();
        auto orig_name = key.name_info();
        auto orig_stream = key.stream_info();

        // Modify stream_info
        key.stream_info(7000);

        // FRS and name_info should be unchanged
        CHECK(key.frs() == orig_frs);
        CHECK(key.name_info() == orig_name);
        CHECK(key.stream_info() == 7000);  // Only this changed

        // Modify index
        key.index(255);

        // Other fields still unchanged
        CHECK(key.frs() == orig_frs);
        CHECK(key.name_info() == orig_name);
        CHECK(key.stream_info() == 7000);
        CHECK(key.index() == 255);
    }

    TEST_CASE("sentinel value handling - max bitfield value means 'invalid'") {
        // When all bits are 1, the getter returns ~0 (sentinel for "no value")
        // This is how the code represents "no hardlink" or "no stream"

        // name_info: 10 bits, max = 1023 = 0x3FF
        uffs::key_type_internal key_max_name(0, 1023, 0);
        CHECK(key_max_name.name_info() == static_cast<unsigned short>(~0));

        // But 1022 is a valid value
        uffs::key_type_internal key_valid_name(0, 1022, 0);
        CHECK(key_valid_name.name_info() == 1022);

        // stream_info: 13 bits, max = 8191 = 0x1FFF
        uffs::key_type_internal key_max_stream(0, 0, 8191);
        CHECK(key_max_stream.stream_info() == static_cast<unsigned short>(~0));

        // But 8190 is valid
        uffs::key_type_internal key_valid_stream(0, 0, 8190);
        CHECK(key_valid_stream.stream_info() == 8190);
    }

    TEST_CASE("equality compares FRS, name_info, stream_info - NOT index") {
        uffs::key_type_internal key1(100, 5, 10);
        uffs::key_type_internal key2(100, 5, 10);

        // Same identity
        CHECK(key1 == key2);

        // Different index values should still be equal
        // (index is for sorting results, not identity)
        key1.index(1);
        key2.index(999);
        CHECK(key1 == key2);

        // But different FRS means different key
        uffs::key_type_internal key3(101, 5, 10);
        CHECK_FALSE(key1 == key3);

        // Different name_info means different key
        uffs::key_type_internal key4(100, 6, 10);
        CHECK_FALSE(key1 == key4);

        // Different stream_info means different key
        uffs::key_type_internal key5(100, 5, 11);
        CHECK_FALSE(key1 == key5);
    }

    TEST_CASE("FRS can hold full 32-bit MFT record numbers") {
        // Large NTFS volumes can have millions of files
        uffs::key_type_internal key(0xFFFFFFFF, 0, 0);
        CHECK(key.frs() == 0xFFFFFFFF);

        // Typical large value
        uffs::key_type_internal key2(10000000, 0, 0);
        CHECK(key2.frs() == 10000000);
    }
}

