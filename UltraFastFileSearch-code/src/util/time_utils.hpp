// ============================================================================
// Time Utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#ifndef UFFS_TIME_UTILS_HPP
#define UFFS_TIME_UTILS_HPP

#include <Windows.h>

namespace uffs {

// ============================================================================
// Time Zone Utilities
// ============================================================================

/**
 * @brief Gets the current time zone bias in 100-nanosecond intervals
 * @return The difference between local time and UTC (positive = ahead of UTC)
 * 
 * This is used to convert NTFS timestamps (which are in UTC) to local time.
 * The bias is calculated as: local_time - utc_time
 */
[[nodiscard]] inline long long get_time_zone_bias()
{
    long long ft = 0;
    GetSystemTimeAsFileTime(&reinterpret_cast<FILETIME&>(ft));
    long long ft_local = 0;
    if (!FileTimeToLocalFileTime(
            &reinterpret_cast<FILETIME&>(ft), 
            &reinterpret_cast<FILETIME&>(ft_local)))
    {
        ft_local = 0;
    }

    return ft_local - ft;
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::get_time_zone_bias;

#endif // UFFS_TIME_UTILS_HPP

