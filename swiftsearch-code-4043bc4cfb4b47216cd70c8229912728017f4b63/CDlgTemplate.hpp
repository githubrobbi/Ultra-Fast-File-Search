#pragma once

#include <windows.h>

class CDlgTemplate
{
private:
	HGLOBAL m_hTemplate;
	DWORD m_dwTemplateSize;
	BOOL m_bSystemFont;

#pragma pack(push, 1)
	struct DLGTEMPLATEEX
	{
		WORD dlgVer;
		WORD signature;
		DWORD helpID;
		DWORD exStyle;
		DWORD style;
		WORD cDlgItems;
		short x;
		short y;
		short cx;
		short cy;

		// Everything else in this structure is variable length,
		// and therefore must be determined dynamically

		// sz_Or_Ord menu;			// name or ordinal of a menu resource
		// sz_Or_Ord windowClass;	// name or ordinal of a window class
		// WCHAR title[titleLen];	// title string of the dialog box
		// short pointsize;			// only if DS_SETFONT is set
		// short weight;			// only if DS_SETFONT is set
		// short bItalic;			// only if DS_SETFONT is set
		// WCHAR font[fontLen];		// typeface name, if DS_SETFONT is set
	};

	struct DLGITEMTEMPLATEEX
	{
		DWORD helpID;
		DWORD exStyle;
		DWORD style;
		short x;
		short y;
		short cx;
		short cy;
		DWORD id;

		// Everything else in this structure is variable length,
		// and therefore must be determined dynamically

		// sz_Or_Ord windowClass;	// name or ordinal of a window class
		// sz_Or_Ord title;			// title string or ordinal of a resource
		// WORD extraCount;			// bytes following creation data
	};
#pragma pack(pop)

