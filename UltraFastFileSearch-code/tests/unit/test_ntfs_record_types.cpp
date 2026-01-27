// ============================================================================
// Unit Tests for ntfs_record_types.hpp
// ============================================================================
// Tests NameInfo, LinkInfo, StreamInfo, ChildInfo, and Record structures
// ============================================================================

#include "../doctest.h"
#include "../../src/core/ntfs_record_types.hpp"

TEST_SUITE("small_t") {
    TEST_CASE("type is unsigned int") {
        CHECK(sizeof(uffs::small_t<size_t>::type) == sizeof(unsigned int));
    }
}

TEST_SUITE("NameInfo") {
    TEST_CASE("ascii flag storage") {
        uffs::NameInfo info{};
        
        SUBCASE("default is not ASCII") {
            info._offset = 0;
            CHECK_FALSE(info.ascii());
        }

        SUBCASE("set ASCII flag") {
            info._offset = 0;
            info.ascii(true);
            CHECK(info.ascii());
        }

        SUBCASE("clear ASCII flag") {
            info._offset = 1;  // ASCII bit set
            info.ascii(false);
            CHECK_FALSE(info.ascii());
        }
    }

    TEST_CASE("offset storage") {
        uffs::NameInfo info{};
        
        SUBCASE("zero offset") {
            info._offset = 0;
            info.offset(0);
            CHECK(info.offset() == 0);
        }

        SUBCASE("typical offset") {
            info._offset = 0;
            info.offset(1000);
            CHECK(info.offset() == 1000);
        }

        SUBCASE("offset preserves ASCII flag") {
            info._offset = 0;
            info.ascii(true);
            info.offset(500);
            CHECK(info.offset() == 500);
            CHECK(info.ascii());  // ASCII flag should be preserved
        }
    }

    TEST_CASE("length field") {
        uffs::NameInfo info{};
        info.length = 255;
        CHECK(info.length == 255);
    }
}

TEST_SUITE("LinkInfo") {
    TEST_CASE("default construction") {
        uffs::LinkInfo link;
        // next_entry should be negative_one (sentinel)
        CHECK(link.next_entry == static_cast<uffs::LinkInfo::next_entry_type>(~0U));
        // name.offset should be negative_one
        CHECK(link.name.offset() == static_cast<uffs::small_t<size_t>::type>(~0U));
    }

    TEST_CASE("parent field") {
        uffs::LinkInfo link;
        link.parent = 12345;
        CHECK(link.parent == 12345);
    }
}

TEST_SUITE("StreamInfo") {
    TEST_CASE("default construction") {
        uffs::StreamInfo stream;
        CHECK(stream.next_entry == 0);
        CHECK(stream.is_sparse == 0);
        CHECK(stream.is_allocated_size_accounted_for_in_main_stream == 0);
        CHECK(stream.type_name_id == 0);
    }

    TEST_CASE("inherits from SizeInfo") {
        uffs::StreamInfo stream;
        stream.length = 1024ULL;
        stream.allocated = 4096ULL;
        CHECK(static_cast<unsigned long long>(stream.length) == 1024ULL);
        CHECK(static_cast<unsigned long long>(stream.allocated) == 4096ULL);
    }

    TEST_CASE("sparse flag") {
        uffs::StreamInfo stream;
        stream.is_sparse = 1;
        CHECK(stream.is_sparse == 1);
    }
}

TEST_SUITE("ChildInfo") {
    TEST_CASE("default construction") {
        uffs::ChildInfo child;
        CHECK(child.next_entry == static_cast<uffs::ChildInfo::next_entry_type>(~0U));
        CHECK(child.record_number == static_cast<uffs::small_t<size_t>::type>(~0U));
        CHECK(child.name_index == static_cast<unsigned short>(~0U));
    }
}

TEST_SUITE("Record") {
    TEST_CASE("default construction") {
        uffs::Record record;
        CHECK(record.name_count == 0);
        CHECK(record.stream_count == 0);
        CHECK(record.first_child == static_cast<uffs::ChildInfo::next_entry_type>(~0U));
    }

    TEST_CASE("name and stream counts") {
        uffs::Record record;
        record.name_count = 5;
        record.stream_count = 3;
        CHECK(record.name_count == 5);
        CHECK(record.stream_count == 3);
    }
}

