# Pattern Matching Engine

## Introduction

This document provides exhaustive detail on how Ultra Fast File Search implements pattern matching. After reading this document, you should be able to:

1. Implement all pattern types (verbatim, glob, globstar, regex)
2. Understand the pattern compilation and optimization pipeline
3. Build the Boyer-Moore-Horspool substring search
4. Implement case-insensitive matching with custom iterators
5. Use high-water mark tracking for early termination

---

## Overview: Pattern Matching Pipeline

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Pattern Matching Pipeline                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  User Input                                                             │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ "*.cpp"  or  ">.*\.h$"  or  "C:\Users\**\*.txt"                  │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 1: Pattern Type Detection (MatchOperation::init)                  │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ - Regex prefix ">" → pattern_regex                               │  │
│  │ - Contains "\\" or "**" → pattern_globstar (path pattern)        │  │
│  │ - Contains "*" or "?" → pattern_glob                             │  │
│  │ - No wildcards → auto-wrap with "**" for substring match         │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 2: Root Path Optimization                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ "C:\Users\*.txt" → root="C:\Users\", pattern="*.txt"             │  │
│  │ Skip drives that don't match root prefix                         │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 3: Pattern Compilation (string_matcher::init)                     │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ - Strip leading/trailing wildcards → set unanchored flags        │  │
│  │ - Reduce glob → globstar if no "?" wildcards                     │  │
│  │ - Reduce globstar → verbatim if no middle wildcards              │  │
│  │ - Convert glob/globstar → regex                                  │  │
│  │ - Compile regex with Boost.Xpressive                             │  │
│  │ - Build Boyer-Moore-Horspool tables for verbatim                 │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│                              ▼                                          │
│  Step 4: Matching (string_matcher::is_match)                            │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ - pattern_anything: return true                                  │  │
│  │ - pattern_verbatim: Boyer-Moore-Horspool search                  │  │
│  │ - pattern_regex: regex_match or regex_search                     │  │
│  │ - Track high-water mark for early termination                    │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Pattern Types

### Enumeration

```cpp
enum pattern_kind {
    pattern_anything,   // Matches everything (empty or all-wildcard pattern)
    pattern_verbatim,   // Exact substring/prefix/suffix/full match
    pattern_glob,       // Shell-style wildcards (* and ?)
    pattern_globstar,   // Extended glob with ** for directory traversal
    pattern_regex       // Full regular expression (prefix with ">")
};

enum pattern_options {
    pattern_option_none = 0,
    pattern_option_case_insensitive = 1 << 0
};
```

### Pattern Type Semantics

| Type | Syntax | Description | Example |
|------|--------|-------------|---------|
| `pattern_anything` | (empty) | Matches any string | "" matches "anything" |
| `pattern_verbatim` | literal | Exact substring match | "readme" matches "README.txt" |
| `pattern_glob` | `*`, `?` | Shell wildcards, `*` stops at `\` | `*.txt` matches `file.txt` |
| `pattern_globstar` | `**` | Extended glob, `**` crosses `\` | `**\*.cpp` matches `src\main.cpp` |
| `pattern_regex` | `>pattern` | Full regex (prefix with `>`) | `>.*\.h$` matches `header.h` |

### Wildcard Semantics

| Wildcard | In `pattern_glob` | In `pattern_globstar` |
|----------|-------------------|----------------------|
| `*` | Matches any characters **except** `\` and `/` | Matches any characters **except** `\` and `/` |
| `**` | Same as `*` (no special meaning) | Matches any characters **including** `\` and `/` |
| `?` | Matches any single character **except** `\` and `/` | Matches any single character **except** `\` and `/` |

---

## MatchOperation: Pattern Preprocessing

### Structure

```cpp
struct MatchOperation {
    value_initialized<bool>
        is_regex,              // Pattern starts with ">"
        is_path_pattern,       // Contains "\\" or "**"
        is_stream_pattern,     // Contains ":" (NTFS ADS)
        requires_root_path_match;  // Has non-wildcard prefix

