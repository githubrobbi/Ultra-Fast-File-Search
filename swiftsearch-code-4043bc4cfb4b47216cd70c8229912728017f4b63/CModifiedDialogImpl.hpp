#pragma once

#include "CDlgTemplate.hpp"
#include <wingdi.h>
#include <vector>
#pragma comment(lib, "Gdi32.lib")

template<class T, class TBase = ATL::CWindow>
class CModifiedDialogImpl : public ATL::CDialogImpl<T, TBase>
{
	bool useSystemFont, rtl;
public:
	CModifiedDialogImpl(bool useSystemFont = true, bool const rtl = false) : useSystemFont(useSystemFont), rtl(rtl) { }

	INT_PTR DoModal(HWND hWndParent = ::GetActiveWindow(), LPARAM dwInitParam = NULL)
	{
		T *const me = static_cast<T *>(this);
		std::vector<BYTE> data(me->GetDlgTemplate(NULL, 0));
		LPDLGTEMPLATE pTemplate = static_cast<LPDLGTEMPLATE>(static_cast<PVOID>(data.empty() ? NULL : &data[0]));
		DWORD cb = me->GetDlgTemplate(pTemplate, static_cast<DWORD>(data.size()));
		if (cb > 0 && pTemplate != NULL)
		{
			BOOL result;
			ATLASSERT(m_hWnd == NULL);

			// Allocate the thunk structure here, where we can fail gracefully.

			result = this->m_thunk.Init(NULL,NULL);
			if (result == FALSE)
			{
				SetLastError(ERROR_OUTOFMEMORY);
				return -1;
			}

			__if_exists(_Module) { _Module.AddCreateWndData(&this->m_thunk.cd, (CDialogImplBaseT< TBase >*)this); }
			__if_exists(_AtlWinModule) { _AtlWinModule.AddCreateWndData(&this->m_thunk.cd, (CDialogImplBaseT< TBase >*)this); }
#ifdef _DEBUG
			m_bModal = true;
#endif //_DEBUG
			HINSTANCE hInstance = GetModuleHandle(NULL);
			return ::DialogBoxIndirectParam(hInstance, pTemplate, hWndParent, T::StartDialogProc, dwInitParam);
		}
		else { return this->CDialogImpl<T, TBase>::DoModal(hWndParent, dwInitParam); }
	}

	HWND Create(HWND hWndParent, LPARAM dwInitParam = NULL)
	{
		T *const me = static_cast<T *>(this);
		std::vector<BYTE> data(me->GetDlgTemplate(NULL, 0));
		LPDLGTEMPLATE pTemplate = static_cast<LPDLGTEMPLATE>(static_cast<PVOID>(data.empty() ? NULL : &data[0]));
		DWORD cb = me->GetDlgTemplate(pTemplate, static_cast<DWORD>(data.size()));
		if (cb > 0)
		{
			BOOL result;
			ATLASSERT(m_hWnd == NULL);

			// Allocate the thunk structure here, where we can fail gracefully.

			result = this->m_thunk.Init(NULL,NULL);
			if (result == FALSE)
			{
				SetLastError(ERROR_OUTOFMEMORY);
				return NULL;
			}

			_Module.AddCreateWndData(&this->m_thunk.cd, (CDialogImplBaseT< TBase >*)this);
#ifdef _DEBUG
			m_bModal = false;
#endif //_DEBUG
			return ::CreateDialogIndirectParam(_Module.GetResourceInstance(), pTemplate, hWndParent, T::StartDialogProc, dwInitParam);
		}
		else { return this->CDialogImpl<T, TBase>::Create(hWndParent, dwInitParam); }
	}

