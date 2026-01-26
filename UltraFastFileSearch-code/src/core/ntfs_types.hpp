#pragma once

// ============================================================================
// NTFS Type Definitions
// ============================================================================
// This file contains all NTFS-related structures for the Ultra Fast File Search
// project. These structures are used for reading and parsing NTFS Master File
// Table (MFT) records.
//
// Reference: https://docs.microsoft.com/en-us/windows/win32/fileio/master-file-table
// ============================================================================

#include <Windows.h>
#include <tchar.h>
#include <cstdint>

namespace uffs {
namespace ntfs {

// ============================================================================
// NTFS Boot Sector
// ============================================================================
#pragma pack(push, 1)
struct BootSector
{
    unsigned char Jump[3];
    unsigned char Oem[8];
    unsigned short BytesPerSector;
    unsigned char SectorsPerCluster;
    unsigned short ReservedSectors;
    unsigned char Padding1[3];
    unsigned short Unused1;
    unsigned char MediaDescriptor;
    unsigned short Padding2;
    unsigned short SectorsPerTrack;
    unsigned short NumberOfHeads;
    unsigned long HiddenSectors;
    unsigned long Unused2;
    unsigned long Unused3;
    long long TotalSectors;
    long long MftStartLcn;
    long long Mft2StartLcn;
    signed char ClustersPerFileRecordSegment;
    unsigned char Padding3[3];
    unsigned long ClustersPerIndexBlock;
    long long VolumeSerialNumber;
    unsigned long Checksum;
    unsigned char BootStrap[0x200 - 0x54];

    unsigned int file_record_size() const
    {
        return this->ClustersPerFileRecordSegment >= 0
            ? this->ClustersPerFileRecordSegment * this->SectorsPerCluster * this->BytesPerSector
            : 1U << static_cast<int>(-this->ClustersPerFileRecordSegment);
    }

    unsigned int cluster_size() const
    {
        return this->SectorsPerCluster * this->BytesPerSector;
    }
};
#pragma pack(pop)

// Verify size at compile time
static_assert(sizeof(BootSector) == 512, "BootSector must be 512 bytes");

// ============================================================================
// Multi-Sector Header
// ============================================================================
struct MultiSectorHeader
{
    unsigned long Magic;
    unsigned short USAOffset;
    unsigned short USACount;