    std::tvstring root_path_optimized_away;  // Extracted root path
    string_matcher matcher;                   // Compiled pattern

    void init(std::tstring pattern);
    bool prematch(std::tvstring const& root_path) const;
};
```

### Pattern Detection Algorithm

```cpp
void MatchOperation::init(std::tstring pattern) {
    // Step 1: Detect regex (prefix with ">")
    is_regex = !pattern.empty() && *pattern.begin() == _T('>');
    if (is_regex) {
        pattern.erase(pattern.begin());  // Remove ">" prefix
    }

    // Step 2: Detect path pattern (contains "\\" or "**")
    is_path_pattern = is_regex ||
                      ~pattern.find(_T('\\')) ||
                      ~pattern.find(_T("**"));

    // Step 3: Detect stream pattern (contains ":")
    is_stream_pattern = is_regex || ~pattern.find(_T(':'));

    // Step 4: Check if root path optimization is possible
    // Pattern must start with non-wildcard characters
    requires_root_path_match = is_path_pattern &&
                               !is_regex &&
                               pattern.size() >= 2 &&
                               *(pattern.begin() + 0) != _T('*') &&


### Root Path Optimization

The root path optimization allows skipping entire drives that don't match the pattern prefix:

```cpp
bool MatchOperation::prematch(std::tvstring const& root_path) const {
    // If no root path requirement, match all drives
    if (!requires_root_path_match) {
        return true;
    }

    // Check if drive's root path starts with the optimized-away prefix
    // Case-insensitive comparison
    return root_path.size() >= root_path_optimized_away.size() &&
           std::equal(
               root_path_optimized_away.begin(),
               root_path_optimized_away.end(),
               root_path.begin(),
               [](TCHAR a, TCHAR b) {
                   return std::tolower(a) == std::tolower(b);
               }
           );
}
```

**Example:**
- Pattern: `C:\Users\**\*.txt`
- Extracted root: `C:\Users\`
- Drives D:, E:, F: are skipped entirely
- Only C: drive is searched

---

## string_matcher: Pattern Compilation

### Structure

```cpp
struct string_matcher {
    pattern_kind kind;
    pattern_options options;

    // Anchoring flags (set when leading/trailing wildcards are stripped)
    bool unanchored_begin;  // Pattern doesn't need to match at start
    bool unanchored_end;    // Pattern doesn't need to match at end

    // For pattern_verbatim: Boyer-Moore-Horspool search
    std::tvstring needle;   // The literal string to search for

    // For pattern_regex: Compiled regex
    boost::xpressive::basic_regex<
        case_insensitive_iterator<TCHAR const*>
    > regex_ci;
    boost::xpressive::basic_regex<TCHAR const*> regex_cs;

    void init(pattern_kind kind, pattern_options options,
              TCHAR const* pattern, size_t pattern_length);
    bool is_match(TCHAR const* begin, TCHAR const* end,
                  size_t* high_water_mark = nullptr) const;
};
```

### Pattern Compilation Algorithm

```cpp
void string_matcher::init(pattern_kind kind, pattern_options options,
                          TCHAR const* pattern, size_t pattern_length) {
    this->kind = kind;
    this->options = options;
    this->unanchored_begin = false;
    this->unanchored_end = false;

    // Handle empty pattern
    if (pattern_length == 0) {
        this->kind = pattern_anything;
        return;
    }

    // Step 1: Strip leading wildcards
    // "**foo" → "foo" with unanchored_begin=true
    while (pattern_length > 0 && *pattern == _T('*')) {
        unanchored_begin = true;
        ++pattern;
        --pattern_length;
    }

    // Step 2: Strip trailing wildcards
    // "foo**" → "foo" with unanchored_end=true
    while (pattern_length > 0 && pattern[pattern_length - 1] == _T('*')) {
        unanchored_end = true;
        --pattern_length;
    }

    // Step 3: Check if pattern is now empty (was all wildcards)
    if (pattern_length == 0) {
        this->kind = pattern_anything;
        return;
    }

    // Step 4: Optimize glob → globstar
    // If glob pattern has no "?" wildcards, treat as globstar
    // (simpler regex, same semantics for * without ?)
    if (kind == pattern_glob) {
        bool has_question = false;
        for (size_t i = 0; i < pattern_length; ++i) {
            if (pattern[i] == _T('?')) {
                has_question = true;
                break;
            }
        }
        if (!has_question) {
            kind = pattern_globstar;
        }
    }

    // Step 5: Optimize globstar → verbatim
    // If no wildcards remain in middle, use fast substring search
    if (kind == pattern_globstar) {
        bool has_wildcards = false;
        for (size_t i = 0; i < pattern_length; ++i) {
            if (pattern[i] == _T('*') || pattern[i] == _T('?')) {
                has_wildcards = true;
                break;
            }
        }
        if (!has_wildcards) {
            kind = pattern_verbatim;
        }
    }

    this->kind = kind;

    // Step 6: Compile based on final pattern type
    switch (kind) {
        case pattern_verbatim:
            // Store needle for Boyer-Moore-Horspool search
            needle.assign(pattern, pattern_length);
            break;

        case pattern_glob:
        case pattern_globstar:
            // Convert to regex and compile
            compile_glob_to_regex(pattern, pattern_length, kind == pattern_globstar);
            break;

        case pattern_regex:
            // Compile regex directly
            compile_regex(pattern, pattern_length);
            break;
    }
}
```

---

## Glob to Regex Conversion

### Conversion Rules

| Glob | Regex (glob mode) | Regex (globstar mode) |
|------|-------------------|----------------------|
| `*` | `[^\\/]*` | `[^\\/]*` |
| `**` | `[^\\/]*[^\\/]*` | `.*` |
| `\**\` | N/A | `(?:[^\\/]+\\)*` |
| `?` | `[^\\/]` | `[^\\/]` |
| `.` | `\.` (escaped) | `\.` (escaped) |
| `[` | `\[` (escaped) | `\[` (escaped) |
| Other special | Escaped | Escaped |

### Implementation

```cpp
void string_matcher::compile_glob_to_regex(
    TCHAR const* pattern, size_t length, bool is_globstar) {

    std::tstring regex_pattern;

    // Add start anchor if not unanchored
    if (!unanchored_begin) {
        regex_pattern += _T('^');
    }

    for (size_t i = 0; i < length; ++i) {
        TCHAR c = pattern[i];

        switch (c) {
            case _T('*'):
                if (is_globstar && i + 1 < length && pattern[i + 1] == _T('*')) {
                    // Handle "**" in globstar mode
                    ++i;  // Skip second *

                    // Check for "\**\" pattern (directory traversal)
                    if (i >= 2 && pattern[i - 2] == _T('\\') &&
                        i + 1 < length && pattern[i + 1] == _T('\\')) {
                        // "\**\" → "(?:[^\\/]+\\)*"
                        regex_pattern += _T("(?:[^\\\\/]+\\\\)*");
                        ++i;  // Skip trailing backslash
                    } else {
                        // "**" → ".*"
                        regex_pattern += _T(".*");
                    }
                } else {
                    // Single "*" → "[^\\/]*" (match non-separator chars)
                    regex_pattern += _T("[^\\\\/]*");
                }
                break;

            case _T('?'):
                // "?" → "[^\\/]" (match single non-separator char)
                regex_pattern += _T("[^\\\\/]");
                break;

            case _T('\\'):
                // Backslash: escape in regex
                regex_pattern += _T("\\\\");
                break;

            // Escape regex metacharacters
            case _T('.'):
            case _T('['):
            case _T(']'):
            case _T('('):
            case _T(')'):
            case _T('{'):
            case _T('}'):
            case _T('+'):
            case _T('^'):
            case _T('$'):
            case _T('|'):
                regex_pattern += _T('\\');
                regex_pattern += c;
                break;

            default:
                regex_pattern += c;
                break;
        }
    }

    // Add end anchor if not unanchored
    if (!unanchored_end) {
        regex_pattern += _T('$');
    }

    // Compile the regex
    compile_regex(regex_pattern.data(), regex_pattern.size());
}
```

---

## Regex Compilation with Boost.Xpressive

### Why Boost.Xpressive?

1. **Header-only**: No library linking required
2. **Dynamic compilation**: Patterns compiled at runtime
3. **Custom iterators**: Supports case-insensitive matching via iterator adapters
4. **Performance**: Optimized NFA/DFA hybrid engine

### Compilation

```cpp
void string_matcher::compile_regex(TCHAR const* pattern, size_t length) {
    std::tstring pattern_str(pattern, length);

    if (options & pattern_option_case_insensitive) {
        // Compile with case-insensitive iterator
        regex_ci = boost::xpressive::basic_regex<
            case_insensitive_iterator<TCHAR const*>
        >::compile(pattern_str);
    } else {
        // Compile with regular iterator
        regex_cs = boost::xpressive::basic_regex<TCHAR const*>::compile(pattern_str);
    }
}
```

---

## Case-Insensitive Matching

### Custom Iterator Approach

Rather than converting strings to lowercase before matching, UFFS uses a custom iterator that lowercases characters on-the-fly. This avoids memory allocation and string copying.

```cpp
template<typename BaseIterator>
struct case_insensitive_iterator {
    using value_type = typename std::iterator_traits<BaseIterator>::value_type;
    using difference_type = typename std::iterator_traits<BaseIterator>::difference_type;
    using pointer = typename std::iterator_traits<BaseIterator>::pointer;
    using reference = value_type;  // Returns by value, not reference
    using iterator_category = std::random_access_iterator_tag;

    BaseIterator base;

    case_insensitive_iterator() = default;
    case_insensitive_iterator(BaseIterator it) : base(it) {}

    // Dereference returns lowercase character
    value_type operator*() const {
        return static_cast<value_type>(std::tolower(*base));
    }

    // All iterator operations delegate to base
    case_insensitive_iterator& operator++() { ++base; return *this; }
    case_insensitive_iterator operator++(int) { auto tmp = *this; ++base; return tmp; }
    case_insensitive_iterator& operator--() { --base; return *this; }
    case_insensitive_iterator operator--(int) { auto tmp = *this; --base; return tmp; }

    case_insensitive_iterator& operator+=(difference_type n) { base += n; return *this; }
    case_insensitive_iterator& operator-=(difference_type n) { base -= n; return *this; }

    case_insensitive_iterator operator+(difference_type n) const {
        return case_insensitive_iterator(base + n);
    }
    case_insensitive_iterator operator-(difference_type n) const {
        return case_insensitive_iterator(base - n);
    }

    difference_type operator-(case_insensitive_iterator const& other) const {
        return base - other.base;
    }

    value_type operator[](difference_type n) const {
        return static_cast<value_type>(std::tolower(base[n]));
    }

    bool operator==(case_insensitive_iterator const& other) const {
        return base == other.base;
    }
    bool operator!=(case_insensitive_iterator const& other) const {
        return base != other.base;
    }
    bool operator<(case_insensitive_iterator const& other) const {
        return base < other.base;
    }
    // ... other comparison operators
};
```

### Usage in Matching

```cpp
bool string_matcher::is_match(TCHAR const* begin, TCHAR const* end,
                              size_t* high_water_mark) const {
    if (options & pattern_option_case_insensitive) {
        // Wrap iterators for case-insensitive matching
        case_insensitive_iterator<TCHAR const*> ci_begin(begin);
        case_insensitive_iterator<TCHAR const*> ci_end(end);

        return match_impl(ci_begin, ci_end, regex_ci, high_water_mark);
    } else {
        return match_impl(begin, end, regex_cs, high_water_mark);
    }
}
```

---

## Boyer-Moore-Horspool Substring Search

For `pattern_verbatim` patterns, UFFS uses the Boyer-Moore-Horspool algorithm from Boost.Algorithm for fast substring search.

### Algorithm Overview

Boyer-Moore-Horspool is a simplified version of Boyer-Moore that uses only the bad character rule:

1. **Preprocessing**: Build a skip table mapping each character to how far to shift
2. **Searching**: Compare pattern from right to left, skip based on mismatched character

### Implementation

```cpp
#include <boost/algorithm/searching/boyer_moore_horspool.hpp>

bool string_matcher::verbatim_match(TCHAR const* begin, TCHAR const* end) const {
    // Handle anchoring
    if (!unanchored_begin && !unanchored_end) {
        // Full match: string must equal needle exactly
        size_t len = end - begin;
        if (len != needle.size()) return false;

        if (options & pattern_option_case_insensitive) {
            return std::equal(begin, end, needle.begin(),
                [](TCHAR a, TCHAR b) {
                    return std::tolower(a) == std::tolower(b);
                });
        } else {
            return std::equal(begin, end, needle.begin());
        }
    }

    if (!unanchored_begin) {
        // Prefix match: string must start with needle
        if (static_cast<size_t>(end - begin) < needle.size()) return false;

        if (options & pattern_option_case_insensitive) {
            return std::equal(needle.begin(), needle.end(), begin,
                [](TCHAR a, TCHAR b) {
                    return std::tolower(a) == std::tolower(b);
                });
        } else {
            return std::equal(needle.begin(), needle.end(), begin);
        }
    }

    if (!unanchored_end) {
        // Suffix match: string must end with needle
        if (static_cast<size_t>(end - begin) < needle.size()) return false;

        TCHAR const* suffix_start = end - needle.size();
        if (options & pattern_option_case_insensitive) {
            return std::equal(needle.begin(), needle.end(), suffix_start,
                [](TCHAR a, TCHAR b) {
                    return std::tolower(a) == std::tolower(b);
                });
        } else {
            return std::equal(needle.begin(), needle.end(), suffix_start);
        }
    }

    // Substring match: use Boyer-Moore-Horspool
    if (options & pattern_option_case_insensitive) {
        // Case-insensitive search with custom iterator
        case_insensitive_iterator<TCHAR const*> ci_begin(begin);
        case_insensitive_iterator<TCHAR const*> ci_end(end);
        case_insensitive_iterator<TCHAR const*> needle_begin(needle.data());
        case_insensitive_iterator<TCHAR const*> needle_end(needle.data() + needle.size());

        auto searcher = boost::algorithm::make_boyer_moore_horspool(
            needle_begin, needle_end);
        return searcher(ci_begin, ci_end).first != ci_end;
    } else {
        auto searcher = boost::algorithm::make_boyer_moore_horspool(
            needle.data(), needle.data() + needle.size());
        return searcher(begin, end).first != end;
    }
}
```

### Performance Characteristics

| Pattern Length | Average Case | Worst Case |
|----------------|--------------|------------|
| m characters | O(n/m) | O(n*m) |

For typical file names (short patterns), Boyer-Moore-Horspool provides excellent performance with minimal preprocessing overhead.



---

## High-Water Mark Tracking

### Purpose

When matching file paths, UFFS tracks the "high-water mark" - the furthest position in the string that was examined during matching. This enables early termination optimization.

### Tracking Iterator

```cpp
template<typename BaseIterator>
struct tracking_iterator {
    using value_type = typename std::iterator_traits<BaseIterator>::value_type;
    using difference_type = typename std::iterator_traits<BaseIterator>::difference_type;
    using pointer = typename std::iterator_traits<BaseIterator>::pointer;
    using reference = typename std::iterator_traits<BaseIterator>::reference;
    using iterator_category = std::random_access_iterator_tag;

    BaseIterator base;
    BaseIterator* high_water_mark;  // Pointer to track maximum position

    tracking_iterator() = default;
    tracking_iterator(BaseIterator it, BaseIterator* hwm)
        : base(it), high_water_mark(hwm) {}

    // Dereference updates high-water mark
    reference operator*() const {
        if (high_water_mark && base > *high_water_mark) {
            *high_water_mark = base;
        }
        return *base;
    }

    // Index access also updates high-water mark
    reference operator[](difference_type n) const {
        BaseIterator pos = base + n;
        if (high_water_mark && pos > *high_water_mark) {
            *high_water_mark = pos;
        }
        return *pos;
    }

    // All other operations delegate to base
    tracking_iterator& operator++() { ++base; return *this; }
    tracking_iterator operator++(int) { auto tmp = *this; ++base; return tmp; }
    // ... other iterator operations
};
```

### Usage for Early Termination

```cpp
bool string_matcher::is_match(TCHAR const* begin, TCHAR const* end,
                              size_t* high_water_mark) const {
    if (high_water_mark) {
        // Use tracking iterator to record furthest position examined
        TCHAR const* hwm = begin;
        tracking_iterator<TCHAR const*> track_begin(begin, &hwm);
        tracking_iterator<TCHAR const*> track_end(end, nullptr);

        bool result = match_impl(track_begin, track_end);

        // Return high-water mark as offset from begin
        *high_water_mark = hwm - begin;
        return result;
    } else {
        return match_impl(begin, end);
    }
}
```

### Optimization Benefit

When searching through MFT records, if a pattern like `*.cpp` doesn't match a file, the high-water mark tells us how much of the path was examined. If the pattern only looked at the extension, we know the directory portion wasn't relevant, enabling smarter caching and early termination in directory traversal.

---

## Complete Matching Algorithm

### Main Entry Point

```cpp
bool string_matcher::is_match(TCHAR const* begin, TCHAR const* end,
                              size_t* high_water_mark) const {
    switch (kind) {
        case pattern_anything:
            // Empty pattern matches everything
            if (high_water_mark) *high_water_mark = 0;
            return true;

        case pattern_verbatim:
            return verbatim_match(begin, end, high_water_mark);

        case pattern_glob:
        case pattern_globstar:
        case pattern_regex:
            return regex_match(begin, end, high_water_mark);

        default:
            return false;
    }
}
```

### Regex Matching

```cpp
bool string_matcher::regex_match(TCHAR const* begin, TCHAR const* end,
                                 size_t* high_water_mark) const {
    using namespace boost::xpressive;

    if (options & pattern_option_case_insensitive) {
        case_insensitive_iterator<TCHAR const*> ci_begin(begin);
        case_insensitive_iterator<TCHAR const*> ci_end(end);

        if (unanchored_begin || unanchored_end) {
            // Use regex_search for unanchored patterns
            match_results<case_insensitive_iterator<TCHAR const*>> what;
            bool result = regex_search(ci_begin, ci_end, what, regex_ci);

            if (high_water_mark && !what.empty()) {
                *high_water_mark = what[0].second.base - begin;
            }
            return result;
        } else {
            // Use regex_match for fully anchored patterns
            return boost::xpressive::regex_match(ci_begin, ci_end, regex_ci);
        }
    } else {
        if (unanchored_begin || unanchored_end) {
            match_results<TCHAR const*> what;
            bool result = regex_search(begin, end, what, regex_cs);

            if (high_water_mark && !what.empty()) {
                *high_water_mark = what[0].second - begin;
            }
            return result;
        } else {
            return boost::xpressive::regex_match(begin, end, regex_cs);
        }
    }
}
```

---

## Pattern Examples

### Example 1: Simple Filename Search

```
Input: "readme"
Detection: No wildcards, no path separators
Auto-wrap: "**readme**"
Type: pattern_globstar
Regex: ".*readme.*"
Matches: "README.txt", "C:\docs\readme.md", "readme"
```

### Example 2: Extension Filter

```
Input: "*.cpp"
Detection: Has wildcard, no path separator
Type: pattern_glob
Optimization: No "?" → pattern_globstar
Regex: "^[^\\/]*\.cpp$"
Matches: "main.cpp", "test.cpp"
Does not match: "src\main.cpp" (has path separator)
```

### Example 3: Path Pattern

```
Input: "C:\Users\**\*.txt"
Detection: Has path separator and "**"
Type: pattern_globstar
Root extraction: "C:\Users\" optimized away
Remaining pattern: "**\*.txt"
Regex: ".*\\[^\\/]*\.txt$"
Matches: "C:\Users\docs\notes.txt", "C:\Users\a\b\c\file.txt"
```

### Example 4: Regex Pattern

```
Input: ">.*\.(h|hpp|hxx)$"
Detection: Starts with ">"
Type: pattern_regex
Regex: ".*\.(h|hpp|hxx)$" (compiled directly)
Matches: "header.h", "types.hpp", "config.hxx"
```

### Example 5: Directory Traversal

```
Input: "src\**\test\*.cpp"
Detection: Has path separator and "**"
Type: pattern_globstar
Regex: "^src\\(?:[^\\/]+\\)*test\\[^\\/]*\.cpp$"
Matches: "src\test\main.cpp", "src\a\b\test\unit.cpp"
Does not match: "src\test.cpp" (no "test" directory)
```

---

## Unicode Considerations

### Character Handling

UFFS uses `TCHAR` which maps to `wchar_t` in Unicode builds (the default on Windows). This provides:

1. **Full Unicode support**: All Unicode characters in file names are handled correctly
2. **Case folding**: Uses `std::towlower` for case-insensitive matching
3. **Surrogate pairs**: Handled correctly by Windows APIs

### Limitations

1. **Simple case folding**: Uses `towlower`, not full Unicode case folding
2. **No normalization**: NFC vs NFD forms are not normalized
3. **Locale-independent**: Uses C locale for case conversion

For most file system use cases, these limitations are acceptable since Windows file systems use simple case folding.

---

## Performance Optimizations Summary

| Optimization | Benefit |
|--------------|---------|
| Root path extraction | Skip entire drives that don't match |
| Wildcard stripping | Reduce pattern complexity |
| Glob → Globstar | Simpler regex when no `?` wildcards |
| Globstar → Verbatim | Use fast substring search when possible |
| Boyer-Moore-Horspool | O(n/m) average case for substring search |
| Case-insensitive iterator | No string copying for case folding |
| High-water mark tracking | Enable early termination |
| Compiled regex caching | Pattern compiled once, used many times |

---

## Testing

### Built-in Test Suite

The codebase includes a test suite in `string_matcher_tester`:

```cpp
struct string_matcher_tester {
    static void test() {
        // Glob tests
        test_glob(_T("*.txt"), _T("file.txt"), true);
        test_glob(_T("*.txt"), _T("file.cpp"), false);
        test_glob(_T("file?.txt"), _T("file1.txt"), true);
        test_glob(_T("file?.txt"), _T("file12.txt"), false);

        // Globstar tests
        test_globstar(_T("**\\*.cpp"), _T("src\\main.cpp"), true);
        test_globstar(_T("**\\*.cpp"), _T("main.cpp"), true);
        test_globstar(_T("src\\**\\*.cpp"), _T("src\\a\\b\\test.cpp"), true);

        // Regex tests
        test_regex(_T(".*\\.h$"), _T("header.h"), true);
        test_regex(_T(".*\\.h$"), _T("header.hpp"), false);

        // Case sensitivity tests
        test_case_insensitive(_T("README"), _T("readme"), true);
        test_case_insensitive(_T("README"), _T("ReadMe"), true);
    }
};
```

---

## Implementation Checklist

To implement the pattern matching engine:

- [ ] Define `pattern_kind` and `pattern_options` enums
- [ ] Implement `case_insensitive_iterator` template
- [ ] Implement `tracking_iterator` template
- [ ] Implement `string_matcher` class:
  - [ ] Pattern type detection and optimization
  - [ ] Glob to regex conversion
  - [ ] Regex compilation with Boost.Xpressive
  - [ ] Boyer-Moore-Horspool for verbatim patterns
  - [ ] `is_match` with all pattern types
- [ ] Implement `MatchOperation` class:
  - [ ] Pattern preprocessing
  - [ ] Root path extraction
  - [ ] Auto-wrapping for simple patterns
  - [ ] `prematch` for drive filtering
- [ ] Add comprehensive tests for all pattern types
