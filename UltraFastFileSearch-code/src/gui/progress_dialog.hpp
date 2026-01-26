#pragma once
// ============================================================================
// CProgressDialog - Progress Dialog for Long Operations
// ============================================================================
// Extracted from UltraFastFileSearch.cpp
// A WTL-based progress dialog with marquee support and cancel functionality.
// ============================================================================

#include "modified_dialog_impl.hpp"
#include "../util/temp_swap.hpp"

class CProgressDialog : private CModifiedDialogImpl<CProgressDialog>, private WTL::CDialogResize<CProgressDialog>
{
	typedef CProgressDialog This;
	typedef CModifiedDialogImpl<CProgressDialog> Base;
	friend CDialogResize<This>;
	friend CDialogImpl<This>;
	friend CModifiedDialogImpl<This>;
	enum
	{
		IDD = IDD_DIALOGPROGRESS, BACKGROUND_COLOR = COLOR_WINDOW
	};

	class CUnselectableWindow : public ATL::CWindowImpl<CUnselectableWindow>
	{
#pragma warning(suppress: 4555)
		BEGIN_MSG_MAP(CUnselectableWindow)
			MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)
		END_MSG_MAP()
		LRESULT OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL&)
		{
			LRESULT result = this->DefWindowProc(uMsg, wParam, lParam);
			return result == HTCLIENT ? HTTRANSPARENT : result;
		}
	};

	WTL::CButton btnPause, btnStop;
	CUnselectableWindow progressText;
	WTL::CProgressBarCtrl progressBar;
	bool canceled;
	bool invalidated;
	unsigned long long creationTime;
	unsigned long long lastUpdateTime;
	HWND parent;
	std::tstring lastProgressText, lastProgressTitle;
	bool windowCreated;
	bool windowCreateAttempted;
	bool windowShowAttempted;
	TempSwap<ATL::CWindow> setTopmostWindow;
	int lastProgress, lastProgressTotal;
	StringLoader LoadString;

	void OnDestroy()
	{
		setTopmostWindow.reset();
	}

	BOOL OnInitDialog(CWindow /*wndFocus*/, LPARAM /*lInitParam*/)
	{
		this->setTopmostWindow.reset(::topmostWindow, this->m_hWnd);
		(this->progressText.SubclassWindow)(this->GetDlgItem(IDC_RICHEDITPROGRESS));
		this->btnPause.Attach(this->GetDlgItem(IDRETRY));
		this->btnPause.SetWindowText(this->LoadString(IDS_BUTTON_PAUSE));
		this->btnStop.Attach(this->GetDlgItem(IDCANCEL));
		this->btnStop.SetWindowText(this->LoadString(IDS_BUTTON_STOP));
		this->progressBar.Attach(this->GetDlgItem(IDC_PROGRESS1));
		this->DlgResize_Init(false, false, 0);
		ATL::CComBSTR bstr;
		this->progressText.GetWindowText(&bstr);
		this->lastProgressText.assign(static_cast<LPCTSTR>(bstr), bstr ? bstr.Length() : 0);
		return TRUE;
	}

	void OnPause(UINT uNotifyCode, int nID, CWindow wndCtl)
	{
		UNREFERENCED_PARAMETER(uNotifyCode);
		UNREFERENCED_PARAMETER(nID);
		UNREFERENCED_PARAMETER(wndCtl);
		__debugbreak();
	}

	void OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl)
	{
		UNREFERENCED_PARAMETER(uNotifyCode);
		UNREFERENCED_PARAMETER(nID);
		UNREFERENCED_PARAMETER(wndCtl);
		PostQuitMessage(ERROR_CANCELLED);
	}

	BOOL OnEraseBkgnd(WTL::CDCHandle dc)
	{
		RECT rc = {};
		this->GetClientRect(&rc);
		dc.FillRect(&rc, BACKGROUND_COLOR);
		return TRUE;
	}

	HBRUSH OnCtlColorStatic(WTL::CDCHandle dc, WTL::CStatic /*wndStatic*/)
	{
		return GetSysColorBrush(BACKGROUND_COLOR);
	}

