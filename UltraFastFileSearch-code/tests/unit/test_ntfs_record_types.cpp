// ============================================================================
// Unit Tests for ntfs_record_types.hpp
// ============================================================================
// Tests the packed NTFS record structures used for indexing.
//
// These structures are #pragma pack(push, 1) for memory efficiency.
// Key behaviors to verify:
// - NameInfo: offset and ASCII flag share a single field (bit 0 = ASCII)
// - Sentinel values (~0) indicate "no next entry" in linked lists
// - Structures are properly packed for memory efficiency
// ============================================================================

#include "../doctest.h"
#include "../../src/core/ntfs_record_types.hpp"

TEST_SUITE("NameInfo") {

    TEST_CASE("offset and ASCII flag share storage - bit 0 is ASCII") {
        uffs::NameInfo info{};
        info._offset = 0;

        // Set offset to 1000, ASCII to false
        info.offset(1000);
        info.ascii(false);
        CHECK(info.offset() == 1000);
        CHECK(info.ascii() == false);

        // Now set ASCII to true - offset should be preserved
        info.ascii(true);
        CHECK(info.offset() == 1000);  // Offset unchanged
        CHECK(info.ascii() == true);   // ASCII now true

        // Change offset - ASCII should be preserved
        info.offset(2000);
        CHECK(info.offset() == 2000);  // New offset
        CHECK(info.ascii() == true);   // ASCII still true
    }

    TEST_CASE("offset uses bits 1-31, so max offset is ~2 billion") {
        uffs::NameInfo info{};

        // Large offset value
        unsigned int large_offset = 0x3FFFFFFF;  // ~1 billion
        info.offset(large_offset);
        CHECK(info.offset() == large_offset);
    }

    TEST_CASE("sentinel offset value indicates 'no name'") {
        uffs::NameInfo info{};

        // Setting offset to ~0 (all 1s) is the sentinel
        info.offset(static_cast<unsigned int>(~0U));

        // The getter should return ~0 to indicate "invalid"
        CHECK(info.offset() == static_cast<unsigned int>(~0U));
    }
}

TEST_SUITE("LinkInfo") {

    TEST_CASE("default construction sets sentinel values for linked list") {
        uffs::LinkInfo link;

        // next_entry = ~0 means "end of list"
        CHECK(link.next_entry == static_cast<unsigned int>(~0U));

        // name.offset = ~0 means "no name yet"
        CHECK(link.name.offset() == static_cast<unsigned int>(~0U));
    }

    TEST_CASE("parent field holds MFT record number of parent directory") {
        uffs::LinkInfo link;

        // Root directory is typically record 5
        link.parent = 5;
        CHECK(link.parent == 5);

        // Large record number
        link.parent = 10000000;
        CHECK(link.parent == 10000000);
    }
}

TEST_SUITE("StreamInfo") {

    TEST_CASE("inherits SizeInfo - has length, allocated, bulkiness") {
        uffs::StreamInfo stream;

        // A 1KB file with 4KB allocated (cluster size)
        stream.length = 1024ULL;
        stream.allocated = 4096ULL;
        stream.bulkiness = 4096ULL;

        CHECK(static_cast<unsigned long long>(stream.length) == 1024ULL);
        CHECK(static_cast<unsigned long long>(stream.allocated) == 4096ULL);
        CHECK(static_cast<unsigned long long>(stream.bulkiness) == 4096ULL);
    }

    TEST_CASE("bitfield flags are independent") {
        uffs::StreamInfo stream{};

        // Set sparse flag
        stream.is_sparse = 1;
        CHECK(stream.is_sparse == 1);
        CHECK(stream.is_allocated_size_accounted_for_in_main_stream == 0);

        // Set other flag - sparse should be unchanged
        stream.is_allocated_size_accounted_for_in_main_stream = 1;
        CHECK(stream.is_sparse == 1);
        CHECK(stream.is_allocated_size_accounted_for_in_main_stream == 1);
    }

    TEST_CASE("type_name_id identifies stream type") {
        uffs::StreamInfo stream{};

        // 0 = $I30 index stream
        stream.type_name_id = 0;
        CHECK(stream.type_name_id == 0);

        // Non-zero = other stream types
        stream.type_name_id = 5;
        CHECK(stream.type_name_id == 5);
    }
}

TEST_SUITE("ChildInfo") {

    TEST_CASE("default construction sets all sentinels for empty list") {
        uffs::ChildInfo child;

        CHECK(child.next_entry == static_cast<unsigned int>(~0U));
        CHECK(child.record_number == static_cast<unsigned int>(~0U));
        CHECK(child.name_index == static_cast<unsigned short>(~0U));
    }
}

TEST_SUITE("Record") {

    TEST_CASE("default construction - empty record with no children/names/streams") {
        uffs::Record record;

        CHECK(record.name_count == 0);
        CHECK(record.stream_count == 0);
        CHECK(record.first_child == static_cast<unsigned int>(~0U));  // No children
    }

    TEST_CASE("embedded first_name and first_stream are initialized") {
        uffs::Record record;

        // first_name should have sentinel values
        CHECK(record.first_name.next_entry == static_cast<unsigned int>(~0U));
        CHECK(record.first_name.name.offset() == static_cast<unsigned int>(~0U));

        // first_stream should have sentinel values
        CHECK(record.first_stream.next_entry == static_cast<unsigned int>(~0U));
        CHECK(record.first_stream.name.offset() == static_cast<unsigned int>(~0U));
    }

    TEST_CASE("name_count and stream_count track hardlinks and ADS") {
        uffs::Record record;

        // File with 3 hardlinks
        record.name_count = 3;
        CHECK(record.name_count == 3);

        // File with main stream + 2 alternate data streams
        record.stream_count = 3;
        CHECK(record.stream_count == 3);
    }
}

