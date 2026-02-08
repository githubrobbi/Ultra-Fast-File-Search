/**
 * @file exception_handler.hpp
 * @brief Global exception handler for unhandled exceptions.
 *
 * This file provides a top-level exception handler that catches unhandled
 * structured exceptions (SEH) and presents a user dialog for crash recovery.
 *
 * ## Architecture Overview
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    EXCEPTION HANDLING FLOW                              │
 * │                                                                         │
 * │  ┌─────────────┐     ┌──────────────────┐     ┌───────────────────┐    │
 * │  │ IOCP Thread │────▶│ SEH Exception    │────▶│ global_exception_ │    │
 * │  │ (async I/O) │     │ (access violation│     │ handler()         │    │
 * │  └─────────────┘     │  divide by zero) │     └─────────┬─────────┘    │
 * │                      └──────────────────┘               │              │
 * │                                                         ▼              │
 * │                                              ┌───────────────────┐     │
 * │                                              │ User Dialog       │     │
 * │                                              │ - Abort: Exit     │     │
 * │                                              │ - Retry: Continue │     │
 * │                                              │ - Ignore: Resume  │     │
 * │                                              └───────────────────┘     │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Exception Filtering
 *
 * The handler filters out certain exception types that should not trigger
 * the user dialog:
 *
 * | Exception Code | Name | Action |
 * |----------------|------|--------|
 * | 0x40010006 | DBG_PRINTEXCEPTION_C | Continue search |
 * | 0x4001000A | DBG_PRINTEXCEPTION_WIDE_C | Continue search |
 * | 0xE06D7363 | C++ exception | Continue search |
 * | RPC_S_SERVER_UNAVAILABLE | RPC error | Continue search |
 *
 * ## Thread Safety
 *
 * The handler uses a recursive mutex to prevent concurrent dialogs from
 * multiple threads. This ensures only one error dialog is shown at a time.
 *
 * @note This handler is registered via SetUnhandledExceptionFilter() in
 *       io_completion_port.hpp.
 *
 * @see io_completion_port.hpp for where this handler is registered
 * @see error_utils.hpp for safe_stprintf() used in message formatting
 */

#ifndef UFFS_EXCEPTION_HANDLER_HPP
#define UFFS_EXCEPTION_HANDLER_HPP

#include <Windows.h>
#include <tchar.h>
#include <atlbase.h>

#include "atomic_compat.hpp"
#include "error_utils.hpp"

// ============================================================================
// EXTERNAL DECLARATIONS (at global scope)
// ============================================================================

/**
 * @brief The topmost window handle for displaying error dialogs.
 *
 * This variable is defined in UltraFastFileSearch.cpp and provides
 * the parent window for MessageBox calls. For CLI builds, this will be
 * an empty/null window handle.
 */
extern ATL::CWindow topmostWindow;

namespace uffs {

// ============================================================================
// GLOBAL STATE
// ============================================================================

/**
 * @brief Mutex to prevent concurrent exception dialogs.
 *
 * When multiple threads encounter exceptions simultaneously, this mutex
 * ensures only one dialog is shown at a time.
 */
inline atomic_namespace::recursive_mutex global_exception_mutex;

// ============================================================================
// EXCEPTION HANDLER
// ============================================================================

/**
 * @brief Global exception handler for unhandled structured exceptions.
 *
 * This function is called by the Windows SEH mechanism when an unhandled
 * exception occurs. It presents a dialog to the user with options:
 *
 * - **Abort**: Immediately exit the process with the exception code
 * - **Retry**: Return EXCEPTION_CONTINUE_SEARCH to let other handlers try
 * - **Ignore**: Return EXCEPTION_CONTINUE_EXECUTION to resume (dangerous!)
 *
 * ## Algorithm
 *
 * 1. Check if exception is a filtered type (debug, C++, RPC)
 *    - If yes, return EXCEPTION_CONTINUE_SEARCH
 * 2. Acquire the exception mutex (prevent concurrent dialogs)
 * 3. Format error message with exception code
 * 4. Display MessageBox with appropriate buttons
 * 5. Handle user response:
 *    - IDABORT: Call _exit() with exception code
 *    - IDRETRY: Return EXCEPTION_CONTINUE_SEARCH
 *    - IDIGNORE: Return EXCEPTION_CONTINUE_EXECUTION
 *
 * @param ExceptionInfo  Pointer to exception information structure
 * @return EXCEPTION_CONTINUE_SEARCH or EXCEPTION_CONTINUE_EXECUTION
 *
 * @warning Returning EXCEPTION_CONTINUE_EXECUTION after a fatal exception
 *          (like access violation) will likely cause immediate re-crash.
 */
inline long global_exception_handler(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    long result;

    // Filter out exceptions that should not trigger user dialog
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == 0x40010006 /*DBG_PRINTEXCEPTION_C*/ ||
        ExceptionInfo->ExceptionRecord->ExceptionCode == 0x4001000A /*DBG_PRINTEXCEPTION_WIDE_C*/ ||
        ExceptionInfo->ExceptionRecord->ExceptionCode == 0xE06D7363 /*C++ exception*/ ||
        ExceptionInfo->ExceptionRecord->ExceptionCode == RPC_S_SERVER_UNAVAILABLE)
    {
        result = EXCEPTION_CONTINUE_SEARCH;
    }
    else
    {
        // Acquire mutex to prevent concurrent dialogs
        atomic_namespace::unique_lock<atomic_namespace::recursive_mutex>
            const guard(global_exception_mutex);

        // Format error message
        TCHAR buf[512];
        uffs::error::safe_stprintf(buf,
            _T("The program encountered an error 0x%lX.\r\n\r\n")
            _T("PLEASE send me an email, so I can try to fix it.!\r\n\r\n")
            _T("If you see OK, press OK.\r\nOtherwise:\r\n")
            _T("- Press Retry to attempt to handle the error (recommended)\r\n")
            _T("- Press Abort to quit\r\n")
            _T("- Press Ignore to continue (NOT recommended)"),
            ExceptionInfo->ExceptionRecord->ExceptionCode);
        buf[_countof(buf) - 1] = _T('\0');  // Ensure null termination

        // Determine dialog buttons based on exception continuability
        UINT const buttons = (ExceptionInfo->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
            ? MB_OK
            : MB_ABORTRETRYIGNORE;

        // Show dialog
        int const r = MessageBox(
            topmostWindow.m_hWnd,
            buf,
            _T("Fatal Error"),
            MB_ICONERROR | buttons | MB_TASKMODAL);

        // Handle user response
        if (r == IDABORT)
        {
            _exit(ExceptionInfo->ExceptionRecord->ExceptionCode);
        }

        result = (r == IDIGNORE) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
    }

    return result;
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::global_exception_handler;
using uffs::global_exception_mutex;

#endif // UFFS_EXCEPTION_HANDLER_HPP

