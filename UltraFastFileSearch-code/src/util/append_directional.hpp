// ============================================================================
// append_directional.hpp - Directional string append with ASCII compression
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for use by ntfs_index.hpp
// ============================================================================
#pragma once

#ifndef UFFS_APPEND_DIRECTIONAL_HPP
#define UFFS_APPEND_DIRECTIONAL_HPP

#include <algorithm>
#include <cstddef>
#include "core_types.hpp"
#include "allocators.hpp"

namespace uffs {

/**
 * @brief Append characters to a string with optional ASCII compression/decompression
 * @param str Target string to append to
 * @param sz Source characters
 * @param cch Number of characters
 * @param ascii_mode -1 = decompress ASCII, 0 = no change, +1 = compress ASCII
 * @param reverse If true, append in reverse order
 */
inline void append_directional(std::tvstring& str, TCHAR const sz[], size_t const cch,
    int const ascii_mode, bool const reverse = false)
{
    typedef std::tvstring Str;
    size_t const cch_in_str = ascii_mode > 0 ? (cch + 1) / 2 : cch;
    size_t const n = str.size();

#if defined(_MSC_VER) && !defined(_CPPLIB_VER)
    struct Derived : Str
    {
        using Str::_First;
        using Str::_Last;
        using Str::_End;
        using Str::allocator;
    };

    Derived& derived = static_cast<Derived&>(str);
#endif

    if (n + cch_in_str > str.capacity())
    {
#if defined(_MSC_VER) && !defined(_CPPLIB_VER)
        dynamic_allocator<TCHAR>& alloc = derived.allocator;
        size_t const min_capacity = str.capacity() + str.capacity() / 2;
        size_t new_capacity = n + cch_in_str;
        if (new_capacity < min_capacity)
        {
            new_capacity = min_capacity;
        }

        if (TCHAR* const p = alloc.reallocate(derived._First, new_capacity, true))
        {
            derived._First = p;
            derived._Last = derived._First + static_cast<ptrdiff_t>(n);
            derived._End = derived._First + static_cast<ptrdiff_t>(new_capacity);
        }
#endif
        str.reserve(n + n / 2 + cch_in_str * 2);
    }

#if defined(_MSC_VER) && !defined(_CPPLIB_VER)
    derived._Last += static_cast<ptrdiff_t>(cch_in_str);
#else
    str.resize(n + cch_in_str);
#endif

    if (cch)
    {
        TCHAR* const o = &str[n];
        if (reverse)
        {
            if (ascii_mode < 0)
            {
                std::reverse_copy(
                    static_cast<char const*>(static_cast<void const*>(sz)),
                    static_cast<char const*>(static_cast<void const*>(sz)) + static_cast<ptrdiff_t>(cch),
                    o);
            }
            else if (ascii_mode > 0)
            {
                for (size_t i = 0; i != cch; ++i)
                {
                    static_cast<char*>(static_cast<void*>(o))[i] = static_cast<char>(sz[cch - 1 - i]);
                }
            }
            else
            {
                std::reverse_copy(sz, sz + static_cast<ptrdiff_t>(cch), o);
            }
        }
        else
        {
            if (ascii_mode < 0)
            {
                std::copy(
                    static_cast<char const*>(static_cast<void const*>(sz)),
                    static_cast<char const*>(static_cast<void const*>(sz)) + static_cast<ptrdiff_t>(cch),
                    o);
            }
            else if (ascii_mode > 0)
            {
                for (size_t i = 0; i != cch; ++i)
                {
                    static_cast<char*>(static_cast<void*>(o))[i] = static_cast<char>(sz[i]);
                }
            }
            else
            {
                std::copy(sz, sz + static_cast<ptrdiff_t>(cch), o);
            }
        }
    }
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::append_directional;

#endif // UFFS_APPEND_DIRECTIONAL_HPP