	virtual DWORD GetDlgTemplate(LPDLGTEMPLATE buffer, DWORD cbLen)
	{
		// Memory leak?
		CDlgTemplate dlgTemplate;
		if (!dlgTemplate.Load(MAKEINTRESOURCE(T::IDD)))
		{
			throw std::runtime_error("Could not find dialog resource!");
		}
		if (this->useSystemFont)
		{
			LOGFONT font;
			SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(font), &font, 0);
			// getting number of pixels per logical inch along the display height
			HDC hDC = ::GetDC(NULL);
			int nLPixY = GetDeviceCaps(hDC, LOGPIXELSY);
			::ReleaseDC(NULL, hDC);
			dlgTemplate.SetFont(font.lfFaceName, (WORD)(-MulDiv(font.lfHeight, 72, nLPixY)));
		}
		HGLOBAL hGlobal = dlgTemplate.Detach();
		ATLASSERT(hGlobal != NULL);
		LPCDLGTEMPLATE pTemplate = (LPCDLGTEMPLATE)LockResource(hGlobal);
		ATLASSERT(pTemplate != NULL);
		SIZE_T size = GlobalSize(hGlobal);
		memcpy(buffer, pTemplate, static_cast<SIZE_T>(cbLen) < size ? static_cast<SIZE_T>(cbLen) : size);
		if (cbLen >= sizeof(DWORD))
		{
			bool const exdialog = HIWORD(buffer->style) == 0xFFFF;
			DWORD *const exstyle = exdialog ? reinterpret_cast<DWORD *>(&buffer->cdit) : &buffer->dwExtendedStyle;
			if (cbLen >= static_cast<size_t>(reinterpret_cast<unsigned char const *>(exstyle + 1) - reinterpret_cast<unsigned char const *>(buffer)))
			{
				if (rtl)
				{
					*exstyle |= WS_EX_LAYOUTRTL;
				}
			}
		}
		GlobalFree(hGlobal);
		return static_cast<DWORD>(size);
	}
};

template<typename T>
class CInvokeImpl
{
	UINT WM_INVOKE_ASYNC, WM_INVOKE_SYNC;
	struct Thunk
	{
		virtual LRESULT operator()() { return 0; }
		virtual ~Thunk() { }
	};
protected:
	// Do NOT override this...
	LRESULT OnInvokeAsync(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
	{
		LRESULT result = FALSE;
		if (lParam == (LPARAM)this)
		{
			Thunk *pThunk = (Thunk *)wParam;
			__try { result = (*pThunk)(); }
			__finally { delete pThunk; }
			bHandled = TRUE;
		}
		else
		{
			bHandled = FALSE;
		}
		return result;
	}
	// Do NOT override this...
	LRESULT OnInvokeSync(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
	{
		LRESULT result = FALSE;
		if (lParam == (LPARAM)this)
		{
			Thunk *pThunk = (Thunk *)wParam;
			result = (*pThunk)();
			bHandled = TRUE;
		}
		else
		{
			bHandled = FALSE;
		}
		return result;
	}
public:
	CInvokeImpl()
	{
		LRESULT (CInvokeImpl<T>::*OnInvokeAsync)(UINT, WPARAM, LPARAM, BOOL &) = &CInvokeImpl::OnInvokeAsync;
		this->WM_INVOKE_ASYNC = WM_USER + static_cast<UINT>(reinterpret_cast<const ULONG_PTR &>(OnInvokeAsync) % (0x8000 - WM_USER - 3));
		this->WM_INVOKE_SYNC = WM_INVOKE_ASYNC + 1;
	}

	template<typename F>
	BOOL InvokeAsync(const F &f)
	{
		struct FThunk : public virtual Thunk
		{
			F f;
			FThunk(const F &f) : f(f) { }
			LRESULT operator()() { return (LRESULT)this->f(); }
		};
		FThunk *pThunk = new FThunk(f);
		if (::PostMessage(static_cast<T &>(*this), WM_INVOKE_ASYNC, (WPARAM)static_cast<Thunk *>(pThunk), (LPARAM)this))
		{
			return TRUE;
		}
		else
		{
			delete pThunk;
			return FALSE;
		}
	}

	template<typename F>
	LRESULT Invoke(const F &f)
	{
		struct FThunk : public virtual Thunk
		{
			F f;
			FThunk(const F &f) : f(f) { }
			LRESULT operator()() { return (LRESULT)this->f(); }
		};
		FThunk thunk(f);
		return ::SendMessage(static_cast<T &>(*this), WM_INVOKE_SYNC, (WPARAM)static_cast<Thunk *>(&thunk), (LPARAM)this);
	}

#pragma warning(suppress: 4555)
	BEGIN_MSG_MAP(CInvokeImpl)
		MESSAGE_HANDLER(WM_INVOKE_SYNC, OnInvokeSync)
		MESSAGE_HANDLER(WM_INVOKE_ASYNC, OnInvokeAsync)
	END_MSG_MAP()
};

