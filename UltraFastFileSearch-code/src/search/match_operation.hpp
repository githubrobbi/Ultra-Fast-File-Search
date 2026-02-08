/**
 * @file match_operation.hpp
 * @brief Pattern matching operation for file search with glob and regex support
 *
 * @details
 * This file provides the MatchOperation class which handles pattern matching
 * for file search operations. It supports multiple pattern types and includes
 * optimizations for path-based matching.
 *
 * ## Pattern Types
 *
 * | Prefix | Type     | Example           | Description                    |
 * |--------|----------|-------------------|--------------------------------|
 * | (none) | Glob     | `*.txt`           | Standard wildcard matching     |
 * | `>`    | Regex    | `>.*\.txt$`       | Full regular expression        |
 * | `**`   | Globstar | `src/**/*.cpp`    | Recursive directory matching   |
 *
 * ## Pattern Matching Flow
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    Pattern Processing                           │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Input Pattern                                                  │
 * │       │                                                         │
 * │       v                                                         │
 * │  ┌─────────────────┐                                           │
 * │  │ Check for '>'   │──Yes──> is_regex = true                   │
 * │  │ prefix          │                                           │
 * │  └────────┬────────┘                                           │
 * │           │ No                                                  │
 * │           v                                                     │
 * │  ┌─────────────────┐                                           │
 * │  │ Check for '\\'  │──Yes──> is_path_pattern = true            │
 * │  │ or '**'         │                                           │
 * │  └────────┬────────┘                                           │
 * │           │ No                                                  │
 * │           v                                                     │
 * │  ┌─────────────────┐                                           │
 * │  │ Check for ':'   │──Yes──> is_stream_pattern = true          │
 * │  │ (ADS)           │                                           │
 * │  └────────┬────────┘                                           │
 * │           │                                                     │
 * │           v                                                     │
 * │  ┌─────────────────┐                                           │
 * │  │ Root path       │──Yes──> Extract root for optimization     │
 * │  │ optimization?   │                                           │
 * │  └────────┬────────┘                                           │
 * │           │                                                     │
 * │           v                                                     │
 * │  ┌─────────────────┐                                           │
 * │  │ Create matcher  │                                           │
 * │  └─────────────────┘                                           │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Root Path Optimization
 *
 * When a pattern starts with a specific path (e.g., `C:\Users\*.txt`),
 * the root path (`C:\Users\`) is extracted and used to skip volumes
 * that don't match, improving search performance.
 *
 * ## Thread Safety
 *
 * - MatchOperation is NOT thread-safe for concurrent modification
 * - Once initialized via init(), it is safe for concurrent reads
 * - The underlying string_matcher is immutable after construction
 *
 * ## Usage Example
 *
 * ```cpp
 * MatchOperation op;
 * op.init(_T("*.txt"));  // Simple glob pattern
 *
 * // Check if a volume should be searched
 * if (op.prematch(root_path)) {
 *     std::tvstring current = op.get_current_path(root_path);
 *     // Search files in current path
 *     if (op.matcher.is_match(filename.data(), filename.size())) {
 *         // File matches pattern
 *     }
 * }
 * ```
 *
 * @see string_matcher - The underlying pattern matching engine
 */

#pragma once

#ifndef UFFS_MATCH_OPERATION_HPP
#define UFFS_MATCH_OPERATION_HPP

#include <tchar.h>
#include <string>
#include <algorithm>

#include "io/overlapped.hpp"      // For value_initialized
#include "util/core_types.hpp"    // For std::tstring, std::tvstring
#include "string_matcher.hpp"     // For string_matcher

namespace uffs {

/**
 * @struct MatchOperation
 * @brief Pattern matching operation for file search
 *
 * @details
 * Encapsulates a search pattern and provides methods for matching
 * file paths against it. Supports glob patterns, regex patterns,
 * and path-based matching with case-insensitive comparison.
 *
 * ## Member Variables
 *
 * | Member                    | Description                              |
 * |---------------------------|------------------------------------------|
 * | is_regex                  | True if pattern uses regex syntax        |
 * | is_path_pattern           | True if pattern includes path separators |
 * | is_stream_pattern         | True if pattern matches ADS (has ':')    |
 * | requires_root_path_match  | True if pattern starts with specific path|
 * | root_path_optimized_away  | Extracted root path for optimization     |
 * | matcher                   | The compiled pattern matcher             |
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

