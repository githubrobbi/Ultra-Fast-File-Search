#pragma once
// Extended FILE_ATTRIBUTE_* constants not in standard Windows SDK headers
// These are newer attributes added in Windows 8/10/11

#include <Windows.h>

// Ensure we don't redefine if already defined by newer SDK
#ifndef FILE_ATTRIBUTE_DEVICE
#define FILE_ATTRIBUTE_DEVICE                0x00000040
#endif

#ifndef FILE_ATTRIBUTE_NORMAL
#define FILE_ATTRIBUTE_NORMAL                0x00000080
#endif

#ifndef FILE_ATTRIBUTE_TEMPORARY
#define FILE_ATTRIBUTE_TEMPORARY             0x00000100
#endif

#ifndef FILE_ATTRIBUTE_ENCRYPTED
#define FILE_ATTRIBUTE_ENCRYPTED             0x00004000
#endif

#ifndef FILE_ATTRIBUTE_INTEGRITY_STREAM
#define FILE_ATTRIBUTE_INTEGRITY_STREAM      0x00008000
#endif

#ifndef FILE_ATTRIBUTE_VIRTUAL
#define FILE_ATTRIBUTE_VIRTUAL               0x00010000
#endif

#ifndef FILE_ATTRIBUTE_NO_SCRUB_DATA
#define FILE_ATTRIBUTE_NO_SCRUB_DATA         0x00020000
#endif

#ifndef FILE_ATTRIBUTE_EA
#define FILE_ATTRIBUTE_EA                    0x00040000
#endif

#ifndef FILE_ATTRIBUTE_PINNED
#define FILE_ATTRIBUTE_PINNED                0x00080000
#endif

#ifndef FILE_ATTRIBUTE_UNPINNED
#define FILE_ATTRIBUTE_UNPINNED              0x00100000
#endif

#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN        0x00040000
#endif

#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif

/*
 * Windows ATTRIB command reference:
 *
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
 */

