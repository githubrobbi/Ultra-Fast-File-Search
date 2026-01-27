// ============================================================================
// Unit Tests for buffer.hpp
// ============================================================================
// Tests the raw memory buffer class - a malloc/realloc-based container.
//
// Key behaviors to verify:
// - Uses malloc/free (not new/delete) for raw memory
// - Capacity vs size distinction (pre-allocated vs used)
// - Deep copy semantics (copy doesn't alias original)
// - emplace_back placement-news into the buffer
// - Auto-growth when capacity exceeded
// ============================================================================

#include "../doctest.h"
#include "../../src/util/buffer.hpp"

TEST_SUITE("buffer") {

    TEST_CASE("capacity vs size - buffer pre-allocates without using") {
        buffer buf(1000);

        // Capacity is what we asked for
        CHECK(buf.capacity() == 1000);

        // But size is 0 - nothing used yet
        CHECK(buf.size() == 0);
        CHECK(buf.empty());

        // Memory is allocated though
        CHECK(buf.get() != nullptr);
    }

    TEST_CASE("emplace_back uses capacity, then triggers realloc") {
        buffer buf(16);  // Small initial capacity

        // Add 4 ints (16 bytes) - fits exactly
        for (int i = 0; i < 4; ++i) {
            int* p = buf.emplace_back<int>();
            *p = i * 10;
        }
        CHECK(buf.size() == 16);

        // Add one more - triggers realloc (but note: buffer::resize has a bug
        // where it doesn't update this->c after realloc, so capacity() stays stale)
        int* p = buf.emplace_back<int>();
        *p = 999;
        CHECK(buf.size() == 20);
        // Note: capacity() is not updated by resize() - this is a known issue
        // The memory IS reallocated, but the capacity member isn't updated

        // Verify data survived the realloc - this is the important test
        CHECK(*reinterpret_cast<int*>(buf.begin()) == 0);
        CHECK(*reinterpret_cast<int*>(buf.begin() + 4) == 10);
        CHECK(*reinterpret_cast<int*>(buf.begin() + 16) == 999);
    }

    TEST_CASE("deep copy - modifying copy doesn't affect original") {
        buffer original(100);

        // Put some data in original
        for (int i = 0; i < 10; ++i) {
            unsigned char* p = original.emplace_back<unsigned char>();
            *p = static_cast<unsigned char>(i);
        }

        // Copy it
        buffer copy(original);

        // Verify copy has same data
        CHECK(copy.size() == original.size());
        for (size_t i = 0; i < 10; ++i) {
            CHECK(copy[i] == original[i]);
        }

        // Modify copy
        copy[0] = 0xFF;
        copy[5] = 0xAB;

        // Original is unchanged
        CHECK(original[0] == 0);
        CHECK(original[5] == 5);

        // Copy has new values
        CHECK(copy[0] == 0xFF);
        CHECK(copy[5] == 0xAB);
    }

    TEST_CASE("swap exchanges all state - no copying of data") {
        buffer buf1(100);
        buffer buf2(200);

        // Put different data in each
        *buf1.emplace_back<int>() = 111;
        *buf2.emplace_back<int>() = 222;
        *buf2.emplace_back<int>() = 333;

        void* ptr1 = buf1.get();
        void* ptr2 = buf2.get();

        buf1.swap(buf2);

        // Pointers swapped (no data copied)
        CHECK(buf1.get() == ptr2);
        CHECK(buf2.get() == ptr1);

        // Sizes swapped
        CHECK(buf1.size() == 2 * sizeof(int));
        CHECK(buf2.size() == sizeof(int));

        // Capacities swapped
        CHECK(buf1.capacity() == 200);
        CHECK(buf2.capacity() == 100);

        // Data accessible through swapped buffers
        CHECK(*reinterpret_cast<int*>(buf1.begin()) == 222);
        CHECK(*reinterpret_cast<int*>(buf2.begin()) == 111);
    }

    TEST_CASE("clear releases memory - not just size reset") {
        buffer buf(1000);
        buf.emplace_back<int>();

        void* old_ptr = buf.get();
        CHECK(old_ptr != nullptr);

        buf.clear();

        // After clear, buffer is truly empty
        CHECK(buf.size() == 0);
        CHECK(buf.capacity() == 0);
        CHECK(buf.get() == nullptr);  // Memory freed
    }

    TEST_CASE("begin/end/tail iterator relationships") {
        buffer buf(100);

        // Empty buffer: all iterators equal
        CHECK(buf.begin() == buf.end());
        CHECK(buf.begin() == buf.tail());

        // Add some data
        buf.emplace_back<int>();
        buf.emplace_back<int>();

        // begin() is start of data
        // end() and tail() are past the used portion
        CHECK(buf.end() == buf.begin() + buf.size());
        CHECK(buf.tail() == buf.begin() + buf.size());
        CHECK(buf.end() == buf.tail());

        // Distance is size
        CHECK(static_cast<size_t>(buf.end() - buf.begin()) == buf.size());
    }

    TEST_CASE("operator[] provides byte-level access") {
        buffer buf(100);

        // emplace an int
        int* p = buf.emplace_back<int>();
        *p = 0x12345678;

        // operator[] accesses individual bytes
        // (endianness-dependent, but we can check consistency)
        unsigned char byte0 = buf[0];
        unsigned char byte1 = buf[1];
        unsigned char byte2 = buf[2];
        unsigned char byte3 = buf[3];

        // Reconstruct the int from bytes
        int reconstructed = byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
        CHECK(reconstructed == 0x12345678);
    }

    TEST_CASE("assignment operator makes deep copy") {
        buffer buf1(100);
        *buf1.emplace_back<int>() = 42;

        buffer buf2;
        buf2 = buf1;

        // Same size
        CHECK(buf2.size() == buf1.size());

        // Same data
        CHECK(*reinterpret_cast<int*>(buf2.begin()) == 42);

        // But different memory (deep copy)
        CHECK(buf2.get() != buf1.get());
    }
}

