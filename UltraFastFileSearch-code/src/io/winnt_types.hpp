#pragma once

// ============================================================================
// Windows NT Types for I/O Operations
// ============================================================================
// This file contains Windows NT API types and function declarations used
// for low-level I/O operations like setting I/O priority.
// Extracted from UltraFastFileSearch.cpp for better organization.
// ============================================================================

#include <Windows.h>
#include <tchar.h>

namespace uffs {
namespace winnt {

// ============================================================================
// I/O Status Block
// ============================================================================
struct IO_STATUS_BLOCK
{
    union
    {
        long Status;
        void* Pointer;
    };
    uintptr_t Information;
};

// ============================================================================
// I/O Priority Hints
// ============================================================================
enum IO_PRIORITY_HINT
{
    IoPriorityVeryLow = 0,
    IoPriorityLow,
    IoPriorityNormal,
    IoPriorityHigh,
    IoPriorityCritical,
    MaxIoPriorityTypes
};

// ============================================================================
// File System Information Structures
// ============================================================================
struct FILE_FS_SIZE_INFORMATION
{
    long long TotalAllocationUnits;
    long long ActualAvailableAllocationUnits;
    unsigned long SectorsPerAllocationUnit;
    unsigned long BytesPerSector;
};

struct FILE_FS_ATTRIBUTE_INFORMATION
{
    unsigned long FileSystemAttributes;
    unsigned long MaximumComponentNameLength;
    unsigned long FileSystemNameLength;
    wchar_t FileSystemName[1];
};

struct FILE_FS_DEVICE_INFORMATION
{
    unsigned long DeviceType;
    unsigned long Characteristics;
};

union FILE_IO_PRIORITY_HINT_INFORMATION
{
    IO_PRIORITY_HINT PriorityHint;
    unsigned long long _alignment;
};

// ============================================================================
// Time Fields
// ============================================================================
struct TIME_FIELDS
{
    short Year;
    short Month;
    short Day;
    short Hour;
    short Minute;
    short Second;
    short Milliseconds;
    short Weekday;
};

// ============================================================================
// NTSTATUS Type
// ============================================================================
typedef long NTSTATUS;

// ============================================================================
// NT API Function Declarations
// ============================================================================
// These are loaded dynamically from ntdll.dll at runtime.
// The macro creates references to function pointers obtained via GetProcAddress.
// ============================================================================

namespace detail {
    template<class T> struct identity { typedef T type; };
}

#define UFFS_WINNT_FUNC(F, T) \
    inline detail::identity<T>::type& F = \
        *reinterpret_cast<detail::identity<T>::type* const&>( \
            static_cast<FARPROC const&>( \
                GetProcAddress(GetModuleHandle(_T("ntdll.dll")), #F)))

UFFS_WINNT_FUNC(NtQueryVolumeInformationFile,
    NTSTATUS NTAPI(HANDLE FileHandle, IO_STATUS_BLOCK* IoStatusBlock,
                   PVOID FsInformation, unsigned long Length,
                   unsigned long FsInformationClass));

UFFS_WINNT_FUNC(NtQueryInformationFile,
    NTSTATUS NTAPI(IN HANDLE FileHandle, OUT IO_STATUS_BLOCK* IoStatusBlock,
                   OUT PVOID FileInformation, IN ULONG Length,
                   IN unsigned long FileInformationClass));

UFFS_WINNT_FUNC(NtSetInformationFile,
    NTSTATUS NTAPI(IN HANDLE FileHandle, OUT IO_STATUS_BLOCK* IoStatusBlock,
                   IN PVOID FileInformation, IN ULONG Length,
                   IN unsigned long FileInformationClass));

UFFS_WINNT_FUNC(RtlNtStatusToDosError,
    unsigned long NTAPI(IN NTSTATUS NtStatus));

UFFS_WINNT_FUNC(RtlTimeToTimeFields,
    VOID NTAPI(LARGE_INTEGER* Time, TIME_FIELDS* TimeFields));

#undef UFFS_WINNT_FUNC

} // namespace winnt
} // namespace uffs

