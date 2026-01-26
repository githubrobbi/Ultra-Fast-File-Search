#pragma once
/**
 * @file file_attribute_colors.hpp
 * @brief Color constants for file attribute visualization
 * 
 * Defines COLORREF constants used to display files with different
 * attributes (compressed, encrypted, hidden, etc.) in the ListView.
 * 
 * Extracted from main_dialog.hpp for reusability.
 */

#include <Windows.h>

namespace uffs {

/**
 * @brief Color palette for file attribute visualization
 * 
 * These colors are used in ListView custom draw to indicate
 * file attributes at a glance.
 */
struct FileAttributeColors
{
    COLORREF deleted;     ///< Deleted/orphaned files (gray)
    COLORREF encrypted;   ///< Encrypted files (green)
    COLORREF compressed;  ///< Compressed files (blue)
    COLORREF directory;   ///< Directories (orange)
    COLORREF hidden;      ///< Hidden files (light red/pink)
    COLORREF system;      ///< System files (red)
    COLORREF sparse;      ///< Sparse files (blue-gray blend)

    /**
     * @brief Get the default color palette
     * 
     * Returns the standard Windows Explorer-like color scheme.
     */
    [[nodiscard]] static constexpr FileAttributeColors defaults() noexcept
    {
        return FileAttributeColors{
            RGB(0xC0, 0xC0, 0xC0),  // deleted - gray
            RGB(0x00, 0xFF, 0x00),  // encrypted - green
            RGB(0x00, 0x00, 0xFF),  // compressed - blue
            RGB(0xFF, 0x99, 0x33),  // directory - orange
            RGB(0xFF, 0x99, 0x99),  // hidden - light red/pink
            RGB(0xFF, 0x00, 0x00),  // system - red
            RGB(0x00, 0x7F, 0x7F)   // sparse - blue-gray (average of compressed G+B)
        };
    }

    /**
     * @brief Get color for file attributes
     * 
     * @param attrs File attributes (FILE_ATTRIBUTE_* flags)
     * @param defaultColor Color to return if no special attribute
     * @return COLORREF for the most significant attribute
     */
    [[nodiscard]] COLORREF colorForAttributes(unsigned long attrs, COLORREF defaultColor = 0) const noexcept
    {
        // Check in priority order (most important first)
        if ((attrs & 0x40000000) != 0)              return deleted;    // Deleted/orphaned
        if ((attrs & FILE_ATTRIBUTE_SYSTEM) != 0)   return system;
        if ((attrs & FILE_ATTRIBUTE_HIDDEN) != 0)   return hidden;
        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) return directory;
        if ((attrs & FILE_ATTRIBUTE_COMPRESSED) != 0) return compressed;
        if ((attrs & FILE_ATTRIBUTE_ENCRYPTED) != 0) return encrypted;
        if ((attrs & FILE_ATTRIBUTE_SPARSE_FILE) != 0) return sparse;
        return defaultColor;
    }
};

/// Default file attribute color palette (constexpr)
inline constexpr FileAttributeColors kDefaultFileColors = FileAttributeColors::defaults();

} // namespace uffs

// Backward compatibility
using uffs::FileAttributeColors;
using uffs::kDefaultFileColors;

