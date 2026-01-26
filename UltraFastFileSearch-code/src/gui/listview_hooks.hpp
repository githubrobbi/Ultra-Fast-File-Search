// ============================================================================
// listview_hooks.hpp - ListView message and redraw optimization hooks
// ============================================================================
// Extracted from UltraFastFileSearch.cpp during Wave 2 refactoring
// ============================================================================
#pragma once

#include <Windows.h>
#include <tchar.h>

#include "../util/nt_user_call_hook.hpp"

namespace uffs {
namespace gui {

// ============================================================================
// CDisableListViewUnnecessaryMessages - Caches LVM_GETACCVERSION results
// ============================================================================
// Hooks NtUserMessageCall to cache accessibility version queries, avoiding
// repeated expensive calls during ListView operations.
// ============================================================================
class CDisableListViewUnnecessaryMessages : hook_detail::thread_hook_swap<HOOK_TYPE(NtUserMessageCall)>
{
	typedef CDisableListViewUnnecessaryMessages hook_type;
	CDisableListViewUnnecessaryMessages(hook_type const&);
	hook_type& operator=(hook_type const&);

	enum { LVM_GETACCVERSION = 0x10C1 };

	struct
	{
		HWND hwnd;
		bool valid;
		LRESULT value;
	} prev_result_LVM_GETACCVERSION;

	LRESULT operator()(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
		ULONG_PTR xParam, DWORD xpfnProc, BOOL bAnsi) override
	{
		LRESULT result;
		if (msg == LVM_GETACCVERSION &&
			this->prev_result_LVM_GETACCVERSION.valid &&
			this->prev_result_LVM_GETACCVERSION.hwnd == hWnd)
		{
			result = msg == LVM_GETACCVERSION && this->prev_result_LVM_GETACCVERSION.valid
				? this->prev_result_LVM_GETACCVERSION.value
				: 0;
		}
		else
		{
			result = this->hook_base_type::operator()(hWnd, msg, wParam, lParam,
				xParam, xpfnProc, bAnsi);
			if (msg == LVM_GETACCVERSION)
			{
				this->prev_result_LVM_GETACCVERSION.hwnd = hWnd;
				this->prev_result_LVM_GETACCVERSION.value = result;
				this->prev_result_LVM_GETACCVERSION.valid = true;
			}
		}
		return result;
	}

public:
	CDisableListViewUnnecessaryMessages() : prev_result_LVM_GETACCVERSION() {}
	~CDisableListViewUnnecessaryMessages() {}
};

#ifdef WM_SETREDRAW
// ============================================================================
// CSetRedraw - RAII wrapper for WM_SETREDRAW with redraw optimization
// ============================================================================
// Manages WM_SETREDRAW state with property-based tracking and hooks
// NtUserRedrawWindow to cache results for the same window.
// ============================================================================
class CSetRedraw
{
	static TCHAR const* key()
	{
		return _T("Redraw.{E8F1F2FD-7AD9-4AC6-88F9-59CE0F0BB173}");
	}

	struct Hooked
	{
		typedef Hooked hook_type;
		HWND prev_hwnd;
		BOOL prev_result;

		struct : hook_detail::thread_hook_swap<HOOK_TYPE(NtUserRedrawWindow)>
		{
			BOOL operator()(HWND hWnd, CONST RECT* lprcUpdate,
				HRGN hrgnUpdate, UINT flags) override
			{
				BOOL result;
				hook_type* const self = CONTAINING_RECORD(this, hook_type,
					HOOK_CONCAT(hook_, NtUserRedrawWindow));
				if (!self || self->prev_hwnd != hWnd)
				{
					result = this->hook_base_type::operator()(hWnd, lprcUpdate,
						hrgnUpdate, flags);
					if (self)
					{
						self->prev_result = result;
						self->prev_hwnd = hWnd;
					}
				}
				else
				{
					result = self->prev_result;
				}
				return result;
			}
		} HOOK_CONCAT(hook_, NtUserRedrawWindow);

		Hooked() : prev_hwnd(), prev_result() {}
		~Hooked() {}
	} HOOK_CONCAT(hook_, NtUserProp);

public:
	HWND hWnd;
	HANDLE notPrev;

	CSetRedraw(HWND const hWnd, BOOL redraw)
		: hWnd(hWnd), notPrev(GetProp(hWnd, key()))
	{
		SendMessage(hWnd, WM_SETREDRAW, redraw, 0);
		SetProp(hWnd, key(), (HANDLE)(!redraw));
	}

	~CSetRedraw()
	{
		SetProp(hWnd, key(), notPrev);
		SendMessage(hWnd, WM_SETREDRAW, !this->notPrev, 0);
	}
};
#endif // WM_SETREDRAW

} // namespace gui
} // namespace uffs

