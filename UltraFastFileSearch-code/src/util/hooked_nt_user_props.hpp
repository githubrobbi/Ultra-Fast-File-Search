#pragma once
// ============================================================================
// hooked_nt_user_props.hpp - NT User Property Hook Caching
// ============================================================================
// Provides caching for NtUserGetProp/NtUserSetProp calls to improve ListView
// performance by avoiding redundant system calls.
//
// Dependencies:
// - nt_user_hooks.hpp (for HOOK_TYPE macros)
// - nt_user_call_hook.hpp (for hook_detail::thread_hook_swap)
// ============================================================================

#include "nt_user_call_hook.hpp"
#include "nt_user_hooks.hpp"

// ============================================================================
// HookedNtUserProps - Caching wrapper for NT User property hooks
// ============================================================================
// This struct caches the result of NtUserGetProp calls to avoid redundant
// system calls when the same property is queried multiple times for the
// same window.
// ============================================================================
struct HookedNtUserProps
{
    typedef HookedNtUserProps hook_type;
    HWND prev_hwnd;
    ATOM prev_atom;
    HANDLE prev_result;

    struct : hook_detail::thread_hook_swap<HOOK_TYPE(NtUserGetProp)>
    {
        HANDLE operator()(HWND hWnd, ATOM PropId) override
        {
            hook_type* const self = CONTAINING_RECORD(this, hook_type, HOOK_CONCAT(hook_, NtUserGetProp));
            if (self->prev_hwnd != hWnd || self->prev_atom != PropId)
            {
                self->prev_result = this->hook_base_type::operator()(hWnd, PropId);
                self->prev_hwnd = hWnd;
                self->prev_atom = PropId;
            }
            return self->prev_result;
        }
    } HOOK_CONCAT(hook_, NtUserGetProp);

    struct : hook_detail::thread_hook_swap<HOOK_TYPE(NtUserSetProp)>
    {
        BOOL operator()(HWND hWnd, ATOM PropId, HANDLE value) override
        {
            hook_type* const self = CONTAINING_RECORD(this, hook_type, HOOK_CONCAT(hook_, NtUserSetProp));
            BOOL const result = this->hook_base_type::operator()(hWnd, PropId, value);
            if (result && self->prev_hwnd == hWnd && self->prev_atom == PropId)
            {
                self->prev_result = value;
            }
            return result;
        }
    } HOOK_CONCAT(hook_, NtUserSetProp);

    HookedNtUserProps() : prev_hwnd(), prev_atom(), prev_result() {}
    ~HookedNtUserProps() {}
};