    bool unfixup(size_t max_size)
    {
        unsigned short* usa = reinterpret_cast<unsigned short*>(
            &reinterpret_cast<unsigned char*>(this)[this->USAOffset]);
        unsigned short const usa0 = usa[0];
        bool result = true;
        for (unsigned short i = 1; i < this->USACount; i++)
        {
            const size_t offset = i * 512 - sizeof(unsigned short);
            unsigned short* const check = (unsigned short*)((unsigned char*)this + offset);
            if (offset < max_size)
            {
                result &= *check == usa0;
                *check = usa[i];
            }
            else
            {
                break;
            }
        }
        return result;
    }
};

// ============================================================================
// Attribute Type Codes
// ============================================================================
enum class AttributeTypeCode : int
{
    AttributeNone                = 0,      // For default-constructed comparison
    AttributeStandardInformation = 0x10,
    AttributeAttributeList       = 0x20,
    AttributeFileName            = 0x30,
    AttributeObjectId            = 0x40,
    AttributeSecurityDescriptor  = 0x50,
    AttributeVolumeName          = 0x60,
    AttributeVolumeInformation   = 0x70,
    AttributeData                = 0x80,
    AttributeIndexRoot           = 0x90,
    AttributeIndexAllocation     = 0xA0,
    AttributeBitmap              = 0xB0,
    AttributeReparsePoint        = 0xC0,
    AttributeEAInformation       = 0xD0,
    AttributeEA                  = 0xE0,
    AttributePropertySet         = 0xF0,
    AttributeLoggedUtilityStream = 0x100,
    AttributeEnd                 = -1,
};

// ============================================================================
// File Record Header Flags
// ============================================================================
enum FileRecordHeaderFlags
{
    FRH_IN_USE    = 0x0001,  // Record is in use
    FRH_DIRECTORY = 0x0002,  // Record is a directory
};

// ============================================================================
// Reparse Point Type Flags
// ============================================================================
enum ReparseTypeFlags : unsigned int
{
    ReparseIsMicrosoft              = 0x80000000,
    ReparseIsHighLatency            = 0x40000000,
    ReparseIsAlias                  = 0x20000000,
    ReparseTagNSS                   = 0x68000005,
    ReparseTagNSSRecover            = 0x68000006,
    ReparseTagSIS                   = 0x68000007,
    ReparseTagSDFS                  = 0x68000008,
    ReparseTagMountPoint            = 0x88000003,
    ReparseTagHSM                   = 0xA8000004,
    ReparseTagSymbolicLink          = 0xE8000000,
    ReparseTagMountPoint2           = 0xA0000003,
    ReparseTagSymbolicLink2         = 0xA000000C,
    ReparseTagWofCompressed         = 0x80000017,
    ReparseTagWindowsContainerImage = 0x80000018,
    ReparseTagGlobalReparse         = 0x80000019,
    ReparseTagAppExecLink           = 0x8000001B,
    ReparseTagCloud                 = 0x9000001A,
    ReparseTagGVFS                  = 0x9000001C,
    ReparseTagLinuxSymbolicLink     = 0xA000001D,
};

// ============================================================================
// Attribute Record Header
// ============================================================================
struct AttributeRecordHeader
{
    AttributeTypeCode Type;
    unsigned long Length;
    unsigned char IsNonResident;
    unsigned char NameLength;
    unsigned short NameOffset;
    unsigned short Flags;    // 0x0001 = Compressed, 0x4000 = Encrypted, 0x8000 = Sparse
    unsigned short Instance;

    // Resident and NonResident data share the same memory location (union)
    // because an attribute is either resident OR non-resident, never both.
    struct ResidentData
    {
        unsigned long ValueLength;
        unsigned short ValueOffset;
        unsigned short Flags;

        // GetValue() - returns pointer to the attribute value
        // Uses offsetof to calculate the AttributeRecordHeader base address
        inline void* GetValue()
        {
            // Calculate offset from Resident member to start of AttributeRecordHeader
            auto* base = reinterpret_cast<char*>(this) - offsetof(AttributeRecordHeader, Resident);
            return reinterpret_cast<void*>(base + this->ValueOffset);
        }

        inline void const* GetValue() const
        {
            auto* base = reinterpret_cast<const char*>(this) - offsetof(AttributeRecordHeader, Resident);
            return reinterpret_cast<const void*>(base + this->ValueOffset);
        }
    };

    struct NonResidentData
    {
        long long LowestVCN;
        long long HighestVCN;
        unsigned short MappingPairsOffset;
        unsigned char CompressionUnit;
        unsigned char Reserved[5];
        long long AllocatedSize;
        long long DataSize;
        long long InitializedSize;
        long long CompressedSize;
    };

    // CRITICAL: This must be a union! Resident and NonResident occupy the same
    // memory location in the NTFS on-disk format.
    union
    {
        ResidentData Resident;
        NonResidentData NonResident;
    };

    AttributeRecordHeader* next()
    {
        return reinterpret_cast<AttributeRecordHeader*>(
            reinterpret_cast<unsigned char*>(this) + this->Length);
    }

    AttributeRecordHeader const* next() const
    {
        return reinterpret_cast<AttributeRecordHeader const*>(
            reinterpret_cast<unsigned char const*>(this) + this->Length);
    }

    wchar_t* name()
    {
        return reinterpret_cast<wchar_t*>(
            reinterpret_cast<unsigned char*>(this) + this->NameOffset);
    }

