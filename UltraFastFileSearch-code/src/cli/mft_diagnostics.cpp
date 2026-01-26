// ============================================================================
// MFT Diagnostic Tools - Implementation
// ============================================================================
// Extracted from UltraFastFileSearch.cpp
// These functions provide diagnostic and benchmarking capabilities for NTFS MFT.
// ============================================================================

#include "mft_diagnostics.hpp"

#include <windows.h>
#include <winioctl.h>
#include <tchar.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>

// For get_retrieval_pointers
#include "../util/volume_utils.hpp"

// For std::tvstring
#include "../util/string_utils.hpp"

namespace uffs {

// ============================================================================
// UFFS-MFT Header structure (64 bytes)
// ============================================================================
#pragma pack(push, 1)
struct UffsMftHeader {
    char magic[8];           // "UFFS-MFT"
    uint32_t version;        // 1
    uint32_t flags;          // 0 = no compression
    uint32_t record_size;    // e.g., 1024
    uint64_t record_count;   // number of MFT records
    uint64_t original_size;  // total bytes = record_size * record_count
    uint64_t compressed_size;// 0 for uncompressed
    uint8_t reserved[20];    // padding to 64 bytes
};
#pragma pack(pop)

static_assert(sizeof(UffsMftHeader) == 64, "UffsMftHeader must be exactly 64 bytes");

// ============================================================================
// dump_raw_mft - Dump raw MFT to file in UFFS-MFT format
// ============================================================================
int dump_raw_mft(char drive_letter, const char* output_path, std::ostream& OS)
{
    OS << "\n=== Raw MFT Dump Tool ===\n";
    OS << "Drive: " << drive_letter << ":\n";
    OS << "Output: " << output_path << "\n\n";

    // Build volume path: \\.\X:
    std::wstring volume_path = L"\\\\.\\";
    volume_path += static_cast<wchar_t>(toupper(drive_letter));
    volume_path += L":";

    // Open volume handle
    HANDLE volume_handle = CreateFileW(
        volume_path.c_str(),
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        nullptr
    );

    if (volume_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        OS << "ERROR: Failed to open volume " << drive_letter << ": (error " << err << ")\n";
        OS << "Make sure you are running as Administrator.\n";
        return static_cast<int>(err);
    }

    // Get NTFS volume data
    NTFS_VOLUME_DATA_BUFFER volume_data = {};
    DWORD bytes_returned = 0;

    if (!DeviceIoControl(
        volume_handle,
        FSCTL_GET_NTFS_VOLUME_DATA,
        nullptr, 0,
        &volume_data, sizeof(volume_data),
        &bytes_returned,
        nullptr
    )) {
        DWORD err = GetLastError();
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to get NTFS volume data (error " << err << ")\n";
        return static_cast<int>(err);
    }

    OS << "Volume Information:\n";
    OS << "  BytesPerSector: " << volume_data.BytesPerSector << "\n";
    OS << "  BytesPerCluster: " << volume_data.BytesPerCluster << "\n";
    OS << "  BytesPerFileRecordSegment: " << volume_data.BytesPerFileRecordSegment << "\n";
    OS << "  MftValidDataLength: " << volume_data.MftValidDataLength.QuadPart << "\n";
    OS << "  MftStartLcn: " << volume_data.MftStartLcn.QuadPart << "\n\n";

    // Build $MFT path for retrieval pointers
    std::wstring mft_path = L"\\\\.\\";
    mft_path += static_cast<wchar_t>(toupper(drive_letter));
    mft_path += L":\\$MFT";

    // Get MFT extents using get_retrieval_pointers
    long long mft_size = 0;
    std::vector<std::pair<unsigned long long, long long>> ret_ptrs;

    try {
        std::tstring mft_path_t;
        mft_path_t = drive_letter;
        mft_path_t += _T(":\\$MFT");
        ret_ptrs = get_retrieval_pointers(mft_path_t.c_str(), &mft_size,
            volume_data.MftStartLcn.QuadPart, volume_data.BytesPerFileRecordSegment);
    } catch (...) {
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to get MFT retrieval pointers\n";
        return ERROR_READ_FAULT;
    }

    if (ret_ptrs.empty()) {
        CloseHandle(volume_handle);
        OS << "ERROR: No MFT extents found\n";
        return ERROR_READ_FAULT;
    }

    OS << "MFT Extents: " << ret_ptrs.size() << "\n";
    OS << "MFT Size: " << mft_size << " bytes\n";

    // Calculate record count
    uint32_t record_size = volume_data.BytesPerFileRecordSegment;
    uint64_t record_count = static_cast<uint64_t>(mft_size) / record_size;
    uint64_t total_bytes = record_count * record_size;

    OS << "Record Size: " << record_size << " bytes\n";
    OS << "Record Count: " << record_count << "\n";
    OS << "Total Bytes to Write: " << total_bytes << "\n\n";

    // Open output file
    HANDLE out_handle = CreateFileA(
        output_path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (out_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to create output file (error " << err << ")\n";
        return static_cast<int>(err);
    }

    // Write UFFS-MFT header
    UffsMftHeader header = {};
    memcpy(header.magic, "UFFS-MFT", 8);
    header.version = 1;
    header.flags = 0;  // No compression
    header.record_size = record_size;
    header.record_count = record_count;
    header.original_size = total_bytes;
    header.compressed_size = 0;
    memset(header.reserved, 0, sizeof(header.reserved));

    DWORD written = 0;
    if (!WriteFile(out_handle, &header, sizeof(header), &written, nullptr) || written != sizeof(header)) {
        DWORD err = GetLastError();
        CloseHandle(out_handle);
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to write header (error " << err << ")\n";
        return static_cast<int>(err);
    }

    OS << "Reading MFT data...\n";

    // Read MFT data extent by extent
    uint64_t bytes_written = 0;
    uint64_t cluster_size = volume_data.BytesPerCluster;

    // Allocate aligned read buffer (must be sector-aligned for FILE_FLAG_NO_BUFFERING)
    size_t buffer_size = 1024 * 1024;  // 1MB buffer
    // Align to sector size
    buffer_size = (buffer_size / volume_data.BytesPerSector) * volume_data.BytesPerSector;
    std::vector<unsigned char> read_buffer(buffer_size);

    unsigned long long prev_vcn = 0;
    for (size_t i = 0; i < ret_ptrs.size(); ++i) {
        unsigned long long next_vcn = ret_ptrs[i].first;
        long long lcn = ret_ptrs[i].second;

        // Calculate cluster count for this extent
        unsigned long long cluster_count = next_vcn - prev_vcn;
        if (cluster_count == 0) continue;

        // Calculate byte offset and length
        long long byte_offset = lcn * static_cast<long long>(cluster_size);
        unsigned long long byte_length = cluster_count * cluster_size;

        // Read in chunks
        unsigned long long extent_bytes_read = 0;
        while (extent_bytes_read < byte_length && bytes_written < total_bytes) {
            // Seek to position
            LARGE_INTEGER seek_pos = {};
            seek_pos.QuadPart = byte_offset + static_cast<long long>(extent_bytes_read);
            if (!SetFilePointerEx(volume_handle, seek_pos, nullptr, FILE_BEGIN)) {
                DWORD err = GetLastError();
                CloseHandle(out_handle);
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to seek (error " << err << ")\n";
                return static_cast<int>(err);
            }

            // Calculate how much to read
            unsigned long long remaining_in_extent = byte_length - extent_bytes_read;
            unsigned long long remaining_total = total_bytes - bytes_written;
            size_t to_read = static_cast<size_t>(std::min({
                static_cast<unsigned long long>(buffer_size),
                remaining_in_extent,
                remaining_total
            }));
            // Align to sector size for reading
            to_read = (to_read / volume_data.BytesPerSector) * volume_data.BytesPerSector;
            if (to_read == 0) to_read = volume_data.BytesPerSector;

            DWORD bytes_read = 0;
            if (!ReadFile(volume_handle, read_buffer.data(), static_cast<DWORD>(to_read), &bytes_read, nullptr)) {
                DWORD err = GetLastError();
                CloseHandle(out_handle);
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to read from volume (error " << err << ")\n";
                return static_cast<int>(err);
            }

            // Write to output (may need to trim to not exceed total_bytes)
            size_t to_write = static_cast<size_t>(std::min(
                static_cast<unsigned long long>(bytes_read),
                total_bytes - bytes_written
            ));

            DWORD out_written = 0;
            if (!WriteFile(out_handle, read_buffer.data(), static_cast<DWORD>(to_write), &out_written, nullptr)) {
                DWORD err = GetLastError();
                CloseHandle(out_handle);
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to write to output (error " << err << ")\n";
                return static_cast<int>(err);
            }

            bytes_written += out_written;
            extent_bytes_read += bytes_read;

            // Progress indicator
            if ((bytes_written % (100 * 1024 * 1024)) == 0) {
                OS << "  Progress: " << (bytes_written / (1024 * 1024)) << " MB / "
                   << (total_bytes / (1024 * 1024)) << " MB\n";
            }
        }

        prev_vcn = next_vcn;
    }

    CloseHandle(out_handle);
    CloseHandle(volume_handle);

    OS << "\n=== Dump Complete ===\n";
    OS << "Total extents: " << ret_ptrs.size() << "\n";
    OS << "Total bytes written: " << bytes_written << "\n";
    OS << "Record count: " << record_count << "\n";
    OS << "Output file: " << output_path << "\n";

    return 0;
}

// ============================================================================
// dump_mft_extents - Dump MFT extents as JSON for diagnostic purposes
// ============================================================================
int dump_mft_extents(char drive_letter, const char* output_path, bool verify_extents, std::ostream& OS)
{
    // Get current timestamp in ISO 8601 format
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm tm_now;
    gmtime_s(&tm_now, &time_t_now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &tm_now);

    // Build volume path: \\.\X:
    std::wstring volume_path = L"\\\\.\\";
    volume_path += static_cast<wchar_t>(toupper(drive_letter));
    volume_path += L":";

    // Open volume handle
    HANDLE volume_handle = CreateFileW(
        volume_path.c_str(),
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        nullptr
    );

    if (volume_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        OS << "{\"error\": \"Failed to open volume " << drive_letter << ": (error " << err << ")\"}\n";
        return static_cast<int>(err);
    }

    // Get NTFS volume data
    NTFS_VOLUME_DATA_BUFFER volume_data = {};
    DWORD bytes_returned = 0;

    if (!DeviceIoControl(
        volume_handle,
        FSCTL_GET_NTFS_VOLUME_DATA,
        nullptr, 0,
        &volume_data, sizeof(volume_data),
        &bytes_returned,
        nullptr
    )) {
        DWORD err = GetLastError();
        CloseHandle(volume_handle);
        OS << "{\"error\": \"Failed to get NTFS volume data (error " << err << ")\"}\n";
        return static_cast<int>(err);
    }

    // Get MFT extents using get_retrieval_pointers
    long long mft_size = 0;
    std::vector<std::pair<unsigned long long, long long>> ret_ptrs;

    try {
        std::tstring mft_path_t;
        mft_path_t = drive_letter;
        mft_path_t += _T(":\\$MFT");
        ret_ptrs = get_retrieval_pointers(mft_path_t.c_str(), &mft_size,
            volume_data.MftStartLcn.QuadPart, volume_data.BytesPerFileRecordSegment);
    } catch (...) {
        CloseHandle(volume_handle);
        OS << "{\"error\": \"Failed to get MFT retrieval pointers\"}\n";
        return ERROR_READ_FAULT;
    }

    if (ret_ptrs.empty()) {
        CloseHandle(volume_handle);
        OS << "{\"error\": \"No MFT extents found\"}\n";
        return ERROR_READ_FAULT;
    }

    uint64_t bytes_per_cluster = volume_data.BytesPerCluster;
    uint32_t record_size = volume_data.BytesPerFileRecordSegment;
    uint64_t records_per_cluster = bytes_per_cluster / record_size;

    // Build JSON output
    std::ostringstream json;
    json << "{\n";
    json << "  \"drive\": \"" << static_cast<char>(toupper(drive_letter)) << "\",\n";
    json << "  \"timestamp\": \"" << timestamp << "\",\n";
    json << "  \"volume_info\": {\n";
    json << "    \"bytes_per_sector\": " << volume_data.BytesPerSector << ",\n";
    json << "    \"bytes_per_cluster\": " << bytes_per_cluster << ",\n";
    json << "    \"bytes_per_file_record\": " << record_size << ",\n";
    json << "    \"mft_start_lcn\": " << volume_data.MftStartLcn.QuadPart << ",\n";
    json << "    \"mft_valid_data_length\": " << volume_data.MftValidDataLength.QuadPart << ",\n";
    json << "    \"total_clusters\": " << volume_data.TotalClusters.QuadPart << "\n";
    json << "  },\n";
    json << "  \"mft_extents\": [\n";

    uint64_t total_clusters = 0;
    uint64_t total_records = 0;
    uint64_t prev_vcn = 0;

    // Allocate buffer for verification reads (one cluster)
    std::vector<uint8_t> verify_buffer;
    if (verify_extents) {
        verify_buffer.resize(static_cast<size_t>(bytes_per_cluster));
    }

    for (size_t i = 0; i < ret_ptrs.size(); ++i) {
        uint64_t next_vcn = ret_ptrs[i].first;
        int64_t lcn = ret_ptrs[i].second;
        uint64_t cluster_count = next_vcn - prev_vcn;
        uint64_t start_frs = prev_vcn * records_per_cluster;
        uint64_t end_frs = start_frs + (cluster_count * records_per_cluster) - 1;
        uint64_t byte_offset = static_cast<uint64_t>(lcn) * bytes_per_cluster;
        uint64_t byte_length = cluster_count * bytes_per_cluster;

        total_clusters += cluster_count;
        total_records = end_frs + 1;

        json << "    {\n";
        json << "      \"index\": " << i << ",\n";
        json << "      \"vcn\": " << prev_vcn << ",\n";
        json << "      \"lcn\": " << lcn << ",\n";
        json << "      \"cluster_count\": " << cluster_count << ",\n";
        json << "      \"start_frs\": " << start_frs << ",\n";
        json << "      \"end_frs\": " << end_frs << ",\n";
        json << "      \"byte_offset\": " << byte_offset << ",\n";
        json << "      \"byte_length\": " << byte_length;

        // Verification: read first record from this extent and check FRS number
        if (verify_extents && lcn >= 0) {
            LARGE_INTEGER seek_pos = {};
            seek_pos.QuadPart = static_cast<LONGLONG>(byte_offset);

            if (SetFilePointerEx(volume_handle, seek_pos, nullptr, FILE_BEGIN)) {
                DWORD bytes_read = 0;
                if (ReadFile(volume_handle, verify_buffer.data(),
                    static_cast<DWORD>(bytes_per_cluster), &bytes_read, nullptr) && bytes_read >= record_size) {
                    // Check FILE signature and extract FRS number from record header
                    bool valid_signature = (verify_buffer[0] == 'F' && verify_buffer[1] == 'I' &&
                                           verify_buffer[2] == 'L' && verify_buffer[3] == 'E');
                    uint64_t header_frs = 0;
                    if (record_size >= 48) {
                        // FRS is at offset 44 (0x2C), 6 bytes (48-bit) in little-endian
                        header_frs = static_cast<uint64_t>(verify_buffer[44]) |
                                    (static_cast<uint64_t>(verify_buffer[45]) << 8) |
                                    (static_cast<uint64_t>(verify_buffer[46]) << 16) |
                                    (static_cast<uint64_t>(verify_buffer[47]) << 24) |
                                    (static_cast<uint64_t>(verify_buffer[48]) << 32) |
                                    (static_cast<uint64_t>(verify_buffer[49]) << 40);
                    }
                    json << ",\n      \"verify\": {\n";
                    json << "        \"valid_signature\": " << (valid_signature ? "true" : "false") << ",\n";
                    json << "        \"header_frs\": " << header_frs << ",\n";
                    json << "        \"expected_frs\": " << start_frs << ",\n";
                    json << "        \"match\": " << ((header_frs == start_frs) ? "true" : "false") << "\n";
                    json << "      }";
                } else {
                    json << ",\n      \"verify\": {\"error\": \"read_failed\"}";
                }
            } else {
                json << ",\n      \"verify\": {\"error\": \"seek_failed\"}";
            }
        }

        json << "\n    }";
        if (i < ret_ptrs.size() - 1) {
            json << ",";
        }
        json << "\n";

        prev_vcn = next_vcn;
    }

    json << "  ],\n";
    json << "  \"summary\": {\n";
    json << "    \"extent_count\": " << ret_ptrs.size() << ",\n";
    json << "    \"total_clusters\": " << total_clusters << ",\n";
    json << "    \"total_records\": " << total_records << ",\n";
    json << "    \"total_bytes\": " << (total_clusters * bytes_per_cluster) << ",\n";
    json << "    \"is_fragmented\": " << (ret_ptrs.size() > 1 ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}\n";

    CloseHandle(volume_handle);

    // Output to file or stdout
    std::string json_str = json.str();

    if (output_path && strlen(output_path) > 0) {
        std::ofstream out_file(output_path, std::ios::binary);
        if (!out_file) {
            OS << "{\"error\": \"Failed to create output file: " << output_path << "\"}\n";
            return ERROR_CANNOT_MAKE;
        }
        out_file.write(json_str.c_str(), json_str.size());
        out_file.close();
        OS << "MFT extent data written to: " << output_path << "\n";
        OS << "Extents: " << ret_ptrs.size() << ", Total records: " << total_records << "\n";
    } else {
        // Output to stdout
        OS << json_str;
    }

    return 0;
}

// ============================================================================
// benchmark_mft_read - Benchmark raw MFT reading speed
// ============================================================================
int benchmark_mft_read(char drive_letter, std::ostream& OS)
{
    OS << "\n=== MFT Read Benchmark Tool ===\n";
    OS << "Drive: " << drive_letter << ":\n\n";

    // Build volume path: \\.\X:
    std::wstring volume_path = L"\\\\.\\";
    volume_path += static_cast<wchar_t>(toupper(drive_letter));
    volume_path += L":";

    // Open volume handle
    HANDLE volume_handle = CreateFileW(
        volume_path.c_str(),
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        nullptr
    );

    if (volume_handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        OS << "ERROR: Failed to open volume " << drive_letter << ": (error " << err << ")\n";
        OS << "Make sure you are running as Administrator.\n";
        return static_cast<int>(err);
    }

    // Get NTFS volume data
    NTFS_VOLUME_DATA_BUFFER volume_data = {};
    DWORD bytes_returned = 0;

    if (!DeviceIoControl(
        volume_handle,
        FSCTL_GET_NTFS_VOLUME_DATA,
        nullptr, 0,
        &volume_data, sizeof(volume_data),
        &bytes_returned,
        nullptr
    )) {
        DWORD err = GetLastError();
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to get NTFS volume data (error " << err << ")\n";
        return static_cast<int>(err);
    }

    OS << "Volume Information:\n";
    OS << "  BytesPerSector: " << volume_data.BytesPerSector << "\n";
    OS << "  BytesPerCluster: " << volume_data.BytesPerCluster << "\n";
    OS << "  BytesPerFileRecordSegment: " << volume_data.BytesPerFileRecordSegment << "\n";
    OS << "  MftValidDataLength: " << volume_data.MftValidDataLength.QuadPart << "\n";
    OS << "  MftStartLcn: " << volume_data.MftStartLcn.QuadPart << "\n\n";

    // Get MFT extents using get_retrieval_pointers
    long long mft_size = 0;
    std::vector<std::pair<unsigned long long, long long>> ret_ptrs;

    try {
        std::tstring mft_path_t;
        mft_path_t = drive_letter;
        mft_path_t += _T(":\\$MFT");
        ret_ptrs = get_retrieval_pointers(mft_path_t.c_str(), &mft_size,
            volume_data.MftStartLcn.QuadPart, volume_data.BytesPerFileRecordSegment);
    } catch (...) {
        CloseHandle(volume_handle);
        OS << "ERROR: Failed to get MFT retrieval pointers\n";
        return ERROR_READ_FAULT;
    }

    if (ret_ptrs.empty()) {
        CloseHandle(volume_handle);
        OS << "ERROR: No MFT extents found\n";
        return ERROR_READ_FAULT;
    }

    // Calculate sizes
    uint32_t record_size = volume_data.BytesPerFileRecordSegment;
    uint64_t record_count = static_cast<uint64_t>(mft_size) / record_size;
    uint64_t total_bytes = record_count * record_size;
    uint64_t cluster_size = volume_data.BytesPerCluster;

    OS << "MFT Information:\n";
    OS << "  Extents: " << ret_ptrs.size() << "\n";
    OS << "  MFT Size: " << mft_size << " bytes (" << (mft_size / (1024 * 1024)) << " MB)\n";
    OS << "  Record Size: " << record_size << " bytes\n";
    OS << "  Record Count: " << record_count << "\n";
    OS << "  Total Bytes to Read: " << total_bytes << "\n\n";

    // Allocate aligned read buffer (must be sector-aligned for FILE_FLAG_NO_BUFFERING)
    size_t buffer_size = 1024 * 1024;  // 1MB buffer
    buffer_size = (buffer_size / volume_data.BytesPerSector) * volume_data.BytesPerSector;
    std::vector<unsigned char> read_buffer(buffer_size);

    // Variables to capture first and last 4 bytes
    unsigned char first_4_bytes[4] = {0, 0, 0, 0};
    unsigned char last_4_bytes[4] = {0, 0, 0, 0};
    bool captured_first = false;

    OS << "Starting MFT read benchmark...\n";
    OS.flush();

    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();

    // Read MFT data extent by extent
    uint64_t bytes_read_total = 0;
    unsigned long long prev_vcn = 0;

    for (size_t i = 0; i < ret_ptrs.size(); ++i) {
        unsigned long long next_vcn = ret_ptrs[i].first;
        long long lcn = ret_ptrs[i].second;

        // Calculate cluster count for this extent
        unsigned long long cluster_count = next_vcn - prev_vcn;
        if (cluster_count == 0) continue;

        // Calculate byte offset and length
        long long byte_offset = lcn * static_cast<long long>(cluster_size);
        unsigned long long byte_length = cluster_count * cluster_size;

        // Read in chunks
        unsigned long long extent_bytes_read = 0;
        while (extent_bytes_read < byte_length && bytes_read_total < total_bytes) {
            // Seek to position
            LARGE_INTEGER seek_pos = {};
            seek_pos.QuadPart = byte_offset + static_cast<long long>(extent_bytes_read);
            if (!SetFilePointerEx(volume_handle, seek_pos, nullptr, FILE_BEGIN)) {
                DWORD err = GetLastError();
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to seek (error " << err << ")\n";
                return static_cast<int>(err);
            }

            // Calculate how much to read
            unsigned long long remaining_in_extent = byte_length - extent_bytes_read;
            unsigned long long remaining_total = total_bytes - bytes_read_total;
            size_t to_read = static_cast<size_t>(std::min({
                static_cast<unsigned long long>(buffer_size),
                remaining_in_extent,
                remaining_total
            }));
            // Align to sector size for reading
            to_read = (to_read / volume_data.BytesPerSector) * volume_data.BytesPerSector;
            if (to_read == 0) to_read = volume_data.BytesPerSector;

            DWORD bytes_read = 0;
            if (!ReadFile(volume_handle, read_buffer.data(), static_cast<DWORD>(to_read), &bytes_read, nullptr)) {
                DWORD err = GetLastError();
                CloseHandle(volume_handle);
                OS << "ERROR: Failed to read from volume (error " << err << ")\n";
                return static_cast<int>(err);
            }

            // Capture first 4 bytes (from very first read)
            if (!captured_first && bytes_read >= 4) {
                memcpy(first_4_bytes, read_buffer.data(), 4);
                captured_first = true;
            }

            // Always update last 4 bytes (from the actual MFT data portion)
            size_t actual_data_in_buffer = static_cast<size_t>(std::min(
                static_cast<unsigned long long>(bytes_read),
                total_bytes - bytes_read_total
            ));
            if (actual_data_in_buffer >= 4) {
                memcpy(last_4_bytes, read_buffer.data() + actual_data_in_buffer - 4, 4);
            }

            bytes_read_total += actual_data_in_buffer;
            extent_bytes_read += bytes_read;
        }

        prev_vcn = next_vcn;
    }

    // Stop timing
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double seconds = static_cast<double>(duration.count()) / 1000.0;
    double mb_per_sec = (seconds > 0) ? (static_cast<double>(bytes_read_total) / (1024.0 * 1024.0)) / seconds : 0;

    CloseHandle(volume_handle);

    // Output results
    OS << "\n=== Benchmark Results ===\n";
    OS << "Total bytes read: " << bytes_read_total << " (" << (bytes_read_total / (1024 * 1024)) << " MB)\n";
    OS << "Total records: " << record_count << "\n";
    OS << "Time elapsed: " << duration.count() << " ms (" << std::fixed << std::setprecision(3) << seconds << " seconds)\n";
    OS << "Read speed: " << std::fixed << std::setprecision(2) << mb_per_sec << " MB/s\n\n";

    // Proof of reading - first and last 4 bytes
    OS << "=== Proof of Complete Read ===\n";
    OS << "First 4 bytes (hex): ";
    for (int i = 0; i < 4; ++i) {
        OS << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(first_4_bytes[i]);
        if (i < 3) OS << " ";
    }
    OS << std::dec;  // Reset to decimal
    OS << "  (ASCII: ";
    for (int i = 0; i < 4; ++i) {
        char c = static_cast<char>(first_4_bytes[i]);
        OS << (isprint(c) ? c : '.');
    }
    OS << ")\n";

    OS << "Last 4 bytes (hex):  ";
    for (int i = 0; i < 4; ++i) {
        OS << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(last_4_bytes[i]);
        if (i < 3) OS << " ";
    }
    OS << std::dec;  // Reset to decimal
    OS << "  (ASCII: ";
    for (int i = 0; i < 4; ++i) {
        char c = static_cast<char>(last_4_bytes[i]);
        OS << (isprint(c) ? c : '.');
    }
    OS << ")\n";

    // Note about expected values
    OS << "\nNote: First 4 bytes should be 'FILE' (46 49 4C 45) - the MFT record signature.\n";

    return 0;
}

} // namespace uffs
