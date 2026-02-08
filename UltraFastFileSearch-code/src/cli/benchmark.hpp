/**
 * @file benchmark.hpp
 * @brief Index build benchmarking tool for UFFS.
 *
 * This file provides a benchmarking function that measures the full UFFS
 * indexing pipeline performance, including async I/O, MFT parsing, and
 * index building.
 *
 * ## Architecture Overview
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    BENCHMARK PIPELINE                                   │
 * │                                                                         │
 * │  ┌─────────────┐     ┌──────────────────┐     ┌───────────────────┐    │
 * │  │ NtfsIndex   │────▶│ IoCompletionPort │────▶│ MftReadPayload    │    │
 * │  │ (container) │     │ (async I/O)      │     │ (MFT reader)      │    │
 * │  └─────────────┘     └──────────────────┘     └───────────────────┘    │
 * │         │                    │                         │               │
 * │         │                    │                         │               │
 * │         ▼                    ▼                         ▼               │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                    TIMING & STATISTICS                          │   │
 * │  │  - Wall clock time (chrono::high_resolution_clock)              │   │
 * │  │  - CPU time (clock())                                           │   │
 * │  │  - Records processed                                            │   │
 * │  │  - Names indexed                                                │   │
 * │  │  - Throughput (MB/s, records/sec, names/sec)                    │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Usage
 *
 * ```cpp
 * #include "src/cli/benchmark.hpp"
 *
 * int main() {
 *     int result = uffs::benchmark_index_build('C', std::cout);
 *     return result;
 * }
 * ```
 *
 * ## Output Format
 *
 * The benchmark outputs:
 * 1. Volume information (MFT capacity, record size, total size)
 * 2. Index statistics (records processed, name entries)
 * 3. Benchmark results (time, throughput metrics)
 * 4. Summary line for easy comparison
 *
 * @note Requires administrator privileges to read the MFT.
 *
 * @see ntfs_index.hpp for the NtfsIndex class
 * @see io_completion_port.hpp for async I/O
 * @see mft_reader.hpp for MFT reading
 */

#ifndef UFFS_BENCHMARK_HPP
#define UFFS_BENCHMARK_HPP

#include <chrono>
#include <iomanip>
#include <iostream>
#include <tchar.h>
#include <Windows.h>

// Forward declarations - these headers are included via textual inclusion
// in UltraFastFileSearch.cpp before this file is included
// class NtfsIndex;
// class IoCompletionPort;
// class OverlappedNtfsMftReadPayload;