    wchar_t const* name() const
    {
        return reinterpret_cast<wchar_t const*>(
            reinterpret_cast<unsigned char const*>(this) + this->NameOffset);
    }
};

// ============================================================================
// File Record Segment Header
// ============================================================================
struct FileRecordSegmentHeader
{
    uffs::ntfs::MultiSectorHeader MultiSectorHeader;
    unsigned long long LogFileSequenceNumber;
    unsigned short SequenceNumber;
    unsigned short LinkCount;
    unsigned short FirstAttributeOffset;
    unsigned short Flags;  // FILE_RECORD_HEADER_FLAGS
    unsigned long BytesInUse;
    unsigned long BytesAllocated;
    unsigned long long BaseFileRecordSegment;
    unsigned short NextAttributeNumber;
    unsigned short SegmentNumberUpper_or_USA_or_UnknownReserved;
    unsigned long SegmentNumberLower;

    AttributeRecordHeader* begin()
    {
        return reinterpret_cast<AttributeRecordHeader*>(
            reinterpret_cast<unsigned char*>(this) + this->FirstAttributeOffset);
    }

    AttributeRecordHeader const* begin() const
    {
        return reinterpret_cast<AttributeRecordHeader const*>(
            reinterpret_cast<unsigned char const*>(this) + this->FirstAttributeOffset);
    }

    void* end(size_t const max_buffer_size = ~size_t())
    {
        return reinterpret_cast<unsigned char*>(this) +
            (max_buffer_size < this->BytesInUse ? max_buffer_size : this->BytesInUse);
    }

    void const* end(size_t const max_buffer_size = ~size_t()) const
    {
        return reinterpret_cast<unsigned char const*>(this) +
            (max_buffer_size < this->BytesInUse ? max_buffer_size : this->BytesInUse);
    }
};

// ============================================================================
// Filename Information
// ============================================================================
struct FilenameInformation
{
    unsigned long long ParentDirectory;
    long long CreationTime;
    long long LastModificationTime;
    long long LastChangeTime;
    long long LastAccessTime;
    long long AllocatedLength;
    long long FileSize;
    unsigned long FileAttributes;
    unsigned short PackedEaSize;
    unsigned short Reserved;
    unsigned char FileNameLength;
    unsigned char Flags;
    WCHAR FileName[1];
};

// ============================================================================
// Standard Information
// ============================================================================
struct StandardInformation
{
    long long CreationTime;
    long long LastModificationTime;
    long long LastChangeTime;
    long long LastAccessTime;
    unsigned long FileAttributes;
    // There's more, but only in newer versions
};

// ============================================================================
// Index Header
// ============================================================================
struct IndexHeader
{
    unsigned long FirstIndexEntry;
    unsigned long FirstFreeByte;
    unsigned long BytesAvailable;
    unsigned char Flags;    // '1' == has INDEX_ALLOCATION
    unsigned char Reserved[3];
};

// ============================================================================
// Index Root
// ============================================================================
struct IndexRoot
{
    AttributeTypeCode Type;
    unsigned long CollationRule;
    unsigned long BytesPerIndexBlock;
    unsigned char ClustersPerIndexBlock;
    IndexHeader Header;
};

// ============================================================================
// Attribute List
// ============================================================================
struct AttributeList
{
    AttributeTypeCode AttributeType;
    unsigned short Length;
    unsigned char NameLength;
    unsigned char NameOffset;
    unsigned long long StartVcn;    // LowVcn
    unsigned long long FileReferenceNumber;
    unsigned short AttributeNumber;
    unsigned short AlignmentOrReserved[3];
};

// ============================================================================
// Reparse Point
// ============================================================================
struct ReparsePoint
{
    ReparseTypeFlags TypeFlags;
    unsigned short DataLength;
    unsigned short Padding;

    void* begin()
    {
        return reinterpret_cast<void*>(
            reinterpret_cast<unsigned char*>(this) + sizeof(*this));
    }

    void const* begin() const
    {
        return reinterpret_cast<void const*>(
            reinterpret_cast<unsigned char const*>(this) + sizeof(*this));
    }

    void* end(size_t const max_buffer_size = ~size_t())
    {
        return reinterpret_cast<unsigned char*>(this->begin()) +
            static_cast<ptrdiff_t>(this->DataLength);
    }

