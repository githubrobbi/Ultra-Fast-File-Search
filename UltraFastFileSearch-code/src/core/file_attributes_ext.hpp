#pragma once
/**
 * @file file_attributes_ext.hpp
 * @brief Extended FILE_ATTRIBUTE_* constants not in standard Windows SDK headers
 *
 * @details
 * This file provides definitions for newer Windows file attributes that may
 * not be present in older Windows SDK versions. These attributes were added
 * in Windows 8, 10, and 11.
 *
 * ## Attribute Bitmask Overview
 *
 * Windows file attributes are stored as a 32-bit bitmask. Each bit represents
 * a different attribute:
 *
 * ```
 * Bit   Value       Attribute                    Since
 * ---   ----------  -------------------------    --------
 * 0     0x00000001  FILE_ATTRIBUTE_READONLY      NT 3.1
 * 1     0x00000002  FILE_ATTRIBUTE_HIDDEN        NT 3.1
 * 2     0x00000004  FILE_ATTRIBUTE_SYSTEM        NT 3.1
 * 4     0x00000010  FILE_ATTRIBUTE_DIRECTORY     NT 3.1
 * 5     0x00000020  FILE_ATTRIBUTE_ARCHIVE       NT 3.1
 * 6     0x00000040  FILE_ATTRIBUTE_DEVICE        NT 3.1
 * 7     0x00000080  FILE_ATTRIBUTE_NORMAL        NT 3.1
 * 8     0x00000100  FILE_ATTRIBUTE_TEMPORARY     NT 3.1
 * 9     0x00000200  FILE_ATTRIBUTE_SPARSE_FILE   Win2000
 * 10    0x00000400  FILE_ATTRIBUTE_REPARSE_POINT Win2000
 * 11    0x00000800  FILE_ATTRIBUTE_COMPRESSED    NT 3.51
 * 12    0x00001000  FILE_ATTRIBUTE_OFFLINE       Win2000
 * 13    0x00002000  FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  Win2000
 * 14    0x00004000  FILE_ATTRIBUTE_ENCRYPTED     Win2000
 * 15    0x00008000  FILE_ATTRIBUTE_INTEGRITY_STREAM  Win8
 * 16    0x00010000  FILE_ATTRIBUTE_VIRTUAL       Vista
 * 17    0x00020000  FILE_ATTRIBUTE_NO_SCRUB_DATA Win8
 * 18    0x00040000  FILE_ATTRIBUTE_EA            Win8
 * 19    0x00080000  FILE_ATTRIBUTE_PINNED        Win10 1709
 * 20    0x00100000  FILE_ATTRIBUTE_UNPINNED      Win10 1709
 * 22    0x00400000  FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS  Win10
 * ```
 *
 * ## Usage
 *
 * ```cpp
 * #include "file_attributes_ext.hpp"
 *
 * DWORD attrs = GetFileAttributes(path);
 * if (attrs & FILE_ATTRIBUTE_PINNED) {
 *     // File is pinned to local storage (OneDrive)
 * }
 * ```
 *
 * @note All definitions use `#ifndef` guards to avoid conflicts with newer SDKs
 * @see StandardInfo - Uses these attributes in the NTFS index
 * @see https://docs.microsoft.com/en-us/windows/win32/fileio/file-attribute-constants
 */

#include <Windows.h>

// ============================================================================
// Standard Attributes (may be missing in very old SDKs)
// ============================================================================

#ifndef FILE_ATTRIBUTE_DEVICE
#define FILE_ATTRIBUTE_DEVICE                0x00000040  ///< Reserved for system use (device file)
#endif

#ifndef FILE_ATTRIBUTE_NORMAL
#define FILE_ATTRIBUTE_NORMAL                0x00000080  ///< No other attributes set
#endif

#ifndef FILE_ATTRIBUTE_TEMPORARY
#define FILE_ATTRIBUTE_TEMPORARY             0x00000100  ///< Temporary file (kept in memory if possible)
#endif

// ============================================================================
// Windows 2000+ Attributes
// ============================================================================

#ifndef FILE_ATTRIBUTE_ENCRYPTED
#define FILE_ATTRIBUTE_ENCRYPTED             0x00004000  ///< File or directory is encrypted (EFS)
#endif

// ============================================================================
// Windows 8+ Attributes (ReFS and Storage Spaces)
// ============================================================================

#ifndef FILE_ATTRIBUTE_INTEGRITY_STREAM
#define FILE_ATTRIBUTE_INTEGRITY_STREAM      0x00008000  ///< Data integrity stream (ReFS)
#endif

#ifndef FILE_ATTRIBUTE_VIRTUAL
#define FILE_ATTRIBUTE_VIRTUAL               0x00010000  ///< Reserved for system use
#endif

#ifndef FILE_ATTRIBUTE_NO_SCRUB_DATA
#define FILE_ATTRIBUTE_NO_SCRUB_DATA         0x00020000  ///< Exclude from data integrity scan
#endif

#ifndef FILE_ATTRIBUTE_EA
#define FILE_ATTRIBUTE_EA                    0x00040000  ///< File has extended attributes
#endif

// ============================================================================
// Windows 10+ Attributes (OneDrive and Cloud Files)
// ============================================================================

#ifndef FILE_ATTRIBUTE_PINNED
#define FILE_ATTRIBUTE_PINNED                0x00080000  ///< Always available locally (OneDrive)
#endif

#ifndef FILE_ATTRIBUTE_UNPINNED
#define FILE_ATTRIBUTE_UNPINNED              0x00100000  ///< Not locally cached (OneDrive)
#endif

#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN        0x00040000  ///< Recall from remote storage on open
#endif

#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000  ///< Recall from remote storage on data access
#endif

// ============================================================================
// Windows ATTRIB Command Reference
// ============================================================================
/**
 * @brief Reference for the Windows ATTRIB command
 *
 * ```
 * ATTRIB [+R | -R] [+A | -A] [+S | -S] [+H | -H] [+O | -O] [+I | -I]
 *        [+X | -X] [+P | -P] [+U | -U] [drive:][path][filename]
 *        [/S [/D]] [/L]
 *
 * +   Sets an attribute.
 * -   Clears an attribute.
 * R   Read-only file attribute.
 * A   Archive file attribute.
 * S   System file attribute.
 * H   Hidden file attribute.
 * O   Offline attribute.
 * I   Not content indexed file attribute.
 * X   No scrub file attribute.
 * V   Integrity attribute.
 * P   Pinned attribute.
 * U   Unpinned attribute.
 * B   SMR Blob attribute.
 *
 * /S  Processes matching files in the current folder and all subfolders.
 * /D  Processes folders as well.
 * /L  Work on the attributes of the Symbolic Link versus the target.
 * ```
 */

