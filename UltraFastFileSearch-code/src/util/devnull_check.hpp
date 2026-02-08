/**
 * @file devnull_check.hpp
 * @brief Utilities for detecting the Windows null device (NUL)
 *
 * @details
 * This file provides functions to check if a file descriptor or FILE*
 * points to the Windows null device (NUL), equivalent to /dev/null on Unix.
 *
 * ## Why Check for NUL?
 *
 * When output is redirected to NUL, we can skip expensive formatting
 * and I/O operations entirely, improving performance significantly.
 *
 * ## How It Works
 *
 * Uses NtQueryVolumeInformationFile to query the device type:
 * - DeviceType 0x00000015 = FILE_DEVICE_NULL
 *
 * ## Usage Example
 *
 * ```cpp
 * if (isdevnull(stdout)) {
 *     // Skip expensive output formatting
 *     return;
 * }
 *
 * // Normal output path
 * fprintf(stdout, "Results: %d files found\n", count);
 * ```
 *
 * @see io/winnt_types.hpp - Windows NT native API declarations
 */

#pragma once

#ifndef UFFS_DEVNULL_CHECK_HPP
#define UFFS_DEVNULL_CHECK_HPP

#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>

#include "io/winnt_types.hpp"

namespace uffs {

/**
 * @brief Check if a file descriptor points to the null device (NUL)
 *
 * @param fd File descriptor to check (from _fileno() or open())
 * @return true if the file descriptor is the null device
 *
 * @details
 * Queries the device type using NtQueryVolumeInformationFile.
 * FILE_DEVICE_NULL (0x15) indicates the null device.
 *
 * @note This is a low-overhead check suitable for hot paths.
 */
[[nodiscard]] inline bool isdevnull(int fd)
{
    winnt::IO_STATUS_BLOCK iosb;
    winnt::FILE_FS_DEVICE_INFORMATION fsinfo = {};
    return winnt::NtQueryVolumeInformationFile(
        (HANDLE)_get_osfhandle(fd), 
        &iosb, 
        &fsinfo, 
        sizeof(fsinfo), 
        4) == 0 && fsinfo.DeviceType == 0x00000015;
}

/**
 * @brief Check if a FILE* points to the null device (NUL)
 * @param f FILE pointer to check
 * @return true if the file is the null device
 */
[[nodiscard]] inline bool isdevnull(FILE* f)
{
    return isdevnull(
#ifdef _O_BINARY 
        _fileno(f)
#else
        fileno(f)
#endif
    );
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::isdevnull;

#endif // UFFS_DEVNULL_CHECK_HPP

