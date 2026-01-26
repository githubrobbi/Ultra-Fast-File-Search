#pragma once

// ============================================================================
// I/O Priority Management
// ============================================================================
// This file contains the IoPriority RAII class for managing Windows I/O
// priority hints. Setting lower I/O priority prevents MFT reading from
// starving other system I/O operations.
// Extracted from UltraFastFileSearch.cpp for better organization.
// ============================================================================

#include "winnt_types.hpp"
#include <utility>

namespace uffs {

// ============================================================================
// IoPriority - RAII wrapper for I/O priority management
// ============================================================================
// This class saves the current I/O priority on construction and restores
// it on destruction. Use it to temporarily lower I/O priority during
// bulk read operations.
//
// Example:
//   {
//       IoPriority low_priority(volume_handle, winnt::IoPriorityLow);
//       // ... perform bulk I/O at low priority ...
//   } // Original priority restored here
// ============================================================================
class IoPriority
{
    uintptr_t _volume;
    winnt::IO_PRIORITY_HINT _old;

    // Non-assignable
    IoPriority& operator=(IoPriority const&);

public:
    // Query current I/O priority for a file handle
    static winnt::IO_PRIORITY_HINT query(uintptr_t const file)
    {
        winnt::FILE_IO_PRIORITY_HINT_INFORMATION old = { winnt::IoPriorityNormal };
        winnt::IO_STATUS_BLOCK iosb;
        winnt::NtQueryInformationFile(
            reinterpret_cast<HANDLE>(file),
            &iosb,
            &old,
            sizeof(old),
            43  // FileIoPriorityHintInformation
        );
        return old.PriorityHint;
    }

    // Set I/O priority for a file handle
    static void set(uintptr_t const volume, winnt::IO_PRIORITY_HINT const value)
    {
        if (value != winnt::MaxIoPriorityTypes)
        {
            winnt::IO_STATUS_BLOCK iosb;
            winnt::FILE_IO_PRIORITY_HINT_INFORMATION io_priority = { value };

            winnt::NTSTATUS const status = winnt::NtSetInformationFile(
                reinterpret_cast<HANDLE>(volume),
                &iosb,
                &io_priority,
                sizeof(io_priority),
                43  // FileIoPriorityHintInformation
            );

            // Ignore certain expected errors
            if (status != 0 &&
                status != 0xC0000003 /*STATUS_INVALID_INFO_CLASS*/ &&
                status != 0xC0000008 /*STATUS_INVALID_HANDLE*/ &&
                status != 0xC0000024 /*STATUS_OBJECT_TYPE_MISMATCH*/)
            {
#ifdef _DEBUG
                // In debug mode, raise an exception for unexpected errors
                // CppRaiseException(winnt::RtlNtStatusToDosError(status));
#endif
            }
        }
    }

    // Get the volume handle
    uintptr_t volume() const
    {
        return this->_volume;
    }

    // Default constructor
    IoPriority() : _volume(), _old() {}

    // Copy constructor (does not restore priority)
    IoPriority(IoPriority const& other)
        : _volume(other._volume), _old()
    {
        this->_old = winnt::MaxIoPriorityTypes;
    }

    // Constructor that sets priority and saves old value
    explicit IoPriority(uintptr_t const volume, winnt::IO_PRIORITY_HINT const priority)
        : _volume(volume), _old(query(volume))
    {
        set(volume, priority);
    }

    // Get the original priority
    winnt::IO_PRIORITY_HINT old() const
    {
        return this->_old;
    }

    // Destructor restores original priority
    ~IoPriority()
    {
        if (this->_volume)
        {
            set(this->_volume, this->_old);
        }
    }

    // Swap with another IoPriority
    void swap(IoPriority& other)
    {
        using std::swap;
        swap(this->_volume, other._volume);
        swap(this->_old, other._old);
    }
};

// Free function swap
inline void swap(IoPriority& a, IoPriority& b)
{
    a.swap(b);
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::IoPriority;

