// ============================================================================
// Unit Tests for buffer.hpp
// ============================================================================
// Tests the raw memory buffer class
// ============================================================================

#include "../doctest.h"
#include "../../src/util/buffer.hpp"

TEST_SUITE("buffer") {
    TEST_CASE("default construction") {
        buffer buf;
        CHECK(buf.size() == 0);
        CHECK(buf.capacity() == 0);
        CHECK(buf.empty());
        CHECK(buf.get() == nullptr);
    }

    TEST_CASE("construction with capacity") {
        buffer buf(100);
        CHECK(buf.size() == 0);
        CHECK(buf.capacity() == 100);
        CHECK(buf.empty());
        CHECK(buf.get() != nullptr);
    }

    TEST_CASE("iterators on empty buffer") {
        buffer buf;
        CHECK(buf.begin() == buf.end());
    }

    TEST_CASE("emplace_back adds elements") {
        buffer buf(100);
        
        int* p1 = buf.emplace_back<int>();
        *p1 = 42;
        CHECK(buf.size() == sizeof(int));
        CHECK_FALSE(buf.empty());
        
        int* p2 = buf.emplace_back<int>();
        *p2 = 100;
        CHECK(buf.size() == 2 * sizeof(int));
    }

    TEST_CASE("operator[] access") {
        buffer buf(100);
        buf.emplace_back<unsigned char>();
        buf[0] = 0xAB;
        CHECK(buf[0] == 0xAB);
    }

    TEST_CASE("copy construction") {
        buffer buf1(100);
        unsigned char* p = buf1.emplace_back<unsigned char>();
        *p = 0x42;
        
        buffer buf2(buf1);
        CHECK(buf2.size() == buf1.size());
        CHECK(buf2[0] == 0x42);
        // Verify it's a deep copy
        buf2[0] = 0xFF;
        CHECK(buf1[0] == 0x42);  // Original unchanged
    }

    TEST_CASE("swap") {
        buffer buf1(100);
        buf1.emplace_back<int>();
        
        buffer buf2(200);
        buf2.emplace_back<double>();
        
        size_t size1 = buf1.size();
        size_t size2 = buf2.size();
        size_t cap1 = buf1.capacity();
        size_t cap2 = buf2.capacity();
        
        buf1.swap(buf2);
        
        CHECK(buf1.size() == size2);
        CHECK(buf2.size() == size1);
        CHECK(buf1.capacity() == cap2);
        CHECK(buf2.capacity() == cap1);
    }

    TEST_CASE("clear") {
        buffer buf(100);
        buf.emplace_back<int>();
        CHECK_FALSE(buf.empty());
        
        buf.clear();
        CHECK(buf.empty());
        CHECK(buf.size() == 0);
    }

    TEST_CASE("tail pointer") {
        buffer buf(100);
        CHECK(buf.tail() == buf.begin());
        
        buf.emplace_back<int>();
        CHECK(buf.tail() == buf.begin() + sizeof(int));
    }

    TEST_CASE("reserve_bytes grows capacity") {
        buffer buf(10);
        CHECK(buf.capacity() == 10);
        
        buf.reserve_bytes(100);
        CHECK(buf.capacity() >= 100);
    }

    TEST_CASE("assignment operator") {
        buffer buf1(100);
        buf1.emplace_back<int>();
        
        buffer buf2;
        buf2 = buf1;
        
        CHECK(buf2.size() == buf1.size());
    }
}