	static void __stdcall _AfxConvertDialogUnitsToPixels(LPCTSTR pszFontFace, WORD wFontSize, int cxDlg, int cyDlg, SIZE* pSizePixel)
	{
		// Attempt to create the font to be used in the dialog box
		UINT cxSysChar, cySysChar;
		LOGFONT lf;
		HDC hDC = GetDC(NULL);
		memset(&lf, 0, sizeof(LOGFONT));
		lf.lfHeight = -MulDiv(wFontSize, GetDeviceCaps(hDC, LOGPIXELSY), 72);
		lf.lfWeight = FW_NORMAL;
		lf.lfCharSet = DEFAULT_CHARSET;
		_tcsncpy(lf.lfFaceName, pszFontFace, LF_FACESIZE);

		HFONT hNewFont = CreateFontIndirect(&lf);
		if (hNewFont != NULL)
		{
			HFONT hFontOld = (HFONT)SelectObject(hDC, hNewFont);
			TEXTMETRIC tm;
			GetTextMetrics(hDC, &tm);
			cySysChar = tm.tmHeight + tm.tmExternalLeading;
			SIZE size;
			GetTextExtentPoint32(hDC, _T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"), 52, &size);
			cxSysChar = (size.cx + 26) / 52;
			SelectObject(hDC, hFontOld);
			DeleteObject(hNewFont);
		}
		else
		{
			// Could not create the font so just use the system's values
			cxSysChar = LOWORD(GetDialogBaseUnits());
			cySysChar = HIWORD(GetDialogBaseUnits());
		}
		ReleaseDC(NULL, hDC);

		// Translate dialog units to pixels
		pSizePixel->cx = MulDiv(cxDlg, cxSysChar, 4);
		pSizePixel->cy = MulDiv(cyDlg, cySysChar, 8);
	}

	
public:
	/////////////////////////////////////////////////////////////////////////////
	// IsDialogEx

	static inline BOOL IsDialogEx(const DLGTEMPLATE* pTemplate)
	{
		return ((DLGTEMPLATEEX*)pTemplate)->signature == 0xFFFF;
	}

	/////////////////////////////////////////////////////////////////////////////
	// HasFont

	static inline BOOL HasFont(const DLGTEMPLATE* pTemplate)
	{
		return (DS_SETFONT &
			(IsDialogEx(pTemplate) ? ((DLGTEMPLATEEX*)pTemplate)->style :
			pTemplate->style));
	}

	/////////////////////////////////////////////////////////////////////////////
	// FontAttrSize

	static inline int FontAttrSize(BOOL bDialogEx)
	{
		return (int)sizeof(WORD) * (bDialogEx ? 3 : 1);
	}

	/////////////////////////////////////////////////////////////////////////////
	// CDlgTemplate - implementation class

	CDlgTemplate(const DLGTEMPLATE* pTemplate = NULL)
	{
		if (pTemplate == NULL)
		{
			m_hTemplate = NULL;
			m_dwTemplateSize = 0;
			m_bSystemFont = FALSE;
		}
		else
		{
			BOOL bSet=SetTemplate(pTemplate, GetTemplateSize(pTemplate));
			if(!bSet)
			{
				//AfxThrowMemoryException();
			}
		}
	}

	CDlgTemplate(HGLOBAL hTemplate)
	{
		if (hTemplate == NULL)
		{
			m_hTemplate = NULL;
			m_dwTemplateSize = 0;
			m_bSystemFont = FALSE;
		}
		else
		{
			DLGTEMPLATE* pTemplate = (DLGTEMPLATE*)GlobalLock(hTemplate);
			BOOL bSet=SetTemplate(pTemplate, GetTemplateSize(pTemplate));
			GlobalUnlock(hTemplate);
			if(!bSet)
			{
				//AfxThrowMemoryException();
			}
		}
	}

	BOOL SetTemplate(const DLGTEMPLATE* pTemplate, UINT cb)
	{
		m_dwTemplateSize = cb;
		SIZE_T nAllocSize=m_dwTemplateSize + LF_FACESIZE * 2;
		if (nAllocSize < m_dwTemplateSize)
		{
			return FALSE;
		}
		if ((m_hTemplate = GlobalAlloc(GPTR,nAllocSize)) == NULL)
		{
			return FALSE;
		}
		DLGTEMPLATE* pNew = (DLGTEMPLATE*)GlobalLock(m_hTemplate);
		memcpy((BYTE*)pNew, pTemplate, (size_t)m_dwTemplateSize);

		m_bSystemFont = (ATL::_DialogSizeHelper::HasFont(pNew) == 0);

		GlobalUnlock(m_hTemplate);
		return TRUE;
	}

	~CDlgTemplate()
	{
		if (m_hTemplate != NULL)
			GlobalFree(m_hTemplate);
	}

	BOOL Load(LPCTSTR lpDialogTemplateID)
	{
		HINSTANCE hInst = /*_Module.GetResourceInstance()*/ GetModuleHandle(NULL);
		if (hInst == NULL)
		{
			return FALSE;
		}
		HRSRC hRsrc = FindResource(hInst, lpDialogTemplateID, RT_DIALOG);
		if (hRsrc == NULL)
		{
			return FALSE;
		}
		HGLOBAL hTemplate = LoadResource(hInst, hRsrc);
		DLGTEMPLATE* pTemplate = (DLGTEMPLATE*)LockResource(hTemplate);
		BOOL bSet = SetTemplate(pTemplate, (UINT)SizeofResource(hInst, hRsrc));
#ifndef UnlockResource  // macro generates warning on Clang
		UnlockResource(hTemplate);  // just for documentation, but not necessary
#endif
		FreeResource(hTemplate);
		return bSet;
	}

	HGLOBAL Detach()
	{
		HGLOBAL hTmp = m_hTemplate;
		m_hTemplate = NULL;
		return hTmp;
	}

	BOOL HasFont() const
	{
		DLGTEMPLATE* pTemplate = (DLGTEMPLATE*)GlobalLock(m_hTemplate);
		BOOL bHasFont = ATL::_DialogSizeHelper::HasFont(pTemplate);
		GlobalUnlock(m_hTemplate);
		return bHasFont;
	}

	static inline WCHAR* _SkipString(__in WCHAR* p)
	{
		while (*p++);
		return p;
	}

	static BYTE* GetFontSizeField(const DLGTEMPLATE* pTemplate)
	{
		BOOL bDialogEx = IsDialogEx(pTemplate);
		WORD* pw;

		if (bDialogEx)
			pw = (WORD*)((DLGTEMPLATEEX*)pTemplate + 1);
		else
			pw = (WORD*)(pTemplate + 1);

		if (*pw == (WORD)-1)        // Skip menu name string or ordinal
			pw += 2; // WORDs
		else
			while(*pw++);

		if (*pw == (WORD)-1)        // Skip class name string or ordinal
			pw += 2; // WORDs
		else
			while(*pw++);

		while (*pw++);          // Skip caption string

		return (BYTE*)pw;
	}

	static UINT GetTemplateSize(const DLGTEMPLATE* pTemplate)
	{
		BOOL bDialogEx = IsDialogEx(pTemplate);
		BYTE* pb = GetFontSizeField(pTemplate);

		if (ATL::_DialogSizeHelper::HasFont(pTemplate))
		{
			// Skip font size and name
			pb += FontAttrSize(bDialogEx);  // Skip font size, weight, (italic, charset)
			pb += 2 * (wcslen((WCHAR*)pb) + 1);
		}

		WORD nCtrl = bDialogEx ? (WORD)((DLGTEMPLATEEX*)pTemplate)->cDlgItems :
			(WORD)pTemplate->cdit;

		while (nCtrl > 0)
		{
			pb = (BYTE*)(((DWORD_PTR)pb + 3) & ~DWORD_PTR(3)); // DWORD align

			pb += (bDialogEx ? sizeof(DLGITEMTEMPLATEEX) : sizeof(DLGITEMTEMPLATE));

			if (*(WORD*)pb == (WORD)-1)     // Skip class name string or ordinal
				pb += 2 * sizeof(WORD);
			else
				pb = (BYTE*)_SkipString((WCHAR*)pb);

			if (*(WORD*)pb == (WORD)-1)     // Skip text string or ordinal
				pb += 2 * sizeof(WORD);
			else
				pb = (BYTE*)_SkipString((WCHAR*)pb);

			WORD cbExtra = *(WORD*)pb;      // Skip extra data
			if (cbExtra != 0 && !bDialogEx)
				cbExtra -= 2;
			pb += sizeof(WORD) + cbExtra;
			--nCtrl;
		}

		return UINT(pb - (BYTE*)pTemplate);
	}

	static std::basic_string<TCHAR> GetFont(const DLGTEMPLATE* pTemplate, WORD& nFontSize)
	{
		ATLASSERT(pTemplate != NULL);

		if (!ATL::_DialogSizeHelper::HasFont(pTemplate))
			return FALSE;

		BYTE* pb = GetFontSizeField(pTemplate);
		nFontSize = *(WORD*)pb;
		pb += FontAttrSize(IsDialogEx(pTemplate));

		std::basic_string<TCHAR> strFace;
#if defined(_UNICODE)
		// Copy font name
		strFace = (LPCTSTR)pb;
#else
		// Convert Unicode font name to MBCS
		strFace.resize(LF_FACESIZE + 1);
		strFace.resize(static_cast<size_t>(WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)pb, -1, &strFace[0], LF_FACESIZE, NULL, NULL));
		while (!strFace.empty() && strFace.back() == _T('\0')) { strFace.resize(strFace.size() - 1); }
#endif

		return strFace;
	}

