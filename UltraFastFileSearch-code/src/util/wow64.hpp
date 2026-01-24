#pragma once

// ============================================================================
// WOW64 (Windows 32-bit on Windows 64-bit) Helpers
// ============================================================================
// This file contains utilities for detecting and managing WOW64 file system
// redirection. When running a 32-bit application on 64-bit Windows, the
// system redirects certain file system paths. These utilities allow
// temporarily disabling this redirection when needed.
// Extracted from UltraFastFileSearch.cpp for better organization.
// ============================================================================

#include <Windows.h>

// ============================================================================
// Wow64 - Static helper class for WOW64 detection and redirection control
// ============================================================================
// This class provides lazy-loaded function pointers to WOW64 APIs that may
// not be available on all Windows versions.
//
// Example:
//   if (Wow64::is_wow64()) {
//       void* cookie = Wow64::disable();
//       // ... access real System32 directory ...
//       Wow64::revert(cookie);
//   }
// ============================================================================
struct Wow64
{
    static HMODULE GetKernel32()
    {
        HMODULE kernel32 = nullptr;
        return GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCTSTR>(&GetSystemInfo),
            &kernel32) ? kernel32 : nullptr;
    }

    typedef BOOL WINAPI IsWow64Process_t(IN HANDLE hProcess, OUT PBOOL Wow64Process);
    static IsWow64Process_t* IsWow64Process;

    static bool is_wow64()
    {
        bool result = false;
#ifdef _M_IX86
        BOOL isWOW64 = FALSE;
        if (!IsWow64Process)
        {
            IsWow64Process = reinterpret_cast<IsWow64Process_t*>(
                GetProcAddress(GetKernel32(), _CRT_STRINGIZE(IsWow64Process)));
        }
        result = IsWow64Process && IsWow64Process(GetCurrentProcess(), &isWOW64) && isWOW64;
#endif
        return result;
    }

    typedef BOOL WINAPI Wow64DisableWow64FsRedirection_t(PVOID* OldValue);
    static Wow64DisableWow64FsRedirection_t* Wow64DisableWow64FsRedirection;

    static void* disable()
    {
        void* old = nullptr;
#ifdef _M_IX86
        if (!Wow64DisableWow64FsRedirection)
        {
            Wow64DisableWow64FsRedirection = reinterpret_cast<Wow64DisableWow64FsRedirection_t*>(
                GetProcAddress(GetKernel32(), _CRT_STRINGIZE(Wow64DisableWow64FsRedirection)));
        }
        if (Wow64DisableWow64FsRedirection && !Wow64DisableWow64FsRedirection(&old))
        {
            old = nullptr;
        }
#endif
        return old;
    }

    typedef BOOL WINAPI Wow64RevertWow64FsRedirection_t(PVOID OlValue);
    static Wow64RevertWow64FsRedirection_t* Wow64RevertWow64FsRedirection;

    static bool revert(PVOID old)
    {
        bool result = false;
#ifdef _M_IX86
        if (!Wow64RevertWow64FsRedirection)
        {
            Wow64RevertWow64FsRedirection = reinterpret_cast<Wow64RevertWow64FsRedirection_t*>(
                GetProcAddress(GetKernel32(), _CRT_STRINGIZE(Wow64RevertWow64FsRedirection)));
        }
        result = Wow64RevertWow64FsRedirection && Wow64RevertWow64FsRedirection(old);
#endif
        return result;
    }

    Wow64() {}
};

// Static member initialization (only for x86 builds)
#ifdef _M_IX86
inline Wow64 const init_wow64;
inline Wow64::IsWow64Process_t* Wow64::IsWow64Process = nullptr;
inline Wow64::Wow64DisableWow64FsRedirection_t* Wow64::Wow64DisableWow64FsRedirection = nullptr;
inline Wow64::Wow64RevertWow64FsRedirection_t* Wow64::Wow64RevertWow64FsRedirection = nullptr;
#endif

// ============================================================================
// Wow64Disable - RAII wrapper for WOW64 file system redirection
// ============================================================================
// This class disables WOW64 file system redirection on construction and
// restores it on destruction. Use it when you need to access the real
// System32 directory from a 32-bit process on 64-bit Windows.
//
// Example:
//   {
//       Wow64Disable disable_redirect(true);
//       // ... access real System32 directory ...
//   } // Redirection restored here
// ============================================================================
struct Wow64Disable
{
    bool disable;
    void* cookie;

    explicit Wow64Disable(bool disable) : disable(disable), cookie(nullptr)
    {
        if (this->disable)
        {
            this->cookie = Wow64::disable();
        }
    }

    ~Wow64Disable()
    {
        if (this->disable)
        {
            Wow64::revert(this->cookie);
        }
    }

    // Non-copyable
    Wow64Disable(const Wow64Disable&) = delete;
    Wow64Disable& operator=(const Wow64Disable&) = delete;
};

