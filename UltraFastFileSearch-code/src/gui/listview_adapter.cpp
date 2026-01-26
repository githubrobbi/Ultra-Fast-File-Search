// ============================================================================
// listview_adapter.cpp - ListView adapter implementation
// ============================================================================
// Extracted from UltraFastFileSearch.cpp during Wave 2 refactoring
// ============================================================================

#include "listview_adapter.hpp"

namespace uffs {
namespace gui {

// ============================================================================
// ListViewAdapter implementation
// ============================================================================

bool ListViewAdapter::GetColumn(int const j, Column& col)
{
	bool result = false;
	WTL::CHeaderCtrl header = me->GetHeader();
	if (col.text.empty())
	{
		col.text.resize(16);
	}

	for (;;)
	{
		col.pszText = col.text.empty() ? nullptr : &*col.text.begin();
		col.cchTextMax = static_cast<int>(col.text.size());
		if (!header.GetItem(j, &col))
		{
			break;
		}

		size_t n = 0;
		while (col.pszText && static_cast<int>(n) < col.cchTextMax && col.pszText[n])
		{
			++n;
		}

		if (n < static_cast<size_t>(col.cchTextMax))
		{
			col.text.assign(col.pszText, col.pszText + static_cast<ptrdiff_t>(n));
			result = true;
			break;
		}

		n = n ? n * 2 : 1;
		if (n < col.text.capacity())
		{
			n = col.text.capacity();
		}

		col.text.resize(n);
	}

	if (!result)
	{
		col.text.clear();
	}

	return !!result;
}

std::pair<int, int> ListViewAdapter::GetVisibleItems() const
{
	std::pair<int, int> result(0, me->GetItemCount());
	WTL::CRect rc;
	if (me->GetClientRect(&rc))
	{
		result.first = me->GetTopIndex();
		WTL::CRect rcitem;
		for (int i = result.first; i < result.second; ++i)
		{
			if (me->GetItemRect(i, &rcitem, LVIR_BOUNDS) && !rcitem.IntersectRect(&rcitem, &rc))
			{
				result.second = i;
				break;
			}
		}
	}

	return result;
}

ListViewAdapter::String* ListViewAdapter::GetItemText(int const item, int const subitem)
{
	bool success = false;
	if (text_getter)
	{
		success = text_getter(me, item, subitem, temp2) >= 0;
	}
	else
	{
		if (me->GetItemText(item, subitem, temp1) >= 0)
		{
			success = true;
			temp2 = temp1;
		}
	}

	if (!success)
	{
		temp2.clear();
	}

	return success ? &temp2 : nullptr;
}

// ============================================================================
// autosize_columns implementation
// ============================================================================

void autosize_columns(ListViewAdapter list)
{
	// Autosize columns
	size_t const m = static_cast<size_t>(list->GetColumnCount());
	size_t const n = static_cast<size_t>(list->GetItemCount());
	if (m && n)
	{
		{
			ListViewAdapter::String empty, text;
			int itemp1 = -1;
			int itemp2 = -1;
			for (int i = 0; i < static_cast<int>(m); ++i)
			{
				ListViewAdapter::Column col;
				col.SetMask(ListViewAdapter::LIST_MASK_TEXT);
				if (list->GetColumn(static_cast<int>(i), col))
				{
					text = col.GetText();
				}
				else
				{
					text.clear();
				}

				if (list->cached_column_header_widths->find(text) ==
					list->cached_column_header_widths->end())
				{
					if (itemp1 < 0)
					{
						itemp1 = static_cast<int>(list->InsertColumn(static_cast<int>(m), empty));
					}

					if (itemp2 < 0)
					{
						itemp2 = static_cast<int>(list->InsertColumn(static_cast<int>(m), empty));
					}

					int cx = -1;
					if (text.empty())
					{
						cx = 0;
					}
					else
					{
						int const old_cx = list->GetColumnWidth(static_cast<int>(i));
						list->SetColumnText(static_cast<int>(m), text);
						if (list->SetColumnWidth(static_cast<int>(itemp2), -2 /*AUTOSIZE_USEHEADER*/))
						{
							cx = list->GetColumnWidth(static_cast<int>(itemp2));
						}
						list->SetColumnWidth(static_cast<int>(i), old_cx);
					}

					if (cx > 0 || (cx == 0 && text.empty()))
					{
						(*list->cached_column_header_widths)[text] = cx;
					}
				}
			}

			if (itemp2 >= 0)
			{
				list->DeleteColumn(itemp2);
			}

			if (itemp1 >= 0)
			{
				list->DeleteColumn(itemp1);
			}
		}

		std::vector<std::pair<std::pair<int /*priority*/, int /*width*/>, int /*column*/>> sizes;
		std::vector<int> max_column_widths(m, 0);
		std::vector<int> column_slacks(m, 0);
		std::vector<int> column_widths(m, 0);
		{
			std::pair<int, int> visible = list->GetVisibleItems();
			int const feather = 1;
			visible.first = visible.first >= feather ? visible.first - feather : 0;
			visible.second = visible.second + static_cast<ptrdiff_t>(feather) <=
				static_cast<ptrdiff_t>(n) ? visible.second + feather : static_cast<int>(n);
			int cximg = 0;
			if (ListViewAdapter::ImageList imagelist = list->GetImageList())
			{
				int cyimg;
				if (!(imagelist->GetImageCount() > 0 && imagelist->GetSize(visible.first, cximg, cyimg)))
				{
					cximg = 0;
				}
			}

			sizes.reserve(static_cast<size_t>(n * m));
			ListViewAdapter::ClientDC dc(list);
			wchar_t const breaks[] = { L'\u200B', L'\u200C', L'\u200D' };

			for (size_t j = 0; j != m; ++j)
			{
				{
					ListViewAdapter::Column col;
					col.SetMask(ListViewAdapter::LIST_MASK_TEXT);
					int cxheader;
					if (list->GetColumn(static_cast<int>(j), col))
					{
						ListViewAdapter::String text(col.GetText());
						int const cx_full = dc.GetTextWidth(text);
						ListViewAdapter::CachedColumnHeaderWidths::const_iterator const found =
							list->cached_column_header_widths->find(text);
						size_t ibreak = std::find_first_of(text.begin(), text.end(),
							&breaks[0], &breaks[sizeof(breaks) / sizeof(*breaks)]) - text.begin();
						if (ibreak < text.size())
						{
							text.erase(text.begin() + static_cast<ptrdiff_t>(ibreak), text.end());
							for (int k = 0; k != 3; ++k)
							{
								text.push_back('.');
							}
						}

						int cx = dc.GetTextWidth(text);
						if (found != list->cached_column_header_widths->end())
						{
							cxheader = found->second;
							int const slack = cx_full - cx;
							column_slacks[j] = cxheader - cx_full;
							using std::max;
							cx = max(cxheader - slack, 0);
						}
						else
						{
							cxheader = 0;
							cx += 22;
						}

						column_widths[j] = cx;
					}
					else
					{
						cxheader = 0;
					}

					sizes.push_back(std::make_pair(std::make_pair(SHRT_MAX, cxheader), static_cast<int>(j)));
				}

				for (size_t i = visible.first >= 0 ? static_cast<size_t>(visible.first) : 0;
					 i != (visible.second >= 0 ? static_cast<size_t>(visible.second) : 0); ++i)
				{
					int cx = 0;
					if (ListViewAdapter::String const* const text =
						list->GetItemText(static_cast<long>(i), static_cast<long>(j)))
					{
						cx = (j == 0 ? cximg : 0) + dc.GetTextWidth(*text) +
							column_slacks[static_cast<size_t>(j)];
					}

					sizes.push_back(std::make_pair(std::make_pair(0, cx), static_cast<int>(j)));
				}
			}
		}

		for (auto const& entry : sizes)
		{
			size_t const c = static_cast<size_t>(entry.second);
			if (entry.first.first < SHRT_MAX)
			{
				using std::max;
				max_column_widths[c] = max(max_column_widths[c], entry.first.second);
			}
		}

		int remaining = list->GetClientSize().GetWidth();
		{
			using std::min;
			remaining -= min(remaining, 0);
		}

		for (const auto& width : column_widths)
		{
			remaining -= width;
		}

		std::sort(sizes.begin(), sizes.end());
		for (size_t i = 0; i != sizes.size() && remaining > 0; ++i)
		{
			int const item_width = sizes[i].first.second;
			size_t const column = static_cast<size_t>(sizes[i].second);
			int const old_column_width = column_widths[column];
			if (old_column_width < item_width)
			{
				using std::min;
				int const diff = min(item_width - old_column_width, remaining);
				assert(diff >= 0);
				column_widths[column] += diff;
				remaining -= diff;
			}
		}

		for (size_t i = column_widths.size(); i > 0 && ((void)--i, true);)
		{
			list->SetColumnWidth(static_cast<int>(i), column_widths[i]);
		}
	}
	else if (false)
	{
		for (size_t i = m; i > 0 && ((void)--i, true);)
		{
			list->SetColumnWidth(static_cast<int>(i), -1 /*AUTOSIZE*/);
		}
	}
}

} // namespace gui
} // namespace uffs
