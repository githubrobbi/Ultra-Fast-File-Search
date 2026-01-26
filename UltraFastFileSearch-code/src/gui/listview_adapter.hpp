// ============================================================================
// listview_adapter.hpp - ListView adapter classes for column auto-sizing
// ============================================================================
// Extracted from UltraFastFileSearch.cpp during Wave 2 refactoring
// ============================================================================
#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <map>
#include <utility>
#include <vector>

#include <atlbase.h>
#include <atlapp.h>
#include <atlctrls.h>
#include <atlmisc.h>
#include <atlwin.h>

#include "../util/core_types.hpp"  // for std::tvstring

namespace uffs {
namespace gui {

// ============================================================================
// ImageListAdapter - Wrapper for WTL::CImageList
// ============================================================================
class ImageListAdapter
{
	WTL::CImageList me;
public:
	ImageListAdapter(WTL::CImageList const me) : me(me) {}

	ImageListAdapter* operator->() { return this; }
	ImageListAdapter const* operator->() const { return this; }

	int GetImageCount() const { return me.GetImageCount(); }

	bool GetSize(int const index, int& width, int& height) const
	{
		IMAGEINFO imginfo;
		bool const result = !!me.GetImageInfo(index, &imginfo);
		width = imginfo.rcImage.right - imginfo.rcImage.left;
		height = imginfo.rcImage.bottom - imginfo.rcImage.top;
		return result;
	}

	operator bool() const { return !!me; }
};

// ============================================================================
// ListViewAdapter - Wrapper for WTL::CListViewCtrl with text getter support
// ============================================================================
class ListViewAdapter
{
	WTL::CListViewCtrl* me;
	WTL::CString temp1;
public:
	typedef std::tvstring String;
	String temp2;
	typedef std::map<String, int> CachedColumnHeaderWidths;
	typedef ptrdiff_t text_getter_type(void* const me, int const item,
		int const subitem, String& result);
	CachedColumnHeaderWidths* cached_column_header_widths;
	text_getter_type* text_getter;

	class Freeze
	{
		Freeze(Freeze const&);
		Freeze& operator=(Freeze const&);
		ListViewAdapter* me;
	public:
		~Freeze() {}
		explicit Freeze(ListViewAdapter& me) : me(&me) {}
	};

	struct Column : HDITEM
	{
		String text;
		void SetMask(int const mask) { this->mask = mask; }
		String& GetText() { return text; }
	};

	struct Size : WTL::CSize
	{
		Size(WTL::CSize const& size) : WTL::CSize(size) {}
		int GetWidth() const { return this->cx; }
		int GetHeight() const { return this->cy; }
	};

	enum { LIST_MASK_TEXT = 0x0002 };

	struct ClientDC : WTL::CClientDC
	{
		HFONT old_font;
		ClientDC(ClientDC const&);
		ClientDC& operator=(ClientDC const&);

		~ClientDC()
		{
			this->old_font = this->SelectFont(this->old_font);
		}

		explicit ClientDC(HWND const wnd) : WTL::CClientDC(wnd), old_font()
		{
			this->old_font = this->SelectFont(static_cast<ATL::CWindow>(wnd).GetFont());
		}

		int GetTextWidth(String const& string) const
		{
			WTL::CSize sizeRect;
			this->GetTextExtent(string.data(), static_cast<int>(string.size()), &sizeRect);
			return sizeRect.cx;
		}
	};

	ListViewAdapter(WTL::CListViewCtrl& me,
		CachedColumnHeaderWidths& cached_column_header_widths,
		text_getter_type* const text_getter)
		: me(&me)
		, cached_column_header_widths(&cached_column_header_widths)
		, text_getter(text_getter)
	{}

	ListViewAdapter* operator->() { return this; }
	ListViewAdapter const* operator->() const { return this; }

	typedef ImageListAdapter ImageList;

	bool GetColumn(int const j, Column& col);
	int GetColumnCount() const { return me->GetHeader().GetItemCount(); }
	std::pair<int, int> GetVisibleItems() const;
	bool DeleteColumn(int const i) { return !!me->DeleteColumn(i); }
	int InsertColumn(int const i, String& text) { return me->InsertColumn(i, text.c_str()); }

	int GetColumnWidth(int const i) const
	{
		WTL::CRect rc;
		return me->GetHeader().GetItemRect(i, &rc) ? rc.Width() : 0;
	}

	int GetItemCount() const { return me->GetItemCount(); }
	String* GetItemText(int const item, int const subitem);

	Size GetClientSize() const
	{
		WTL::CRect rc;
		return me->GetClientRect(rc) ? rc.Size() : WTL::CSize();
	}

	ImageList GetImageList()
	{
		unsigned long const view = me->GetView();
		return ImageList(me->GetImageList(
			view == LV_VIEW_DETAILS || view == LV_VIEW_LIST || view == LV_VIEW_SMALLICON
				? LVSIL_SMALL : LVSIL_NORMAL));
	}

	bool SetColumnWidth(int const column, int const width)
	{
		return !!me->SetColumnWidth(column, width);
	}

	bool SetColumnText(int const column, String& text)
	{
		LVCOLUMN col = {};
		col.mask = LVCF_TEXT;
		col.pszText = const_cast<TCHAR*>(text.c_str());
		return !!me->SetColumn(column, &col);
	}

	operator HWND() const { return *me; }
};

// ============================================================================
// autosize_columns - Automatically size ListView columns based on content
// ============================================================================
void autosize_columns(ListViewAdapter list);

} // namespace gui
} // namespace uffs

