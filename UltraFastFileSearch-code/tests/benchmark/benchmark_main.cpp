// ============================================================================
// UFFS Benchmark Suite
// ============================================================================
// Performance benchmarks for NTFS indexing and search operations
// Uses doctest for test organization with manual timing
// ============================================================================

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../doctest.h"

#include <chrono>
#include <iostream>
#include <iomanip>

// Simple benchmark helper
class Benchmark {
public:
    Benchmark(const char* name) : name_(name) {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    ~Benchmark() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        std::cout << "  [BENCH] " << name_ << ": " 
                  << std::fixed << std::setprecision(3) 
                  << (duration.count() / 1000.0) << " ms" << std::endl;
    }
    
private:
    const char* name_;
    std::chrono::high_resolution_clock::time_point start_;
};

#define BENCHMARK(name) Benchmark _bench_##__LINE__(name)

// ============================================================================
// Packed Type Benchmarks
// ============================================================================

#include "../../src/core/packed_file_size.hpp"

TEST_SUITE("Benchmarks") {
    TEST_CASE("file_size_type construction (1M iterations)") {
        BENCHMARK("file_size_type construction");
        volatile unsigned long long sum = 0;
        for (int i = 0; i < 1000000; ++i) {
            uffs::file_size_type size(static_cast<unsigned long long>(i));
            sum += static_cast<unsigned long long>(size);
        }
        CHECK(sum > 0);  // Prevent optimization
    }

    TEST_CASE("file_size_type arithmetic (1M iterations)") {
        BENCHMARK("file_size_type +=/-=");
        uffs::file_size_type size(0);
        for (int i = 0; i < 500000; ++i) {
            size += uffs::file_size_type(100);
        }
        for (int i = 0; i < 500000; ++i) {
            size -= uffs::file_size_type(100);
        }
        CHECK(static_cast<unsigned long long>(size) == 0);
    }
}

// ============================================================================
// Future benchmarks (require Windows)
// ============================================================================
// TODO: Add when running on Windows:
// - NtfsIndex::load() benchmark
// - Search pattern matching benchmark
// - Memory allocation benchmark
// - I/O completion port throughput

