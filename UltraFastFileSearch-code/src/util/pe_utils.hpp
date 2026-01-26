// ============================================================================
// PE Utilities - Portable Executable inspection functions
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#ifndef UFFS_PE_UTILS_HPP
#define UFFS_PE_UTILS_HPP

#include <Windows.h>

namespace uffs {

/**
 * @brief Get the subsystem type from a PE image
 * @param image_base Pointer to the DOS header of the image
 * @return The subsystem value from the optional header (e.g., IMAGE_SUBSYSTEM_WINDOWS_GUI)
 */
inline unsigned short get_subsystem(IMAGE_DOS_HEADER const* const image_base)
{
    return reinterpret_cast<IMAGE_NT_HEADERS const*>(
        reinterpret_cast<unsigned char const*>(image_base) + image_base->e_lfanew
    )->OptionalHeader.Subsystem;
}

/**
 * @brief Get the version timestamp from a PE image as a FILETIME value
 * @param image_base Pointer to the DOS header of the image
 * @return The TimeDateStamp converted to FILETIME format (100-nanosecond intervals since 1601)
 */
inline unsigned long long get_version(IMAGE_DOS_HEADER const* const image_base)
{
    return reinterpret_cast<IMAGE_NT_HEADERS const*>(
        reinterpret_cast<unsigned char const*>(image_base) + image_base->e_lfanew
    )->FileHeader.TimeDateStamp * 10000000ULL + 0x019db1ded53e8000ULL;
}

} // namespace uffs

#endif // UFFS_PE_UTILS_HPP