namespace uffs {

/**
 * @brief Benchmarks the full index building pipeline.
 *
 * This function creates an NtfsIndex for the specified drive and measures
 * the time taken to complete the full indexing process.
 *
 * ## Algorithm
 *
 * 1. Create NtfsIndex for the specified drive
 * 2. Create IoCompletionPort for async I/O
 * 3. Post MftReadPayload to start indexing
 * 4. Wait for indexing to complete
 * 5. Calculate and output statistics
 *
 * ## Error Handling
 *
 * Returns non-zero error codes for:
 * - ERROR_ACCESS_DENIED: Not running as administrator
 * - ERROR_UNRECOGNIZED_VOLUME: Not an NTFS volume
 * - ERROR_WAIT_1: Wait operation failed
 *
 * @param drive_letter  The drive letter to benchmark (e.g., 'C')
 * @param OS            Output stream for results (e.g., std::cout)
 * @return 0 on success, error code on failure
 */
inline int benchmark_index_build(char drive_letter, std::ostream& OS)
{
    OS << "\n=== Index Build Benchmark Tool ===\n";
    OS << "Drive: " << drive_letter << ":\n";
    OS << "This measures the full UFFS indexing pipeline "
       << "(async I/O + parsing + index building)\n\n";

    // Build path name (e.g., "C:\")
    TCHAR path_buf[4] = {
        static_cast<TCHAR>(toupper(drive_letter)),
        _T(':'),
        _T('\\'),
        _T('\0')
    };
    std::tvstring path_name(path_buf);

    OS << "Creating index for " << static_cast<char>(toupper(drive_letter)) << ":\\ ...\n";
    OS.flush();

    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();
    clock_t tbegin = clock();

    // Create the index
    intrusive_ptr<NtfsIndex> index(new NtfsIndex(path_name), true);

    // Create IOCP and closing event
    IoCompletionPort iocp;
    Handle closing_event;

    // Post the read payload to start async indexing
    typedef OverlappedNtfsMftReadPayload T;
    intrusive_ptr<T> payload(new T(iocp, index, closing_event));
    iocp.post(0, 0, payload);

    // Wait for indexing to complete
    OS << "Indexing in progress...\n";
    OS.flush();

    HANDLE wait_handle = reinterpret_cast<HANDLE>(index->finished_event());
    DWORD wait_result = WaitForSingleObject(wait_handle, INFINITE);

    if (wait_result != WAIT_OBJECT_0) {
        OS << "ERROR: Wait failed (result=" << wait_result << ")\n";
        return ERROR_WAIT_1;
    }

    // Stop timing
    auto end_time = std::chrono::high_resolution_clock::now();
    clock_t tend = clock();

    // Check for indexing errors
    unsigned int task_result = index->get_finished();
    if (task_result != 0) {
        OS << "ERROR: Indexing failed with error code " << task_result << "\n";
        if (task_result == ERROR_ACCESS_DENIED) {
            OS << "Make sure you are running as Administrator.\n";
        } else if (task_result == ERROR_UNRECOGNIZED_VOLUME) {
            OS << "The volume is not NTFS formatted.\n";
        }
        return static_cast<int>(task_result);
    }

    // Calculate timing
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    double seconds = static_cast<double>(duration.count()) / 1000.0;
    double clock_seconds = static_cast<double>(tend - tbegin) / CLOCKS_PER_SEC;

    // Get index statistics from public accessors
    size_t total_records = index->records_so_far();
    size_t total_names = index->total_names();
    size_t total_names_and_streams = index->total_names_and_streams();
    unsigned int mft_capacity = index->mft_capacity();
    unsigned int mft_record_size = index->mft_record_size();
    unsigned long long mft_bytes =
        static_cast<unsigned long long>(mft_capacity) * mft_record_size;

    // Calculate throughput
    double mb_per_sec = (seconds > 0)
        ? (static_cast<double>(mft_bytes) / (1024.0 * 1024.0)) / seconds
        : 0;
    double records_per_sec = (seconds > 0)
        ? static_cast<double>(total_records) / seconds
        : 0;
    double names_per_sec = (seconds > 0)
        ? static_cast<double>(total_names) / seconds
        : 0;

    // Output results
    OS << "\n=== Volume Information ===\n";
    OS << "MFT Capacity: " << mft_capacity << " records\n";
    OS << "MFT Record Size: " << mft_record_size << " bytes\n";
    OS << "MFT Total Size: " << mft_bytes << " bytes ("
       << (mft_bytes / (1024 * 1024)) << " MB)\n";

    OS << "\n=== Index Statistics ===\n";
    OS << "Records Processed: " << total_records << "\n";
    OS << "Name Entries: " << total_names << "\n";
    OS << "Names + Streams: " << total_names_and_streams << "\n";

    OS << "\n=== Benchmark Results ===\n";
    OS << "Time Elapsed: " << duration.count() << " ms ("
       << std::fixed << std::setprecision(3) << seconds << " seconds)\n";
    OS << "CPU Time: " << std::fixed << std::setprecision(3)
       << clock_seconds << " seconds\n";
    OS << "MFT Read Speed: " << std::fixed << std::setprecision(2)
       << mb_per_sec << " MB/s\n";
    OS << "Record Processing: " << std::fixed << std::setprecision(0)
       << records_per_sec << " records/sec\n";
    OS << "Name Indexing: " << std::fixed << std::setprecision(0)
       << names_per_sec << " names/sec\n";

    // Summary line for easy comparison
    OS << "\n=== Summary ===\n";
    OS << "Indexed " << total_names << " names in "
       << std::fixed << std::setprecision(3) << seconds << " seconds\n";

    return 0;
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::benchmark_index_build;

#endif // UFFS_BENCHMARK_HPP
