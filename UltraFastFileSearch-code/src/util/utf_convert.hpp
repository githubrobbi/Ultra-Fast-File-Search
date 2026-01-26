// ============================================================================
// UTF Conversion Utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// 
// Provides UTF-8 <-> UTF-16 conversion utilities.
// ============================================================================

#pragma once

#ifndef UFFS_UTF_CONVERT_HPP
#define UFFS_UTF_CONVERT_HPP

#include <string>
#include <locale>
#include <codecvt>

namespace uffs {

/**
 * @brief Global UTF-8 <-> UTF-16 converter
 * 
 * Usage:
 * @code
 * std::string narrow = utf_converter.to_bytes(wide_utf16_source_string);
 * std::wstring wide = utf_converter.from_bytes(narrow_utf8_source_string);
 * @endcode
 * 
 * @note std::wstring_convert is deprecated in C++17 but still functional.
 *       Consider migrating to platform-specific APIs or a library like ICU
 *       for new code.
 */
#pragma warning(push)
#pragma warning(disable: 4996)  // Suppress deprecation warning for wstring_convert
inline std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>& get_utf_converter()
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter;
}
#pragma warning(pop)

// For backward compatibility - provides a reference to the global converter
// Note: This is a function-like macro to avoid static initialization order issues
#define utf_converter (::uffs::get_utf_converter())

} // namespace uffs

// Expose at global scope for backward compatibility
// The 'converter' name is used throughout the codebase
#define converter (::uffs::get_utf_converter())

#endif // UFFS_UTF_CONVERT_HPP