#pragma warning(suppress: 4555)
	BEGIN_MSG_MAP_EX(This)
		CHAIN_MSG_MAP(CDialogResize<This>)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDRETRY, BN_CLICKED, OnPause)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancel)
	END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(This)
		DLGRESIZE_CONTROL(IDC_PROGRESS1, DLSZ_MOVE_Y | DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_RICHEDITPROGRESS, DLSZ_SIZE_Y | DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()

	static BOOL EnableWindowRecursive(HWND hWnd, BOOL enable, BOOL includeSelf = true)
	{
		struct Callback
		{
			static BOOL CALLBACK EnableWindowRecursiveEnumProc(HWND hWnd, LPARAM lParam)
			{
				EnableWindowRecursive(hWnd, static_cast<BOOL>(lParam), TRUE);
				return TRUE;
			}
		};

		if (enable)
		{
			EnumChildWindows(hWnd, &Callback::EnableWindowRecursiveEnumProc, enable);
			return includeSelf && ::EnableWindow(hWnd, enable);
		}
		else
		{
			BOOL result = includeSelf && ::EnableWindow(hWnd, enable);
			EnumChildWindows(hWnd, &Callback::EnableWindowRecursiveEnumProc, enable);
			return result;
		}
	}

	unsigned long WaitMessageLoop(uintptr_t const handles[], size_t const nhandles)
	{
		for (;;)
		{
			unsigned long result;
			if (this->windowCreated)
			{
				result = MsgWaitForMultipleObjectsEx(static_cast<unsigned int>(nhandles), reinterpret_cast<HANDLE const*>(handles), UPDATE_INTERVAL, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
			}
			else
			{
				result = WaitForMultipleObjectsEx(static_cast<unsigned int>(nhandles), reinterpret_cast<HANDLE const*>(handles), FALSE, UPDATE_INTERVAL, FALSE);
			}

			if (result < WAIT_OBJECT_0 + nhandles || result == WAIT_TIMEOUT)
			{
				return result;
			}
			else if (result == WAIT_OBJECT_0 + static_cast<unsigned int>(nhandles))
			{
				this->ProcessMessages();
			}
			else
			{
				CppRaiseException(result == WAIT_FAILED ? GetLastError() : result);
			}
		}
	}

	DWORD GetMinDelay() const
	{
		return IsDebuggerPresent() ? 0 : 750;
	}

public:
	using Base::IsWindow;

	HWND GetHWND() const
	{
		return this->m_hWnd;
	}

	enum
	{
		UPDATE_INTERVAL = 1000 / 64
	};

	CProgressDialog(ATL::CWindow parent) : Base(true, !!(parent.GetExStyle() & WS_EX_LAYOUTRTL)), canceled(false), invalidated(false), creationTime(GetTickCount64()), lastUpdateTime(0), parent(parent), windowCreated(false), windowCreateAttempted(false), windowShowAttempted(false), lastProgress(0), lastProgressTotal(1) {}

	~CProgressDialog()
	{
		if (this->windowCreateAttempted)
		{
			EnableWindowRecursive(parent, TRUE);
		}

		if (this->windowCreated)
		{
			this->DestroyWindow();
		}
	}

	unsigned long long Elapsed(unsigned long long const now = GetTickCount64()) const
	{
		return now - this->lastUpdateTime;
	}

	bool ShouldUpdate(unsigned long long const now = GetTickCount64()) const
	{
		return this->Elapsed(now) >= UPDATE_INTERVAL;
	}

	void ProcessMessages()
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (!this->windowCreated || !this->IsDialogMessage(&msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			if (msg.message == WM_QUIT)
			{
				this->canceled = true;
			}
		}
	}

	void refresh_marquee()
	{
		bool const marquee = this->lastProgressTotal == 0;
		this->progressBar.ModifyStyle(marquee ? 0 : PBS_MARQUEE, marquee ? PBS_MARQUEE : 0, 0);
		this->progressBar.SetMarquee(marquee, UPDATE_INTERVAL);
	}

	void ForceShow()
	{
		if (!this->windowCreateAttempted)
		{
			this->windowCreated = !!this->Create(parent);
			this->windowCreateAttempted = true;
			EnableWindowRecursive(parent, FALSE);
			if (this->windowCreated)
			{
				this->windowShowAttempted = !!this->IsWindowVisible();
				this->refresh_marquee();
			}

			this->Flush();
		}

		if (!this->windowShowAttempted)
		{
			this->ShowWindow(SW_SHOW);
			this->windowShowAttempted = true;
		}
	}

	bool HasUserCancelled(unsigned long long const now = GetTickCount64())
	{
		bool justCreated = false;
		if (abs(static_cast<int>(now - this->creationTime)) >= static_cast<int>(this->GetMinDelay()))
		{
			this->ForceShow();
		}

		if (this->windowCreated && (justCreated || this->ShouldUpdate(now)))
		{
			this->ProcessMessages();
		}

		return this->canceled;
	}

	void Flush()
	{
		if (this->invalidated)
		{
			if (this->windowCreated)
			{
				this->invalidated = false;
				this->SetWindowText(this->lastProgressTitle.c_str());
				if (this->lastProgressTotal == 0)
				{
					this->progressBar.StepIt();
				}
				else
				{
					this->progressBar.SetRange32(0, this->lastProgressTotal);
					this->progressBar.SetPos(this->lastProgress);
				}

				this->progressText.SetWindowText(this->lastProgressText.c_str());
				this->progressBar.UpdateWindow();
				this->progressText.UpdateWindow();
				this->UpdateWindow();
			}

			this->lastUpdateTime = GetTickCount64();
		}
	}

	void SetProgress(long long current, long long total)
	{
		if (total > INT_MAX)
		{
			current = static_cast<long long>((static_cast<double>(current) / total) * INT_MAX);
			total = INT_MAX;
		}

		this->invalidated |= this->lastProgress != current || this->lastProgressTotal != total;
		bool const marquee_invalidated = this->invalidated && ((this->lastProgressTotal == 0) ^ (total == 0));
		this->lastProgressTotal = static_cast<int>(total);
		this->lastProgress = static_cast<int>(current);
		if (marquee_invalidated)
		{
			this->refresh_marquee();
		}
	}

	void SetProgressTitle(LPCTSTR title)
	{
		this->invalidated |= this->lastProgressTitle != title;
		this->lastProgressTitle = title;
	}

	void SetProgressText(std::tvstring const& text)
	{
		this->invalidated |= this->lastProgressText.size() != text.size() || !std::equal(this->lastProgressText.begin(), this->lastProgressText.end(), text.begin());
		this->lastProgressText.assign(text.begin(), text.end());
	}

	void SetProgressText(std::tstring const& text)
	{
		this->invalidated |= this->lastProgressText.size() != text.size() || !std::equal(this->lastProgressText.begin(), this->lastProgressText.end(), text.begin());
		this->lastProgressText.assign(text.begin(), text.end());
	}
};