	std::basic_string<TCHAR> GetFont(WORD& nFontSize) const
	{
		ATLASSERT(m_hTemplate != NULL);

		std::basic_string<TCHAR> result = GetFont((DLGTEMPLATE*)GlobalLock(m_hTemplate), nFontSize);
		GlobalUnlock(m_hTemplate);
		return result;
	}

	BOOL SetFont(LPCTSTR lpFaceName, WORD nFontSize)
	{
		ATLASSERT(m_hTemplate != NULL);

		if (m_dwTemplateSize == 0)
			return FALSE;

		DLGTEMPLATE* pTemplate = (DLGTEMPLATE*)GlobalLock(m_hTemplate);

		BOOL bDialogEx = IsDialogEx(pTemplate);
		BOOL bHasFont = ATL::_DialogSizeHelper::HasFont(pTemplate);
		int cbFontAttr = FontAttrSize(bDialogEx);

		if (bDialogEx)
			((DLGTEMPLATEEX*)pTemplate)->style |= DS_SETFONT;
		else
			pTemplate->style |= DS_SETFONT;

		int nFaceNameLen = lstrlen(lpFaceName);
		if( nFaceNameLen >= LF_FACESIZE )
		{
			// Name too long
			return FALSE;
		}

#ifdef _UNICODE
		int cbNew = cbFontAttr + ((nFaceNameLen + 1) * sizeof(TCHAR));
		BYTE* pbNew = (BYTE*)lpFaceName;
#else
		WCHAR wszFaceName [LF_FACESIZE];
		int cbNew = cbFontAttr + 2 * MultiByteToWideChar(CP_ACP, 0, lpFaceName, -1, wszFaceName, LF_FACESIZE);
		BYTE* pbNew = (BYTE*)wszFaceName;
#endif
		if (cbNew < cbFontAttr)
		{
			return FALSE;
		}
		BYTE* pb = GetFontSizeField(pTemplate);
		int cbOld = (int)(bHasFont ? cbFontAttr + 2 * (wcslen((WCHAR*)(pb + cbFontAttr)) + 1) : 0);

		BYTE* pOldControls = (BYTE*)(((DWORD_PTR)pb + cbOld + 3) & ~DWORD_PTR(3));
		BYTE* pNewControls = (BYTE*)(((DWORD_PTR)pb + cbNew + 3) & ~DWORD_PTR(3));

		WORD nCtrl = bDialogEx ? (WORD)((DLGTEMPLATEEX*)pTemplate)->cDlgItems :
			(WORD)pTemplate->cdit;

		if (cbNew != cbOld && nCtrl > 0)
		{
			size_t nBuffLeftSize=(size_t)(m_dwTemplateSize - (pOldControls - (BYTE*)pTemplate));
			if (nBuffLeftSize > m_dwTemplateSize)
			{
				return FALSE;
			}
			memmove(pNewControls, pOldControls, nBuffLeftSize);
		}

		*(WORD*)pb = nFontSize;
		memmove(pb + cbFontAttr, pbNew, cbNew - cbFontAttr);

		m_dwTemplateSize += ULONG(pNewControls - pOldControls);

		GlobalUnlock(m_hTemplate);
		m_bSystemFont = FALSE;
		return TRUE;
	}

