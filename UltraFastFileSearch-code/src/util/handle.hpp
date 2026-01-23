#pragma once

// ============================================================================
// RAII Handle Wrapper
// ============================================================================
// This file contains a RAII wrapper for Windows HANDLE objects.
// Extracted from UltraFastFileSearch.cpp for better organization.
// ============================================================================

#include <Windows.h>
#include <stdexcept>
#include <utility>

namespace uffs {

// ============================================================================
// Handle - RAII wrapper for Windows HANDLE
// ============================================================================
// This class provides automatic resource management for Windows handles.
// It supports copy (via DuplicateHandle) and move semantics.
// ============================================================================
class Handle
{
    static bool valid(void* const value)
    {
        return value && value != reinterpret_cast<void*>(-1);
    }

public:
    void* value;

    Handle() : value() {}

    explicit Handle(void* const value) : value(value)
    {
        if (!valid(value))
        {
            throw std::invalid_argument("invalid handle");
        }
    }

    Handle(Handle const& other) : value(other.value)
    {
        if (valid(this->value))
        {
            if (!DuplicateHandle(GetCurrentProcess(), this->value, 
                                 GetCurrentProcess(), &this->value, 
                                 MAXIMUM_ALLOWED, TRUE, DUPLICATE_SAME_ACCESS))
            {
                throw std::runtime_error("DuplicateHandle failed");
            }
        }
    }

    ~Handle()
    {
        if (valid(this->value))
        {
            CloseHandle(value);
        }
    }

    Handle& operator=(Handle other)
    {
        return other.swap(*this), *this;
    }

    operator void*() const volatile
    {
        return this->value;
    }

    operator void*() const
    {
        return this->value;
    }

    void swap(Handle& other)
    {
        using std::swap;
        swap(this->value, other.value);
    }

    friend void swap(Handle& a, Handle& b)
    {
        return a.swap(b);
    }
};

} // namespace uffs

