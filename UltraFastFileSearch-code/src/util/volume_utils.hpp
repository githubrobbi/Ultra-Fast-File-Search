/**
 * @file volume_utils.hpp
 * @brief Volume information and extent mapping utilities for NTFS
 *
 * @details
 * This file provides utilities for querying volume information and
 * retrieving file extent maps (retrieval pointers) for NTFS volumes.
 *
 * ## Key Functions
 *
 * | Function                | Description                              |
 * |-------------------------|------------------------------------------|
 * | get_cluster_size()      | Query cluster size for a volume          |
 * | get_volume_path_names() | Enumerate all logical drives             |
 * | get_retrieval_pointers()| Get file extent map (VCN/LCN pairs)      |
 *
 * ## Retrieval Pointers (Extent Map)
 *
 * NTFS stores file data in "extents" - contiguous runs of clusters.
 * The retrieval pointers describe where each extent is located:
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    File Extent Map                              │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  File Data (logical view):                                      │
 * │  ┌──────────────────────────────────────────────────────────┐  │
 * │  │ Cluster 0-99 │ Cluster 100-199 │ Cluster 200-299 │ ...   │  │
 * │  └──────────────────────────────────────────────────────────┘  │
 * │                                                                 │
 * │  Disk Layout (physical):                                        │
 * │  ┌────────┐     ┌────────┐     ┌────────┐                      │
 * │  │LCN 1000│     │LCN 5000│     │LCN 2000│                      │
 * │  │100 clus│     │100 clus│     │100 clus│                      │
 * │  └────────┘     └────────┘     └────────┘                      │
 * │                                                                 │
 * │  Retrieval Pointers:                                            │
 * │  [(100, 1000), (200, 5000), (300, 2000)]                       │
 * │   ^VCN  ^LCN    ^VCN  ^LCN   ^VCN  ^LCN                        │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Thread Safety
 *
 * All functions are thread-safe (no shared state).
 *
 * ## Usage Example
 *
 * ```cpp
 * // Get all NTFS volumes
 * auto volumes = get_volume_path_names();
 * for (const auto& vol : volumes) {
 *     // Open volume and get cluster size
 *     Handle h = open_volume(vol);
 *     unsigned int cluster_size = get_cluster_size(h);
 *
 *     // Get MFT extent map
 *     long long mft_size;
 *     auto extents = get_retrieval_pointers(
 *         (vol + _T("$MFT::$DATA")).c_str(),
 *         &mft_size, 0, 0);
 * }
 * ```
 *
 * @see io/mft_reader.hpp - Uses these utilities for MFT reading
 */

#pragma once

#ifndef UFFS_VOLUME_UTILS_HPP
#define UFFS_VOLUME_UTILS_HPP

#include <Windows.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <algorithm>

#include "handle.hpp"
#include "error_utils.hpp"
#include "core_types.hpp"       // For std::tvstring
#include "io/winnt_types.hpp"   // For winnt:: types

