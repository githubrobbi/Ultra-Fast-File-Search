// ============================================================================
// Volume Utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

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

namespace uffs {

// ============================================================================
// Type Definitions
// ============================================================================

// TCHAR-based string types (matches project conventions)
#ifdef _UNICODE
using tstring = std::wstring;
#else
using tstring = std::string;
#endif

// ============================================================================
// Volume Path Enumeration
// ============================================================================

/**
 * @brief Gets all logical drive path names on the system
 * @return Vector of drive paths (e.g., "C:\", "D:\")
 */
[[nodiscard]] inline std::vector<tstring> get_volume_path_names()
{
    std::vector<tstring> result;
    tstring buf;
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
        result.push_back(tstring(&buf[i], n));
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
using uffs::get_volume_path_names;
using uffs::get_retrieval_pointers;

#endif // UFFS_VOLUME_UTILS_HPP

