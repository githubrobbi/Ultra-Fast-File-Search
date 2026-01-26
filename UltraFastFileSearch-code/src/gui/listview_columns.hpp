#pragma once
/**
 * @file listview_columns.hpp
 * @brief ListView column index definitions
 * 
 * Defines the column indices for the main file list view.
 * These indices are used for sorting, display, and data retrieval.
 * 
 * Extracted from main_dialog.hpp for reusability.
 */

namespace uffs {

/**
 * @brief Column indices for the main file ListView
 * 
 * These indices correspond to the columns in the file list,
 * used for sorting and retrieving cell data.
 */
enum ListViewColumn : int
{
    COLUMN_INDEX_NAME = 0,
    COLUMN_INDEX_PATH,
    COLUMN_INDEX_TYPE,
    COLUMN_INDEX_SIZE,
    COLUMN_INDEX_SIZE_ON_DISK,
    COLUMN_INDEX_CREATION_TIME,
    COLUMN_INDEX_MODIFICATION_TIME,
    COLUMN_INDEX_ACCESS_TIME,
    COLUMN_INDEX_DESCENDENTS,
    
    // Boolean attribute columns
    COLUMN_INDEX_is_readonly,
    COLUMN_INDEX_is_archive,
    COLUMN_INDEX_is_system,
    COLUMN_INDEX_is_hidden,
    COLUMN_INDEX_is_offline,
    COLUMN_INDEX_is_notcontentidx,
    COLUMN_INDEX_is_noscrubdata,
    COLUMN_INDEX_is_integretystream,
    COLUMN_INDEX_is_pinned,
    COLUMN_INDEX_is_unpinned,
    COLUMN_INDEX_is_directory,
    COLUMN_INDEX_is_compressed,
    COLUMN_INDEX_is_encrypted,
    COLUMN_INDEX_is_sparsefile,
    COLUMN_INDEX_is_reparsepoint,
    
    COLUMN_INDEX_ATTRIBUTE,
    
    // Count of columns
    COLUMN_INDEX_COUNT
};

/**
 * @brief Check if a column is a boolean attribute column
 */
[[nodiscard]] inline constexpr bool isAttributeColumn(ListViewColumn col) noexcept
{
    return col >= COLUMN_INDEX_is_readonly && col <= COLUMN_INDEX_is_reparsepoint;
}

/**
 * @brief Check if a column is sortable by numeric value
 */
[[nodiscard]] inline constexpr bool isNumericColumn(ListViewColumn col) noexcept
{
    return col == COLUMN_INDEX_SIZE || 
           col == COLUMN_INDEX_SIZE_ON_DISK || 
           col == COLUMN_INDEX_DESCENDENTS;
}

/**
 * @brief Check if a column is a date/time column
 */
[[nodiscard]] inline constexpr bool isDateTimeColumn(ListViewColumn col) noexcept
{
    return col == COLUMN_INDEX_CREATION_TIME || 
           col == COLUMN_INDEX_MODIFICATION_TIME || 
           col == COLUMN_INDEX_ACCESS_TIME;
}

} // namespace uffs

// Backward compatibility - expose at global scope for existing code
using uffs::ListViewColumn;
using uffs::COLUMN_INDEX_NAME;
using uffs::COLUMN_INDEX_PATH;
using uffs::COLUMN_INDEX_TYPE;
using uffs::COLUMN_INDEX_SIZE;
using uffs::COLUMN_INDEX_SIZE_ON_DISK;
using uffs::COLUMN_INDEX_CREATION_TIME;
using uffs::COLUMN_INDEX_MODIFICATION_TIME;
using uffs::COLUMN_INDEX_ACCESS_TIME;
using uffs::COLUMN_INDEX_DESCENDENTS;
using uffs::COLUMN_INDEX_is_readonly;
using uffs::COLUMN_INDEX_is_archive;
using uffs::COLUMN_INDEX_is_system;
using uffs::COLUMN_INDEX_is_hidden;
using uffs::COLUMN_INDEX_is_offline;
using uffs::COLUMN_INDEX_is_notcontentidx;
using uffs::COLUMN_INDEX_is_noscrubdata;
using uffs::COLUMN_INDEX_is_integretystream;
using uffs::COLUMN_INDEX_is_pinned;
using uffs::COLUMN_INDEX_is_unpinned;
using uffs::COLUMN_INDEX_is_directory;
using uffs::COLUMN_INDEX_is_compressed;
using uffs::COLUMN_INDEX_is_encrypted;
using uffs::COLUMN_INDEX_is_sparsefile;
using uffs::COLUMN_INDEX_is_reparsepoint;
using uffs::COLUMN_INDEX_ATTRIBUTE;
using uffs::COLUMN_INDEX_COUNT;

