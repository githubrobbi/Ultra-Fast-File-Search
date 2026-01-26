// ============================================================================
// Time Utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#ifndef UFFS_TIME_UTILS_HPP
#define UFFS_TIME_UTILS_HPP

#include <Windows.h>
#include "core_types.hpp"       // For std::tvstring
#include "../io/winnt_types.hpp" // For winnt::TIME_FIELDS, winnt::RtlTimeToTimeFields
#include "nformat.hpp"          // For basic_conv

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

// ============================================================================
// Time Formatting
// ============================================================================

/**
 * @brief Converts a system time (UTC) to a formatted string
 * @param system_time The time in 100-nanosecond intervals since 1601 (UTC)
 * @param buffer Output buffer for the formatted string
 * @param sortable If true, use ISO-like sortable format (YYYY-MM-DD HH:MM:SS)
 * @param include_time If true, include time component
 * @param time_zone_bias Time zone bias from get_time_zone_bias()
 * @param lcid Locale ID for non-sortable format
 */
inline void SystemTimeToString(long long system_time /*UTC */, std::tvstring& buffer, bool
    const sortable, bool
    const include_time, long long
    const time_zone_bias, LCID
    const lcid)
{
    long long local_time = system_time + time_zone_bias;
    winnt::TIME_FIELDS tf = {};
    winnt::RtlTimeToTimeFields(&reinterpret_cast<LARGE_INTEGER&>(local_time), &tf);
    if (sortable)
    {
        TCHAR buf[64], * p = buf;
        size_t cch_zero;
        basic_conv<TCHAR>::to_string(0, p, 10);
        cch_zero = std::char_traits<TCHAR>::length(p);
        p += static_cast<ptrdiff_t>(cch_zero);
        size_t cch_year;
        basic_conv<TCHAR>::to_string(tf.Year, p, 10);
        cch_year = std::char_traits<TCHAR>::length(p);
        p += static_cast<ptrdiff_t>(cch_year);
        size_t cch_month;
        basic_conv<TCHAR>::to_string(tf.Month, p, 10);
        cch_month = std::char_traits<TCHAR>::length(p);
        p += static_cast<ptrdiff_t>(cch_month);
        size_t cch_day;
        basic_conv<TCHAR>::to_string(tf.Day, p, 10);
        cch_day = std::char_traits<TCHAR>::length(p);
        p += static_cast<ptrdiff_t>(cch_day);
        size_t cch_hour;
        basic_conv<TCHAR>::to_string(tf.Hour, p, 10);
        cch_hour = std::char_traits<TCHAR>::length(p);
        p += static_cast<ptrdiff_t>(cch_hour);
        size_t cch_minute;
        basic_conv<TCHAR>::to_string(tf.Minute, p, 10);
        cch_minute = std::char_traits<TCHAR>::length(p);
        p += static_cast<ptrdiff_t>(cch_minute);
        size_t cch_second;
        basic_conv<TCHAR>::to_string(tf.Second, p, 10);
        cch_second = std::char_traits<TCHAR>::length(p);
        p += static_cast<ptrdiff_t>(cch_second);
        TCHAR zero = buf[0];
        size_t i = cch_zero;
        {
            size_t const cch = cch_year;
            buffer.append(4 - cch, zero);
            buffer.append(&buf[i], cch);
            i += cch;
        }

        buffer.push_back(_T('-'));
        {
            size_t const cch = cch_month;
            buffer.append(2 - cch, zero);
            buffer.append(&buf[i], cch);
            i += cch;
        }

        buffer.push_back(_T('-'));
        {
            size_t const cch = cch_day;
            buffer.append(2 - cch, zero);
            buffer.append(&buf[i], cch);
            i += cch;
        }

        if (include_time)
        {
            buffer.push_back(_T(' '));
            {
                size_t const cch = cch_hour;
                buffer.append(2 - cch, zero);
                buffer.append(&buf[i], cch);
                i += cch;
            }

            buffer.push_back(_T(':'));
            {
                size_t const cch = cch_minute;
                buffer.append(2 - cch, zero);
                buffer.append(&buf[i], cch);
                i += cch;
            }

            buffer.push_back(_T(':'));
            {
                size_t const cch = cch_second;
                buffer.append(2 - cch, zero);
                buffer.append(&buf[i], cch);
                i += cch;
            }
        }
    }
    else
    {
        SYSTEMTIME sysTime = { static_cast<WORD>(tf.Year),
            static_cast<WORD>(tf.Month),
            static_cast<WORD>(tf.Weekday),
            static_cast<WORD>(tf.Day),
            static_cast<WORD>(tf.Hour),
            static_cast<WORD>(tf.Minute),
            static_cast<WORD>(tf.Second),
            static_cast<WORD>(tf.Milliseconds)
        };

        TCHAR buf[64];
        size_t const buffer_size = sizeof(buf) / sizeof(*buf);
        size_t cch = 0;
        size_t const cchDate = static_cast<size_t>(GetDateFormat(lcid, 0, &sysTime, nullptr, &buf[0], static_cast<int>(buffer_size)));
        cch += cchDate - !!cchDate /*null terminator */;
        if (cchDate > 0 && include_time)
        {
            buf[cch++] = _T(' ');
            size_t const cchTime = static_cast<size_t>(GetTimeFormat(lcid, 0, &sysTime, nullptr, &buf[cchDate], static_cast<int>(buffer_size - cchDate)));
            cch += cchTime - !!cchTime;
        }

        buffer.append(buf, cch);
    }
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::get_time_zone_bias;
using uffs::SystemTimeToString;

#endif // UFFS_TIME_UTILS_HPP

