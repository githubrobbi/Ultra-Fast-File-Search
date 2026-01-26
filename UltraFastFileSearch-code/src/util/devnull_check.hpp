// ============================================================================
// Device Null Check Utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#ifndef UFFS_DEVNULL_CHECK_HPP
#define UFFS_DEVNULL_CHECK_HPP

#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>

#include "src/io/winnt_types.hpp"

namespace uffs {

/**
 * @brief Check if a file descriptor points to the null device (NUL)
 * @param fd File descriptor to check
 * @return true if the file descriptor is the null device
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