	BOOL SetSystemFont(WORD wSize = 0)
	{
		LOGFONT lf;
		LPCTSTR pszFace = _T("System");
		WORD wDefSize = 10;
		HFONT hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
		if (hFont == NULL)
			hFont = (HFONT)::GetStockObject(SYSTEM_FONT);
		if (hFont != NULL)
		{
			if (::GetObject(hFont, sizeof(LOGFONT), &lf) != 0)
			{
				pszFace = lf.lfFaceName;
				HDC hDC = GetDC(NULL);
				if (lf.lfHeight < 0)
					lf.lfHeight = -lf.lfHeight;
				wDefSize = (WORD)MulDiv(lf.lfHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY));
				ReleaseDC(NULL, hDC);
			}
		}

		if (wSize == 0)
			wSize = wDefSize;

		return SetFont(pszFace, wSize);
	}

	void GetSizeInDialogUnits(SIZE* pSize) const
	{
		ATLASSERT(m_hTemplate != NULL);
		//ASSERT_POINTER(pSize, SIZE);

		DLGTEMPLATE* pTemplate = (DLGTEMPLATE*)GlobalLock(m_hTemplate);

		if (IsDialogEx(pTemplate))
		{
			pSize->cx = ((DLGTEMPLATEEX*)pTemplate)->cx;
			pSize->cy = ((DLGTEMPLATEEX*)pTemplate)->cy;
		}
		else
		{
			pSize->cx = pTemplate->cx;
			pSize->cy = pTemplate->cy;
		}

		GlobalUnlock(m_hTemplate);
	}

	void GetSizeInPixels(SIZE* pSize) const
	{
		ATLASSERT(m_hTemplate != NULL);
		//ASSERT_POINTER(pSize, SIZE);

		if (m_bSystemFont)
		{
			GetSizeInDialogUnits(pSize);
			DWORD dwDLU = GetDialogBaseUnits();
			pSize->cx = (pSize->cx * LOWORD(dwDLU)) / 4;
			pSize->cy = (pSize->cy * HIWORD(dwDLU)) / 8;
		}
		else
		{
			WORD wSize = 10;
			SIZE size;
			GetSizeInDialogUnits(&size);
			_AfxConvertDialogUnitsToPixels(GetFont(wSize).c_str(), wSize, size.cx, size.cy, pSize);
		}
	}
};
