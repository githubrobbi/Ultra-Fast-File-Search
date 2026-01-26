// ============================================================================
// MatchOperation - Pattern matching for file search
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// ============================================================================

#pragma once

#ifndef UFFS_MATCH_OPERATION_HPP
#define UFFS_MATCH_OPERATION_HPP

#include <tchar.h>
#include <string>
#include <algorithm>

#include "../io/overlapped.hpp"      // For value_initialized
#include "../util/core_types.hpp"    // For std::tstring, std::tvstring
#include "string_matcher.hpp"        // For string_matcher

namespace uffs {

/**
 * @brief Pattern matching operation for file search
 * 
 * Handles glob patterns, regex patterns, and path-based matching.
 * Supports case-insensitive matching and path optimization.
 */
struct MatchOperation
{
    value_initialized<bool>
        is_regex,
        is_path_pattern,
        is_stream_pattern,
        requires_root_path_match;

    std::tvstring root_path_optimized_away;

    string_matcher matcher;

    MatchOperation() {}

    void init(std::tstring pattern)
    {
        is_regex = !pattern.empty() &&
            *pattern.begin() == _T('>');

        if (is_regex)
        {
            pattern.erase(pattern.begin());
        }

        is_path_pattern = is_regex ||
            ~pattern.find(_T('\\')) ||
            ~pattern.find(_T("**"));

        is_stream_pattern =
            is_regex ||
            ~pattern.find(_T(':'));

        requires_root_path_match =
            is_path_pattern &&
            !is_regex &&
            pattern.size() >= 2 &&
            *(pattern.begin() + 0) != _T('*') &&
            *(pattern.begin() + 0) != _T('?') &&
            *(pattern.begin() + 1) != _T('*') &&
            *(pattern.begin() + 1) != _T('?');

        if (requires_root_path_match)
        {
            root_path_optimized_away.insert(root_path_optimized_away.end(),
                pattern.begin(),
                std::find(pattern.begin(),
                    pattern.end(),
                    _T('\\')
                )
            );
            pattern.erase(pattern.begin(),
                pattern.begin() + static_cast<ptrdiff_t>(root_path_optimized_away.size())
            );
        }

        if (!is_path_pattern && !~pattern.find(_T('*')) && !~pattern.find(_T('?')))
        {
            pattern.insert(pattern.begin(), _T('*'));
            pattern.insert(pattern.begin(), _T('*'));
            pattern.insert(pattern.end(), _T('*'));
            pattern.insert(pattern.end(), _T('*'));
        }

        string_matcher(is_regex ?
            string_matcher::pattern_regex :
            is_path_pattern ?
            string_matcher::pattern_globstar :
            string_matcher::pattern_glob,
            string_matcher::pattern_option_case_insensitive,
            pattern.data(), pattern.size()).swap(matcher);
    }

    bool prematch(std::tvstring const& root_path) const
    {
        return !requires_root_path_match || (root_path.size() >= root_path_optimized_away.size() &&
            std::equal(root_path.begin(),
                root_path.begin() + static_cast<ptrdiff_t>(root_path_optimized_away.size()),
                root_path_optimized_away.begin()
            )
        );
    }

    std::tvstring get_current_path(std::tvstring const& root_path) const
    {
        std::tvstring current_path =
            root_path_optimized_away.empty() ? root_path : std::tvstring();

        while (!current_path.empty() &&
            *(current_path.end() - 1) == _T('\\')
        )
        {
            current_path.erase(current_path.end() - 1);
        }

        return current_path;
    }
};

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::MatchOperation;

#endif // UFFS_MATCH_OPERATION_HPP