    void const* end(size_t const max_buffer_size = ~size_t()) const
    {
        return reinterpret_cast<unsigned char const*>(this->begin()) +
            static_cast<ptrdiff_t>(this->DataLength);
    }
};

// ============================================================================
// Reparse Mount Point Buffer
// ============================================================================
struct ReparseMountPointBuffer
{
    USHORT SubstituteNameOffset;
    USHORT SubstituteNameLength;
    USHORT PrintNameOffset;
    USHORT PrintNameLength;
    WCHAR PathBuffer[1];
};

// ============================================================================
// Attribute Names Table
// ============================================================================
static struct
{
    TCHAR const* data;
    size_t size;
}
attribute_names[] =
{
#define X(S) { _T(S), sizeof(_T(S)) / sizeof(*_T(S)) - 1 }
    X(""),
    X("$STANDARD_INFORMATION"),
    X("$ATTRIBUTE_LIST"),
    X("$FILE_NAME"),
    X("$OBJECT_ID"),
    X("$SECURITY_DESCRIPTOR"),
    X("$VOLUME_NAME"),
    X("$VOLUME_INFORMATION"),
    X("$DATA"),
    X("$INDEX_ROOT"),
    X("$INDEX_ALLOCATION"),
    X("$BITMAP"),
    X("$REPARSE_POINT"),
    X("$EA_INFORMATION"),
    X("$EA"),
    X("$PROPERTY_SET"),
    X("$LOGGED_UTILITY_STREAM"),
#undef  X
};

} // namespace ntfs
} // namespace uffs

// ============================================================================
// Legacy Type Aliases (for backward compatibility with existing code)
// ============================================================================
// These aliases allow existing code using SCREAMING_CASE names to continue
// working without modification.
// ============================================================================
namespace ntfs {

using uffs::ntfs::BootSector;
using uffs::ntfs::MultiSectorHeader;
using uffs::ntfs::AttributeTypeCode;
using uffs::ntfs::AttributeRecordHeader;
using uffs::ntfs::FileRecordHeaderFlags;
using uffs::ntfs::FileRecordSegmentHeader;
using uffs::ntfs::FilenameInformation;
using uffs::ntfs::StandardInformation;
using uffs::ntfs::IndexHeader;
using uffs::ntfs::IndexRoot;
using uffs::ntfs::AttributeList;
using uffs::ntfs::ReparseTypeFlags;
using uffs::ntfs::ReparsePoint;
using uffs::ntfs::ReparseMountPointBuffer;
using uffs::ntfs::attribute_names;

// Legacy SCREAMING_CASE type aliases
using NTFS_BOOT_SECTOR = uffs::ntfs::BootSector;
using MULTI_SECTOR_HEADER = uffs::ntfs::MultiSectorHeader;
using ATTRIBUTE_RECORD_HEADER = uffs::ntfs::AttributeRecordHeader;
using FILE_RECORD_HEADER_FLAGS = uffs::ntfs::FileRecordHeaderFlags;
using FILE_RECORD_SEGMENT_HEADER = uffs::ntfs::FileRecordSegmentHeader;
using FILENAME_INFORMATION = uffs::ntfs::FilenameInformation;
using STANDARD_INFORMATION = uffs::ntfs::StandardInformation;
using INDEX_HEADER = uffs::ntfs::IndexHeader;
using INDEX_ROOT = uffs::ntfs::IndexRoot;
using ATTRIBUTE_LIST = uffs::ntfs::AttributeList;
using REPARSE_POINT = uffs::ntfs::ReparsePoint;
using REPARSE_MOUNT_POINT_BUFFER = uffs::ntfs::ReparseMountPointBuffer;

// Note: AttributeTypeCode is an enum class, so values are accessed via
// ntfs::AttributeTypeCode::AttributeFileName etc.

// FileRecordHeaderFlags enum values (plain enum, accessible directly)
constexpr auto FRH_IN_USE = uffs::ntfs::FRH_IN_USE;
constexpr auto FRH_DIRECTORY = uffs::ntfs::FRH_DIRECTORY;

} // namespace ntfs