namespace uffs {

/**
 * @brief Gets the cluster size for a volume
 *
 * @param volume Handle to the volume (opened with appropriate access)
 * @return Cluster size in bytes (typically 4096 for NTFS)
 * @throws CStructured_Exception on failure
 *
 * @details
 * Queries the volume using NtQueryVolumeInformationFile to get
 * the bytes per sector and sectors per allocation unit, then
 * multiplies them to get the cluster size.
 *
 * Common cluster sizes:
 * - 4 KB (4096 bytes) - default for volumes < 16 TB
 * - 8 KB, 16 KB, 32 KB, 64 KB - for larger volumes
 */
[[nodiscard]] inline unsigned int get_cluster_size(void* const volume)
{
    winnt::IO_STATUS_BLOCK iosb;
    winnt::FILE_FS_SIZE_INFORMATION info = {};

    if (unsigned long const error = winnt::RtlNtStatusToDosError(
            winnt::NtQueryVolumeInformationFile(volume, &iosb, &info, sizeof(info), 3)))
    {
        CppRaiseException(error);
    }

    return info.BytesPerSector * info.SectorsPerAllocationUnit;
}

// ============================================================================
// Volume Path Enumeration
// ============================================================================

/**
 * @brief Gets all logical drive path names on the system
 * @return Vector of drive paths (e.g., "C:\", "D:\")
 */
[[nodiscard]] inline std::vector<std::tvstring> get_volume_path_names()
{
    std::vector<std::tvstring> result;
    std::tvstring buf;
    size_t prev;
    do {
        prev = buf.size();
        buf.resize(std::max(
            static_cast<size_t>(GetLogicalDriveStrings(
                static_cast<unsigned long>(buf.size()),
                buf.empty() ? nullptr : &*buf.begin())),
            buf.size()));
    } while (prev < buf.size());
    
    for (size_t i = 0, n; n = std::char_traits<TCHAR>::length(&buf[i]),
         i < buf.size() && buf[i]; i += n + 1)
    {
        result.push_back(std::tvstring(&buf[i], n));
    }

    return result;
}

// ============================================================================
// Retrieval Pointers (for MFT extent mapping)
// ============================================================================

/**
 * @brief Gets file retrieval pointers (extent map) for a file
 * @param path Path to the file (e.g., "C:\\$MFT::$DATA")
 * @param size Output parameter for file size (can be nullptr)
 * @param mft_start_lcn MFT start LCN (unused, for compatibility)
 * @param file_record_size File record size (unused, for compatibility)
 * @return Vector of (VCN, LCN) pairs representing file extents
 * @throws CStructured_Exception on failure
 */
[[nodiscard]] inline std::vector<std::pair<unsigned long long, long long>> 
get_retrieval_pointers(
    TCHAR const path[], 
    long long* const size, 
    long long const mft_start_lcn, 
    unsigned int const file_record_size)
{
    (void)mft_start_lcn;
    (void)file_record_size;
    
    typedef std::vector<std::pair<unsigned long long, long long>> Result;
    Result result;
    Handle handle;
    
    if (path)
    {
        HANDLE const opened = CreateFile(
            path, 
            FILE_READ_ATTRIBUTES | SYNCHRONIZE, 
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
            nullptr, 
            OPEN_EXISTING, 
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_NO_BUFFERING, 
            nullptr);
        unsigned long const error = opened != INVALID_HANDLE_VALUE ? ERROR_SUCCESS : GetLastError();
        
        if (!error)
        {
            Handle(opened).swap(handle);
        }
        else if (error == ERROR_FILE_NOT_FOUND)
        {
            // do nothing
        }
        else if (error)
        {
            CppRaiseException(error);
        }
    }

    if (handle)
    {
        result.resize(1 + (sizeof(RETRIEVAL_POINTERS_BUFFER) - 1) / sizeof(Result::value_type));
        STARTING_VCN_INPUT_BUFFER input = {};

        BOOL success;
        for (unsigned long nr; 
             !(success = DeviceIoControl(
                 handle, 
                 FSCTL_GET_RETRIEVAL_POINTERS, 
                 &input, sizeof(input), 
                 &*result.begin(), 
                 static_cast<unsigned long>(result.size()) * sizeof(*result.begin()), 
                 &nr, nullptr), success) && GetLastError() == ERROR_MORE_DATA;)
        {
            size_t const n = result.size();
            Result(/*free old memory*/).swap(result);
            Result(n * 2).swap(result);
        }

        CheckAndThrow(success);
        if (size)
        {
            LARGE_INTEGER large_size;
            CheckAndThrow(GetFileSizeEx(handle, &large_size));
            *size = large_size.QuadPart;
        }

        result.erase(result.begin() + 1 + reinterpret_cast<unsigned long const&>(*result.begin()), result.end());
        result.erase(result.begin(), result.begin() + 1);
    }

    return result;
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::get_cluster_size;
using uffs::get_volume_path_names;
using uffs::get_retrieval_pointers;

#endif // UFFS_VOLUME_UTILS_HPP

