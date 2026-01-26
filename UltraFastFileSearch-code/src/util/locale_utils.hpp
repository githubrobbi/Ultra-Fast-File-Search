// ============================================================================
// Locale Utilities - Locale and language handling functions
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#ifndef UFFS_LOCALE_UTILS_HPP
#define UFFS_LOCALE_UTILS_HPP

#include <Windows.h>
#include <tchar.h>
#include <atlbase.h>
#include <atlstr.h>

// Forward declaration for safe_stprintf (from error_utils.hpp)
template <size_t N>
int safe_stprintf(TCHAR(&s)[N], TCHAR const* const format, ...);

namespace uffs {

/**
 * @brief Convert LCID to locale name with XP compatibility
 * @param lcid The locale identifier
 * @param name Buffer to receive the locale name
 * @param name_length Size of the buffer
 * @return Length of the locale name, or 0 on failure
 * 
 * Uses LCIDToLocaleName on Vista+ or registry fallback on XP.
 */
inline int LCIDToLocaleName_XPCompatible(LCID lcid, LPTSTR name, int name_length)
{
    HMODULE hKernel32 = nullptr;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, 
                           reinterpret_cast<LPCTSTR>(&GetSystemInfo), &hKernel32))
    {
        hKernel32 = nullptr;
    }

    typedef int WINAPI LCIDToLocaleName_t(LCID Locale, LPTSTR lpName, int cchName, DWORD dwFlags);
    if (hKernel32)
    if (LCIDToLocaleName_t* const LCIDToLocaleName = reinterpret_cast<LCIDToLocaleName_t*>(
            GetProcAddress(hKernel32, _CRT_STRINGIZE(LCIDToLocaleName))))
    {
        name_length = (*LCIDToLocaleName)(lcid, name, name_length, 0);
        name_length -= !!name_length;
    }
    else
    {
        ATL::CRegKey key;
        if (key.Open(HKEY_CLASSES_ROOT, TEXT("MIME\\Database\\Rfc1766"), KEY_QUERY_VALUE) == 0)
        {
            TCHAR value_data[64 + MAX_PATH] = {};
            TCHAR value_name[16] = {};
            value_name[0] = _T('\0');
            safe_stprintf(value_name, _T("%04lX"), lcid);
            unsigned long value_data_length = sizeof(value_data) / sizeof(*value_data);
            LRESULT const result = key.QueryValue(value_data, value_name, &value_data_length);
            if (result == 0)
            {
                unsigned long i;
                for (i = 0; i != value_data_length; ++i)
                {
                    if (value_data[i] == _T(';'))
                    {
                        break;
                    }
                    if (name_length >= 0 && i < static_cast<unsigned long>(name_length))
                    {
                        TCHAR ch = value_data[static_cast<ptrdiff_t>(i)];
                        name[static_cast<ptrdiff_t>(i)] = ch;
                    }
                }
                name_length = static_cast<int>(i);
            }
            else
            {
                name_length = 0;
            }
        }
        else
        {
            name_length = 0;
        }
    }

    return name_length;
}

/**
 * @brief Convert LCID to locale name as CString
 * @param lcid The locale identifier
 * @return The locale name as a CString
 */
inline WTL::CString LCIDToLocaleName_XPCompatible(LCID lcid)
{
    WTL::CString result;
    LPTSTR const buf = result.GetBufferSetLength(64);
    int const n = LCIDToLocaleName_XPCompatible(lcid, buf, result.GetLength());
    result.Delete(n, result.GetLength() - n);
    return result;
}

/**
 * @brief Get the current UI locale name
 * @return The UI locale name as a CString
 */
inline WTL::CString get_ui_locale_name()
{
    return LCIDToLocaleName_XPCompatible(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT));
}

} // namespace uffs

#endif // UFFS_LOCALE_UTILS_HPP

