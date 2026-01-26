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
public:
    [[nodiscard]] static constexpr bool valid(void* const value) noexcept
    {
        return value && value != reinterpret_cast<void*>(-1);
    }

    void* value;

    constexpr Handle() noexcept : value(nullptr) {}

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

    // Move constructor
    Handle(Handle&& other) noexcept : value(other.value)
    {
        other.value = nullptr;
    }

    ~Handle()
    {
        if (valid(this->value))
        {
            CloseHandle(value);
        }
    }

    Handle& operator=(Handle other) noexcept
    {
        return other.swap(*this), *this;
    }

    [[nodiscard]] operator void*() const volatile noexcept
    {
        return this->value;
    }

    [[nodiscard]] operator void*() const noexcept
    {
        return this->value;
    }

    [[nodiscard]] void* get() const noexcept { return value; }
    [[nodiscard]] bool is_valid() const noexcept { return valid(value); }
    [[nodiscard]] explicit operator bool() const noexcept { return is_valid(); }

    void swap(Handle& other) noexcept
    {
        using std::swap;
        swap(this->value, other.value);
    }

    friend void swap(Handle& a, Handle& b) noexcept
    {
        return a.swap(b);
    }

    // Release ownership without closing
    [[nodiscard]] void* release() noexcept
    {
        void* tmp = value;
        value = nullptr;
        return tmp;
    }

    // Close current handle and take ownership of new one
    void reset(void* new_value = nullptr)
    {
        if (valid(value))
        {
            CloseHandle(value);
        }
        value = new_value;
    }
};

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::Handle;

