#pragma once
/**
 * @file icon_cache_types.hpp
 * @brief Types for shell icon caching
 * 
 * Contains data structures for caching shell icons and file type information.
 * Used by the main dialog to avoid repeated shell queries for the same files.
 * 
 * Extracted from main_dialog.hpp for reusability.
 */

#include <map>
#include <tchar.h>
#include "../util/core_types.hpp"

namespace uffs {

/**
 * @brief Cached icon and type information for a file
 * 
 * Stores shell icon indices and type name for a file path.
 * The counter is used for LRU-style cache eviction.
 */
struct IconCacheEntry
{
    explicit IconCacheEntry(size_t counter) noexcept
        : counter(counter)
        , valid(false)
        , iIconSmall(-1)
        , iIconLarge(-1)
        , iIconExtraLarge(-1)
    {
        szTypeName[0] = _T('\0');
    }

    size_t counter;           ///< Access counter for LRU eviction
    bool valid;               ///< Whether the icon has been loaded
    int iIconSmall;           ///< Index in small image list (-1 if not loaded)
    int iIconLarge;           ///< Index in large image list (-1 if not loaded)
    int iIconExtraLarge;      ///< Index in extra-large image list (-1 if not loaded)
    TCHAR szTypeName[80];     ///< Shell type name (e.g., "Text Document")
    std::tvstring description; ///< Extended description from version info
};

/**
 * @brief Cache mapping file paths to icon information
 * 
 * Keys are reversed paths for efficient prefix matching.
 */
using ShellIconCache = std::map<std::tvstring, IconCacheEntry>;

/**
 * @brief Cache mapping file extensions to type descriptions
 * 
 * Used for quick lookup of file type without shell queries.
 */
using FileTypeCache = std::map<std::tvstring, std::tvstring>;

} // namespace uffs

// Backward compatibility aliases
using CacheInfo = uffs::IconCacheEntry;
using ShellInfoCache = uffs::ShellIconCache;
using TypeInfoCache = uffs::FileTypeCache;

