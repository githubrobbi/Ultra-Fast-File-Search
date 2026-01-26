#pragma once
// ============================================================================
// nt_user_hooks.hpp - NT User API Hook Type Declarations
// ============================================================================
// Declares the hook types for NT user API functions used by listview_hooks.hpp.
// These must be defined before listview_hooks.hpp is included.
//
// NOTE: This header only DECLARES the hook types using HOOK_DECLARE.
// The static member initialization (HOOK_IMPLEMENT) is done in
// UltraFastFileSearch.cpp to avoid multiple definition errors.
// ============================================================================

#include "nt_user_call_hook.hpp"

// Declare the hook types (struct definitions only)
// The static _instance members are defined in UltraFastFileSearch.cpp

HOOK_DECLARE(void, NtUserGetProp, HANDLE __stdcall(HWND hWnd, ATOM PropId), HOOK_EMPTY_XARGS);
HOOK_DECLARE(void, NtUserSetProp, BOOL __stdcall(HWND hWnd, ATOM PropId, HANDLE value), HOOK_EMPTY_XARGS);
HOOK_DECLARE(void, NtUserMessageCall, LRESULT __stdcall(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, ULONG_PTR xParam, DWORD xpfnProc, BOOL bAnsi), HOOK_EMPTY_XARGS);
HOOK_DECLARE(void, NtUserRedrawWindow, BOOL __stdcall(HWND hWnd, CONST RECT * lprcUpdate, HRGN hrgnUpdate, UINT flags), HOOK_EMPTY_XARGS);

