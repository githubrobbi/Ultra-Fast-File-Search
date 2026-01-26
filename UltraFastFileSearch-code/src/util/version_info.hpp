// ============================================================================
// Version Information
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#ifndef UFFS_VERSION_INFO_HPP
#define UFFS_VERSION_INFO_HPP

#include <string>
#include <iostream>
#include <Windows.h>

namespace uffs {

// Package version string
inline const std::string& get_package_version()
{
    static const std::string version = "0.9.6";
    return version;
}

/**
 * @brief Print version information to stdout
 * 
 * Used by CLI11 for --version flag handling.
 */
inline void print_version()
{
    std::cout << "Ultra Fast File Search \t https://github.com/githubrobbi/Ultra-Fast-File-Search\n\n"
              << "based on SwiftSearch \t https://sourceforge.net/projects/swiftsearch/\n\n";
    std::cout << "\tUFFS version:\t" << get_package_version() << '\n';
    std::cout << "\tBuild for:\t" << "x86_64-pc-windows-msvc" << '\n';
    std::cout << "\n";
    std::cout << "\tOptimized build";
    std::cout << "\n\n";
}

/**
 * @brief Convert narrow string to wide string using Windows API
 * @param s The narrow (ANSI) string to convert
 * @return The wide string equivalent
 */
inline std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = static_cast<int>(s.length()) + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    std::wstring r(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
    return r;
}

} // namespace uffs

#endif // UFFS_VERSION_INFO_HPP

