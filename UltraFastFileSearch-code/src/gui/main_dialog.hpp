#pragma once

// Extracted components
#include "search_pattern_edit.hpp"
#include "listview_columns.hpp"
#include "file_attribute_colors.hpp"
#include "icon_cache_types.hpp"
#include "../util/time_utils.hpp"

class CMainDlg : public CModifiedDialogImpl < CMainDlg>, public WTL::CDialogResize < CMainDlg>, public CInvokeImpl < CMainDlg>, private WTL::CMessageFilter
{
	enum
	{
		IDC_STATUS_BAR = 1100 + 0
	};

	// Column indices are now in listview_columns.hpp (uffs::ListViewColumn enum)
	// Icon cache types are now in icon_cache_types.hpp (CacheInfo, ShellInfoCache, TypeInfoCache)

#ifndef LVN_INCREMENTALSEARCH
		enum
	{
		LVN_INCREMENTALSEARCH =
#ifdef UNICODE
		LVN_FIRST - 63
#else
			LVN_FIRST - 62
#endif
	};
#endif
		struct CThemedListViewCtrl : public WTL::CListViewCtrl, public WTL::CThemeImpl < CThemedListViewCtrl>
	{
		using WTL::CListViewCtrl::Attach;
	};

	static unsigned int WM_TASKBARCREATED()
	{
		static unsigned int result = 0;
		if (!result)
		{
			result = RegisterWindowMessage(_T("TaskbarCreated"));
		}

		return result;
	}

	enum
	{
		WM_NOTIFYICON = WM_USER + 100, WM_READY = WM_NOTIFYICON + 1, WM_REFRESHITEMS = WM_READY + 1
	};

	// NOTE: NameComparator template was removed - it was dead code that referenced
	// non-existent file_name()/stream_name() methods on SearchResult

	CSearchPattern txtPattern;
	WTL::CButton btnOK, btnBrowse;
	WTL::CRichEditCtrl richEdit;
	WTL::CStatusBarCtrl statusbar;
	WTL::CAccelerator accel;
	ShellInfoCache cache;
	std::vector<int> invalidated_rows;
	mutable TypeInfoCache type_cache /*based on file type ONLY; may be accessed concurrently */;
	mutable atomic_namespace::recursive_mutex type_cache_mutex;
	mutable std::tvstring type_cache_str;
	value_initialized<HANDLE> hRichEdit;
	value_initialized<bool> autocomplete_called;
	struct
	{
		value_initialized<int> column;
		value_initialized < unsigned char > variation;
		value_initialized < unsigned int > counter;
	}

	last_sort;
	Results results;
	WTL::CImageList _small_image_list;	// image list that is used as the "small" image list
	WTL::CImageList imgListSmall, imgListLarge, imgListExtraLarge;	// lists of small images
	WTL::CComboBox cmbDrive;
	value_initialized<int> indices_created;
	CThemedListViewCtrl lvFiles;
	ListViewAdapter::CachedColumnHeaderWidths cached_column_header_widths;
	intrusive_ptr<BackgroundWorker> iconLoader;
	Handle closing_event;
	OleIoCompletionPort iocp;
	value_initialized<bool> _initialized;
	NFormat nformat_ui, nformat_io;
	value_initialized < long long > time_zone_bias;
	LCID lcid;
	value_initialized<HANDLE> hWait, hEvent;
	OleInit oleinit;
	COLORREF deletedColor;
	COLORREF encryptedColor;
	COLORREF compressedColor;
	COLORREF directoryColor;
	COLORREF hiddenColor;
	COLORREF systemColor;
	WTL::CSize template_size;
	value_initialized<int> suppress_escapes;
	value_initialized<bool> trie_filtering;
	TempSwap<ATL::CWindow > setTopmostWindow;
	StringLoader stringLoader_;  // Renamed from LoadString to avoid Windows macro conflict
	static DWORD WINAPI SHOpenFolderAndSelectItemsThread(IN LPVOID lpParameter)
	{
		std::unique_ptr<std::pair<std::pair<CShellItemIDList, ATL::CComPtr<IShellFolder> >, std::vector< CShellItemIDList > >> p(static_cast<std::pair<std::pair<CShellItemIDList, ATL::CComPtr<IShellFolder> >, std::vector< CShellItemIDList > > *> (lpParameter));
		// This is in a separate thread because of a BUG:
		// Try this with RmMetadata:
		// 1. Double-click it.
		// 2. Press OK when the error comes up.
		// 3. Now you can't access the main window, because SHOpenFolderAndSelectItems() hasn't returned!
		// So we put this in a separate thread to solve that problem.

		CoInit coInit;
		std::vector<LPCITEMIDLIST> relative_item_ids(p->second.size());
		for (size_t i = 0; i < p->second.size(); ++i)
		{
			relative_item_ids[i] = ILFindChild(p->first.first, p->second[i]);
		}

		return SHOpenFolderAndSelectItems(p->first.first, static_cast<UINT> (relative_item_ids.size()), relative_item_ids.empty() ? nullptr : &relative_item_ids[0], 0);
	}

public:
	CMainDlg(HANDLE
		const hEvent, bool
		const rtl) :
		CModifiedDialogImpl<CMainDlg>(true, rtl),
		iconLoader(BackgroundWorker::create(true, global_exception_handler)),
		closing_event(CreateEvent(nullptr, TRUE, FALSE, nullptr)),
		nformat_ui(std::locale("")), nformat_io(std::locale()), lcid(GetThreadLocale()), hEvent(hEvent),
		deletedColor(RGB(0xC0, 0xC0, 0xC0)), encryptedColor(RGB(0, 0xFF, 0)), compressedColor(RGB(0, 0, 0xFF)), directoryColor(RGB(0xFF, 0x99, 0x33)), hiddenColor(RGB(0xFF, 0x99, 0x99)), systemColor(RGB(0xFF, 0, 0))
	{
		this->time_zone_bias = get_time_zone_bias();
	}

	// Wrapper that uses member variables for time zone and locale
	// The actual implementation is in time_utils.hpp (uffs::SystemTimeToString)
	void SystemTimeToString(long long system_time /*UTC */, std::tvstring& buffer, bool
		const sortable, bool
		const include_time = true) const
	{
		return uffs::SystemTimeToString(system_time, buffer, sortable, include_time, this->time_zone_bias, this->lcid);
	}

	void OnDestroy()
	{
		setTopmostWindow.reset();
		(void)UnregisterWait(this->hWait);
		this->ExitBackground();
		this->iconLoader->clear();
		this->iocp.close();
	}

	struct IconLoaderCallback
	{
		CMainDlg* this_;
		std::tvstring path;
		SIZE iconSmallSize, iconLargeSize;
		unsigned long fileAttributes;
		int iItem;

		struct MainThreadCallback
		{
			CMainDlg* this_;
			std::tvstring description, path;
			WTL::CIcon iconSmall, iconLarge;
			int iItem;
			BOOL success;
			TCHAR szTypeName[80];
			bool operator()()
			{
				WTL::CWaitCursor wait(true, IDC_APPSTARTING);
				std::reverse(path.begin(), path.end());
				auto cached = this_->cache.find(path);
				std::reverse(path.begin(), path.end());
				if (!success && cached != this_->cache.end())
				{
					this_->cache.erase(cached);
					cached = this_->cache.end();
				}

				if (cached != this_->cache.end())
				{
					_tcscpy(cached->second.szTypeName, this->szTypeName);
					cached->second.description = description;

					if (cached->second.iIconSmall < 0)
					{
						cached->second.iIconSmall = this_->imgListSmall.AddIcon(iconSmall);
					}
					else
					{
						this_->imgListSmall.ReplaceIcon(cached->second.iIconSmall, iconSmall);
					}

					if (cached->second.iIconLarge < 0)
					{
						cached->second.iIconLarge = this_->imgListLarge.AddIcon(iconLarge);
					}
					else
					{
						this_->imgListLarge.ReplaceIcon(cached->second.iIconLarge, iconLarge);
					}

					cached->second.valid = true;

					bool
						const should_post_message = this_->invalidated_rows.empty();
					this_->invalidated_rows.push_back(iItem);
					if (should_post_message)
					{
						this_->PostMessage(WM_REFRESHITEMS);
					}
				}

				return true;
			}
		};

		BOOL operator()()
		{
			RECT rcItem = { LVIR_BOUNDS
			};

			RECT rcFiles, intersection;
			this_->lvFiles.GetClientRect(&rcFiles);	// Blocks, but should be fast
			this_->lvFiles.GetItemRect(iItem, &rcItem, LVIR_BOUNDS);	// Blocks, but I'm hoping it's fast...
			bool
				const still_visible = !!IntersectRect(&intersection, &rcFiles, &rcItem);
			{
				std::tvstring normalizedPath = NormalizePath(path);
				SHFILEINFO shfi = { 0 };

				std::tvstring description; 
#if 0
					{
						std::vector<BYTE> buffer;
						DWORD temp;
						buffer.resize(GetFileVersionInfoSize(normalizedPath.c_str(), &temp));
						if (GetFileVersionInfo(normalizedPath.c_str(), nullptr, static_cast<DWORD> (buffer.size()), buffer.empty() ? nullptr : &buffer[0]))
						{
							LPVOID p;
							UINT uLen;
							if (VerQueryValue(buffer.empty() ? nullptr : &buffer[0], _T("\\StringFileInfo\\040904E4\\FileDescription"), &p, &uLen))
							{
								description = std::tvstring((LPCTSTR)p, uLen);
							}
						}
					}
#endif
						Handle fileTemp;	// To prevent icon retrieval from changing the file time
					SetLastError(0);
					WTL::CIcon iconSmall, iconLarge;
					bool
						const attempt = still_visible;
					BOOL success = FALSE;
					{
						for (int pass = 0; attempt && pass < 2; ++pass)
						{
							if (pass == 0 && fileTemp && fileTemp != INVALID_HANDLE_VALUE)
							{
								HANDLE
									const opened = CreateFile(path.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
								if (opened != INVALID_HANDLE_VALUE)
								{
									Handle(opened).swap(fileTemp);
									FILETIME preserve = { ULONG_MAX, ULONG_MAX
									};

									SetFileTime(fileTemp, nullptr, &preserve, nullptr);
								}
							}

							WTL::CSize
								const size = pass ? iconLargeSize : iconSmallSize;
							ULONG
								const flags = SHGFI_ICON | SHGFI_SHELLICONSIZE | SHGFI_ADDOVERLAYS |	//SHGFI_TYPENAME | SHGFI_SYSICONINDEX |
								(pass ? SHGFI_LARGEICON : SHGFI_SMALLICON);
							// CoInit com; 	// MANDATORY!  Some files, like '.sln' files, won't work without it!
							Wow64Disable
								const wow64disable(true);
							success = SHGetFileInfo(normalizedPath.c_str(), fileAttributes, &shfi, sizeof(shfi), flags) != 0;
							if (!success && (flags & SHGFI_USEFILEATTRIBUTES) == 0)
							{
								success = SHGetFileInfo(normalizedPath.c_str(), fileAttributes, &shfi, sizeof(shfi), flags | SHGFI_USEFILEATTRIBUTES) != 0;
							}

							(pass ? iconLarge : iconSmall).Attach(shfi.hIcon);
						}

						std::tvstring
							const path_copy(path);
						int
							const iItem(iItem);
						MainThreadCallback callback = { this_, description, path_copy, iconSmall.Detach(), iconLarge.Detach(), iItem, success
						};

						_tcscpy(callback.szTypeName, shfi.szTypeName);
						this_->InvokeAsync(callback);
						callback.iconLarge.Detach();
						callback.iconSmall.Detach();
					}

					if (!success && attempt)
					{
						_ftprintf(stderr, _T("Could not get the icon for file: %s\n"), normalizedPath.c_str());
					}
			}

			return true;
		}
	};

	int CacheIcon(std::tvstring path, int
		const iItem, ULONG fileAttributes, long
		const timestamp)
	{
		std::reverse(path.begin(), path.end());
		auto entry = this->cache.find(path);
		bool
			const already_in_cache = entry != this->cache.end();
		if (!already_in_cache)
		{
			WTL::CRect rcClient;
			this->lvFiles.GetClientRect(&rcClient);
			size_t max_possible_icons = MulDiv(rcClient.Width(), rcClient.Height(), GetSystemMetrics(SM_CXSMICON) * GetSystemMetrics(SM_CYSMICON));
			size_t current_cache_size = this->cache.size(), max_cache_size = 1 << 10;
			if (rcClient.Height() > 0 && max_cache_size < max_possible_icons)
			{
				max_cache_size = max_possible_icons;
			}

			if (current_cache_size >= 2 * max_cache_size)
			{
				for (auto i = this->cache.begin(); i != this->cache.end();)
				{
					if (i->second.counter + max_cache_size < current_cache_size)
					{
						this->cache.erase(i++);
					}
					else
					{
						i->second.counter -= current_cache_size - max_cache_size;
						++i;
					}
				}

				for (const auto& [key, info] : this->cache)
				{
					assert(info.counter < this->cache.size());
				}
			}

			entry = this->cache.insert(this->cache.end(), ShellInfoCache::value_type(path, CacheInfo(this->cache.size())));
			std::reverse(path.begin(), path.end());

			SIZE iconSmallSize;
			this->imgListSmall.GetIconSize(iconSmallSize);
			SIZE iconSmallLarge;
			this->imgListLarge.GetIconSize(iconSmallLarge);

			IconLoaderCallback callback = { this, path, iconSmallSize, iconSmallLarge, fileAttributes, iItem
			};

			this->iconLoader->add(callback, timestamp);
		}

		return entry->second.iIconSmall;
	}

	LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		return this->lvFiles.SendMessage(uMsg, wParam, lParam);
	}

	static VOID NTAPI WaitCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
	{
		HWND
			const hWnd = reinterpret_cast<HWND> (lpParameter);
		if (!TimerOrWaitFired)
		{
			WINDOWPLACEMENT placement = { sizeof(placement)
			};

			if (::GetWindowPlacement(hWnd, &placement))
			{
				::ShowWindowAsync(hWnd, ::IsZoomed(hWnd) || (placement.flags & WPF_RESTORETOMAXIMIZED) != 0 ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
			}
		}
	}

	WTL::CImageList small_image_list() const
	{
		return this->_small_image_list;
	}

	void small_image_list(WTL::CImageList imgList)
	{
		this->_small_image_list = imgList;
		this->lvFiles.SetImageList(imgList, LVSIL_SMALL);
	}

	WTL::CMenuHandle GetMenu() const
	{
		return this->CDialogImpl::GetMenu();
	}

	// Implementation in main_dialog.cpp
	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);

	DWORD GetDlgTemplate(LPDLGTEMPLATE buffer, DWORD cbLen)
	{
		DWORD cb = this->CModifiedDialogImpl<CMainDlg>::GetDlgTemplate(buffer, cbLen);
		if (cb <= cbLen)
		{
			CDlgTemplate dlgTemplate;
			if (dlgTemplate.SetTemplate(buffer, cb))
			{
				dlgTemplate.GetSizeInPixels(&this->template_size);
			}
		}

		return cb;
	}

	struct ResultCompareBase
	{
		~ResultCompareBase()
		{
			dlg->SetProgress(denominator, denominator);
			dlg->Flush();
		}

		struct swapper_type
		{
			ResultCompareBase* me;
			swapper_type(ResultCompareBase* const me) : me(me) {}

			void operator()(SearchResult& a, SearchResult& b)
			{
				SearchResult* const real_b = &a == &b ? nullptr : &b;
				me->check_cancelled(&a, real_b);
				if (real_b)
				{
					using std::swap;
					swap(a, b);
				}
			}
		};
#pragma pack(push, 1)
			struct result_type
		{
			typedef unsigned short first_type;
			typedef unsigned long long second_type;
			first_type first;
			second_type second;
			result_type() : first(), second() {}

			result_type(first_type
				const& first, second_type
				const& second) : first(first), second(second) {}

			bool operator < (result_type
				const& other) const
			{
				return this->first < other.first || (!(other.first < this->first) && this->second < other.second);
			}
		};
#pragma pack(pop)
			typedef NtfsIndex Index;
		CMainDlg* const this_;
		std::vector<Index::ParentIterator::value_type >* temp;
		std::vector < unsigned long long >* temp_keys;
		unsigned char variation;
		CProgressDialog* dlg;
		unsigned long long denominator;
		unsigned long long numerator;
		std::tvstring buffer, buffer2;
		void check_cancelled(SearchResult
			const* const a = nullptr, SearchResult
			const* const b = nullptr)
		{
			++numerator;
			if (dlg)
			{
				unsigned long long
					const tnow = GetTickCount64();
				if (dlg->ShouldUpdate(tnow))
				{
					if (dlg->HasUserCancelled(tnow))
					{
						throw CStructured_Exception(ERROR_CANCELLED, nullptr);
					}

					buffer.clear();
					if (a)
					{
						Index
							const* const p = this_->results.ith_index(a->index())->unvolatile();
						buffer += p->root_path();
						p->get_path(a->key(), buffer, false);
					}

					if (a && b)
					{
						buffer.push_back(_T('\r'));
						buffer.push_back(_T('\n'));
					}

					if (b)
					{
						Index
							const* const p = this_->results.ith_index(b->index())->unvolatile();
						buffer += p->root_path();
						p->get_path(b->key(), buffer, false);
					}

					dlg->SetProgressText(buffer);
					dlg->SetProgress(numerator, numerator <= denominator ? denominator : 0);
					dlg->Flush();
				}
			}
		}

		swapper_type swapper()
		{
			return this;
		};

	};

	template < int SubItem>
	struct ResultCompare
	{
		ResultCompareBase* base;
		ResultCompare(ResultCompareBase* const base) : base(base) {}

		typedef ResultCompareBase::Index Index;
		typedef ResultCompareBase::result_type result_type;
		typedef unsigned char word_type;
		typedef word_type value_type;
		result_type operator()(Results::value_type
			const& v) const
		{
			base->check_cancelled(&v);
			result_type result = result_type();
			result.first = (base->variation & 0x2) ? static_cast<typename result_type::second_type> (~v.depth()) : typename result_type::second_type();
			NtfsIndex
				const* const p = base->this_->results.ith_index(v.index())->unvolatile();
			typedef Index::key_type key_type;
			key_type
				const key = v.key();
			switch (SubItem)
			{
			case COLUMN_INDEX_SIZE:
			case COLUMN_INDEX_SIZE_ON_DISK:
			case COLUMN_INDEX_DESCENDENTS:
			{
				Index::size_info
					const& info = p->get_sizes(key);
				switch (SubItem)
				{
				case COLUMN_INDEX_SIZE:
					result.second = (base->variation & 0x1) ? /*sort by space saved */ static_cast<unsigned long long> (info.length - info.allocated) - (1ULL << ((sizeof(unsigned long long) * CHAR_BIT) - 1)) : static_cast<unsigned long long> (info.length);
					break;
				case COLUMN_INDEX_SIZE_ON_DISK:
					result.second = (base->variation & 0x1) ? info.bulkiness : info.allocated;
					break;
				case COLUMN_INDEX_DESCENDENTS:
					result.second = info.treesize;
					break;
				default:
					break;
				}

				break;
			}

			case COLUMN_INDEX_CREATION_TIME:
			case COLUMN_INDEX_MODIFICATION_TIME:
			case COLUMN_INDEX_ACCESS_TIME:
			{
				Index::standard_info
					const& info = p->get_stdinfo(key.frs());
				switch (SubItem)
				{
				case COLUMN_INDEX_CREATION_TIME:
					if (base->variation & 0x1)
					{
						result.second = ((static_cast<unsigned long long> (key.frs()) << (sizeof(unsigned int) * CHAR_BIT)) |
							(static_cast<unsigned long long> (/*complemented because we grew our singly-linked list backwards */ ~key.name_info() & ((1U << key_type::name_info_bits) - 1)) << key_type::stream_info_bits) |
							(static_cast<unsigned long long> (/*complemented because we grew our singly-linked list backwards */ ~key.stream_info() & ((1U << key_type::stream_info_bits) - 1)))
							);
					}
					else
					{
						result.second = info.created;
					}

					break;
				case COLUMN_INDEX_MODIFICATION_TIME:
					result.second = info.written;
					break;
				case COLUMN_INDEX_ACCESS_TIME:
					result.second = info.accessed;
					break;
				default:
					break;
				}

				break;
			}

			default:
				break;
			}

			return result;
		}

		int precompare(Results::value_type
			const& a, Results::value_type
			const& b) const
		{
			int r = 0;
			if (base->variation & 0x2)
			{
				size_t
					const
					a_depth = a.depth(),
					b_depth = b.depth();
				if (a_depth < b_depth)
				{
					r = -1;
				}
				else if (b_depth < a_depth)
				{
					r = +1;
				}
			}

			return r;
		}

		typedef result_type first_argument_type, second_argument_type;
		bool operator()(first_argument_type
			const& a, second_argument_type
			const& b) const
		{
			base->check_cancelled(nullptr, nullptr);
			return a < b;
		}

		bool operator()(Results::value_type
			const& a, Results::value_type
			const& b) const
		{
			base->check_cancelled(&a, &b);
			int
				const precomp = this->precompare(a, b);
			bool less;
			if (precomp < 0)
			{
				less = true;
			}
			else
			{
				stdext::remove_cv<Index>::type
					const*
					index1 = base->this_->results.ith_index(a.index())->unvolatile(),
					* index2 = base->this_->results.ith_index(b.index())->unvolatile();
				switch (SubItem)
				{
				case COLUMN_INDEX_NAME:
				case COLUMN_INDEX_PATH:
				case COLUMN_INDEX_TYPE:
				{
					bool
						const name_only = SubItem != COLUMN_INDEX_PATH,
						is_type = SubItem == COLUMN_INDEX_TYPE;
					int c; c = 0;
					if (!name_only)
					{
						Index::ParentIterator::value_type_compare comp;
						std::tvstring
							const& r1 = index1->root_path(), & r2 = index2->root_path();
						Index::ParentIterator::value_type
							v1 = { r1.data()
						},
							v2 = { r2.data()
						};

						v1.second = r1.size();
						v1.ascii = false;
						v2.second = r2.size();
						v2.ascii = false;
						if (comp(v1, v2))
						{
							c = -1;
						}
						else if (comp(v2, v1))
						{
							c = +1;
						}
					}

					if (c != 0)
					{
						less = c < 0;
					}
					else
					{
						Index::ParentIterator i1(index1, a.key()), i2(index2, b.key());
						unsigned short n1, n2;
						if (!name_only)
						{
							Index::ParentIterator j1(index1, a.key()), j2(index2, b.key());
							{
								unsigned short
									const a_depth = a.depth(),
									b_depth = b.depth();
								Index::ParentIterator* const ideeper = &(a_depth < b_depth ? j2 : j1);
								for (unsigned short depthdiff = a_depth < b_depth ? b_depth - a_depth : a_depth - b_depth; depthdiff; --depthdiff)
								{
									++* ideeper;
								}
							}

							for (;;)
							{
								if (j1 == j2)
								{
									break;
								}

								int changed = 0;
								if (!j1.empty())
								{
									++j1;
									++changed;
								}

								if (!j2.empty())
								{
									++j2;
									++changed;
								}

								if (!changed)
								{
									break;
								}
							}

							n1 = j1.icomponent();
							n2 = j2.icomponent();
						}
						else
						{
							n1 = USHRT_MAX;
							n2 = USHRT_MAX;
						}

						if (is_type)
						{
							base->buffer2.clear();
							size_t
								const k0 = 0;
							base->buffer2 += index1->root_path();
							i1.next();
							append_directional(base->buffer2, static_cast<TCHAR
								const*> (i1->first), i1->second, i1->ascii ? -1 : 0, false);
							size_t
								const k1 = k0 + base->this_->get_file_type_blocking(base->buffer2, k0, i1.attributes());
							base->buffer2 += index2->root_path();
							i2.next();
							append_directional(base->buffer2, static_cast<TCHAR
								const*> (i2->first), i2->second, i2->ascii ? -1 : 0, false);
							size_t
								const k2 = k1 + base->this_->get_file_type_blocking(base->buffer2, k1, i2.attributes());
							less = std::lexicographical_compare(base->buffer2.begin() + static_cast<ptrdiff_t> (k0), base->buffer2.begin() + static_cast<ptrdiff_t> (k1),
								base->buffer2.begin() + static_cast<ptrdiff_t> (k1), base->buffer2.begin() + static_cast<ptrdiff_t> (k2));
						}
						else
						{
							size_t itemp = 0;
							base->temp->resize((1 + USHRT_MAX) * 2);
							while (i1.icomponent() != n1 && i1.next() && !(name_only && i1.icomponent()))
							{
								(*base->temp)[itemp++] = *i1;
							}

							size_t
								const len1 = itemp;
							while (i2.icomponent() != n2 && i2.next() && !(name_only && i2.icomponent()))
							{
								(*base->temp)[itemp++] = *i2;
							}

							// Here we rely on the path structure, assuming that components are broken down on well-defined boundaries.
							// This would NOT work with arbitrary substrings.
							less = std::lexicographical_compare(base->temp->rend() - static_cast<ptrdiff_t> (len1), base->temp->rend(),
								base->temp->rend() - static_cast<ptrdiff_t> (itemp), base->temp->rend() - static_cast<ptrdiff_t> (len1),
								Index::ParentIterator::value_type_compare());
						}
					}

					break;
				}

				default:
					less = this->operator()(this->operator()(a), this->operator()(b));
					break;
				}
			}

			return less;
		}
	};

	LRESULT OnFilesListColumnClick(LPNMHDR pnmh)
	{
		LPNM_LISTVIEW pLV = (LPNM_LISTVIEW)pnmh;
		WTL::CHeaderCtrl header = this->lvFiles.GetHeader();
		bool
			const shift_pressed = GetKeyState(VK_SHIFT) < 0;
		bool
			const alt_pressed = GetKeyState(VK_MENU) < 0;
		unsigned char
			const variation = (alt_pressed ? 1U : 0U) | ((shift_pressed ? 1U : 0U) << 1);
		bool cancelled = false;
		this->lvFiles.SetItemState(-1, 0, LVNI_SELECTED);
		int
			const subitem = pLV->iSubItem;
		bool
			const same_key_as_last = this->last_sort.column == subitem + 1 && this->last_sort.variation == variation;
		bool
			const reversed = same_key_as_last && this->last_sort.counter % 2;
		int resulting_sort = 0;
		if ((this->lvFiles.GetStyle() & LVS_OWNERDATA) != 0)
		{
			try
			{
				WTL::CWaitCursor
					const wait_cursor;
				CProgressDialog dlg(this->m_hWnd);
				TCHAR buf[0x100];
				safe_stprintf(buf, this->stringLoader_(IDS_STATUS_SORTING_RESULTS), static_cast<std::tstring> (nformat_ui(this->results.size())).c_str());
				dlg.SetProgressTitle(buf);
				if (!dlg.HasUserCancelled())
				{
					this->statusbar.SetText(0, buf);
					LockedResults locked_results(*this);
					for (size_t i = 0; i != this->results.size(); ++i)
					{
						locked_results(i);
					}

					std::vector<NtfsIndex::ParentIterator::value_type > temp;
					std::vector < unsigned long long > temp_keys;
					Results::iterator
						const begin = this->results.begin(),
						end = this->results.end();
					ptrdiff_t
						const item_count = end - begin;
					using std::ceil;
					ResultCompareBase compare = { this, &temp, &temp_keys, variation, &dlg, static_cast<unsigned long long> ((ceil(log(static_cast<double> (item_count)) + 1) * item_count * 1.5))
					};

					clock_t
						const tstart = clock();
					int proposed_sort = 0;
					bool pretend_reversed;
					CSetRedraw
						const redraw(this->lvFiles, FALSE);
					switch (subitem)
					{
#define X(Column) case Column: { pretend_reversed = !reversed; proposed_sort = pretend_reversed ? +1 : -1; ResultCompare<Column> comp(&compare); if (is_sorted_ex(begin, end, comp, pretend_reversed)) { std::reverse(begin, end); } else { std::stable_sort(begin, end, comp); if (!pretend_reversed) { std::reverse(begin, end); } } } break
						X(COLUMN_INDEX_NAME);
						X(COLUMN_INDEX_PATH);
						X(COLUMN_INDEX_TYPE);
#undef  X

#define X(Column) case Column: { pretend_reversed =  reversed; proposed_sort = pretend_reversed ? +1 : -1; ResultCompare<Column> comp(&compare); if (is_sorted_ex(begin, end, comp, pretend_reversed)) { std::reverse(begin, end); } else { stable_sort_by_key(begin, end, comp, compare.swapper()); if (!pretend_reversed) { std::reverse(begin, end); } } } break
						X(COLUMN_INDEX_SIZE);
						X(COLUMN_INDEX_SIZE_ON_DISK);
						X(COLUMN_INDEX_CREATION_TIME);
						X(COLUMN_INDEX_MODIFICATION_TIME);
						X(COLUMN_INDEX_ACCESS_TIME);
						X(COLUMN_INDEX_is_readonly);
						X(COLUMN_INDEX_is_archive);
						X(COLUMN_INDEX_is_system);
						X(COLUMN_INDEX_is_hidden);
						X(COLUMN_INDEX_is_offline);
						X(COLUMN_INDEX_is_notcontentidx);
						X(COLUMN_INDEX_is_noscrubdata);
						X(COLUMN_INDEX_is_integretystream);
						X(COLUMN_INDEX_is_pinned);
						X(COLUMN_INDEX_is_unpinned);
						X(COLUMN_INDEX_is_directory);
						X(COLUMN_INDEX_is_compressed);
						X(COLUMN_INDEX_is_encrypted);
						X(COLUMN_INDEX_is_sparsefile);
						X(COLUMN_INDEX_is_reparsepoint);
						X(COLUMN_INDEX_ATTRIBUTE);
						// #####
#undef  X
					}

					resulting_sort = proposed_sort;
					clock_t
						const tend = clock();
					safe_stprintf(buf, this->stringLoader_(IDS_STATUS_SORTED_RESULTS), static_cast<std::tstring> (nformat_ui(this->results.size())).c_str(), (tend - tstart) * 1.0 / CLOCKS_PER_SEC);
					this->statusbar.SetText(0, buf);
				}
			}

			catch (CStructured_Exception& ex)
			{
				cancelled = true;
				if (ex.GetSENumber() != ERROR_CANCELLED)
				{
					throw;
				}

				this->statusbar.SetText(0, _T(""));
			}

			this->SetItemCount(this->lvFiles.GetItemCount());
		}

		if (!cancelled)
		{
			this->last_sort.counter = same_key_as_last ? this->last_sort.counter + 1 : 1;
			this->last_sort.column = subitem + 1;
			this->last_sort.variation = variation;
		}

		HDITEM hditem = { HDI_FORMAT
		};

		if (header.GetItem(subitem, &hditem))
		{
			hditem.fmt = (hditem.fmt & ~(HDF_SORTUP | HDF_SORTDOWN)) | (resulting_sort < 0 ? HDF_SORTDOWN : resulting_sort > 0 ? HDF_SORTUP : 0);
			header.SetItem(subitem, &hditem);
		}

		return TRUE;
	}

	void update_and_flush_invalidated_rows()
	{
		std::sort(this->invalidated_rows.begin(), this->invalidated_rows.end());
		for (size_t i = 0; i != this->invalidated_rows.size(); ++i)
		{
			int j1 = this->invalidated_rows[i], j2 = j1;
			while (i + 1 < this->invalidated_rows.size() && this->invalidated_rows[i] == j2 + 1)
			{
				++i;
				++j2;
			}

			this->lvFiles.RedrawItems(j1, j2);
		}

		this->invalidated_rows.clear();
	}

	BOOL SetItemCount(int n, bool
		const assume_existing_are_valid = false)
	{
		BOOL result;
		WTL::CHeaderCtrl header = this->lvFiles.GetHeader();
		int
			const nsubitems = header.GetItemCount();
		this->update_and_flush_invalidated_rows();
		if (this->lvFiles.GetStyle() & LVS_OWNERDATA)
		{
			result = this->lvFiles.SetItemCount(n);
		}
		else
		{
			if (n == 0)
			{
				result = this->lvFiles.DeleteAllItems();
			}
			else
			{
				result = assume_existing_are_valid ? TRUE : this->lvFiles.DeleteAllItems();
				CSetRedraw no_redraw(this->lvFiles, false);
				CDisableListViewUnnecessaryMessages disable_messages;
				int i = this->lvFiles.GetItemCount();
				while (result && i > n)
				{
					result = result && this->lvFiles.DeleteItem(i);
					--i;
				}

				std::tvstring text;
				for (; result && i < n; ++i)
				{
					LVITEM item;
					item.iItem = i;
					for (item.iSubItem = 0; result && item.iSubItem < nsubitems; ++item.iSubItem)
					{
						if (item.iSubItem != COLUMN_INDEX_TYPE)
						{
							item.mask = { LVIF_TEXT
							};

							this->get_subitem_text(static_cast<size_t> (item.iItem), item.iSubItem, text);
							item.pszText = (text.c_str() /*ensure null-terminated */, text.empty() ? nullptr : &*text.begin());
							item.cchTextMax = static_cast<int> (text.size());
							if (!item.iSubItem)
							{
								result = result && this->lvFiles.InsertItem(&item) != -1;
							}
							else
							{
								result = result && this->lvFiles.SetItem(&item);
							}
						}
					}
				}
			}
		}

		if (result)
		{
			for (int subitem = nsubitems; subitem > 0 && ((void) --subitem, true);)
			{
				HDITEM hditem = { HDI_FORMAT
				};

				if (header.GetItem(subitem, &hditem))
				{
					hditem.fmt = (hditem.fmt & ~(HDF_SORTUP | HDF_SORTDOWN));
					header.SetItem(subitem, &hditem);
				}
			}

			this->lvFiles.UpdateWindow();
		}

		return result;
	}

	void clear(bool
		const clear_cache, bool
		const deallocate_buffers)
	{
		WTL::CWaitCursor wait(this->lvFiles.GetItemCount() > 0, IDC_APPSTARTING);
		this->SetItemCount(0);
		this->results.clear();
		if (deallocate_buffers)
		{
			Results().swap(this->results);
		}

		this->last_sort.column = 0;
		this->last_sort.variation = 0;
		this->last_sort.counter = 0;
		if (clear_cache)
		{
			this->cache.clear();
			this->cached_column_header_widths.clear();
			{
				atomic_namespace::unique_lock<atomic_namespace::recursive_mutex>
					const lock(this->type_cache_mutex);
				this->type_cache.clear();
				this->type_cache_str.clear();
			}
		}
	}

	// Implementation in main_dialog.cpp
	void Search();

	void OnBrowse(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
	{
		struct Callback
		{
			std::tstring initial_folder;
			int operator()(ATL::CWindow window, UINT uMsg, LPARAM lParam)
			{
				int result = 0;
				switch (uMsg)
				{
				case BFFM_INITIALIZED:
					window.SendMessage(BFFM_SETSELECTION, TRUE, reinterpret_cast<LPARAM> (this->initial_folder.c_str()));
					break;
				case BFFM_VALIDATEFAILED:
					result = 1;
					break;
				}

				return result;
			}

			static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
			{
				return (*reinterpret_cast<Callback*> (lpData))(hwnd, uMsg, lParam);
			}
		}

		callback;
		ATL::CComBSTR bstr;
		if (this->txtPattern.GetWindowText(bstr.m_str))
		{
			callback.initial_folder.assign(bstr, bstr.Length());
			while (callback.initial_folder.size() > 0 && *(callback.initial_folder.end() - 1) == _T('*'))
			{
				callback.initial_folder.erase(callback.initial_folder.end() - 1, callback.initial_folder.end());
			}
		}

		std::tstring path(MAX_PATH, _T('\0'));
		CoInit
			const coinit;
		BROWSEINFO info = { this->m_hWnd, nullptr, &*path.begin(), this->stringLoader_(IDS_BROWSE_BODY), BIF_NONEWFOLDERBUTTON | BIF_USENEWUI | BIF_RETURNONLYFSDIRS | BIF_RETURNFSANCESTORS | BIF_DONTGOBELOWDOMAIN, Callback::BrowseCallbackProc, reinterpret_cast<LPARAM> (&callback)
		};

		CShellItemIDList pidl(SHBrowseForFolder(&info));
		if (!pidl.IsNull())
		{
			std::fill(path.begin(), path.end(), _T('\0'));
			if (SHGetPathFromIDList(pidl, &*path.begin()))
			{
				path.erase(std::find(path.begin(), path.end(), _T('\0')), path.end());
				adddirsep(path);
				path += _T('*');
				path += _T('*');
				this->txtPattern.SetWindowText(path.c_str());
				this->GotoDlgCtrl(this->txtPattern);
				this->txtPattern.SetSel(this->txtPattern.GetWindowTextLength(), this->txtPattern.GetWindowTextLength());
			}
		}
	}

	void OnOK(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
	{
		if (GetFocus() == this->lvFiles)
		{
			int
				const index = this->lvFiles.GetNextItem(-1, LVNI_FOCUSED);
			if (index >= 0 && (this->lvFiles.GetItemState(index, LVNI_SELECTED) & LVNI_SELECTED))
			{
				this->DoubleClick(index);
			}
			else
			{
				this->Search();
				if (index >= 0)
				{
					this->lvFiles.EnsureVisible(index, FALSE);
					this->lvFiles.SetItemState(index, LVNI_FOCUSED, LVNI_FOCUSED);
				}
			}
		}
		else if (GetFocus() == this->txtPattern || GetFocus() == this->btnOK)
		{
			this->Search();
		}
	}

	void append_selected_indices(std::vector<size_t>& result) const
	{
		HookedNtUserProps HOOK_CONCAT(hook_, NtUserProp);
		result.reserve(result.size() + static_cast<size_t> (this->lvFiles.GetSelectedCount()));
		WNDPROC
			const lvFiles_wndproc = reinterpret_cast<WNDPROC> (::GetWindowLongPtr(this->lvFiles.m_hWnd, GWLP_WNDPROC));
		for (int i = -1;;)
		{
			i = static_cast<int> (lvFiles_wndproc(this->lvFiles.m_hWnd, LVM_GETNEXTITEM, i, MAKELPARAM(LVNI_SELECTED, 0)));
			// i = this->lvFiles.GetNextItem(i, LVNI_SELECTED);
			if (i < 0)
			{
				break;
			}

			result.push_back(static_cast<size_t> (i));
		}
	}

	LRESULT OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		WTL::CWaitCursor wait;
		(void)uMsg;
		LRESULT result = 0;
		POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)
		};

		if ((HWND)wParam == this->lvFiles)
		{
			std::vector<size_t> indices;
			int index;
			if (point.x == -1 && point.y == -1)
			{
				index = this->lvFiles.GetSelectedIndex();
				if (index >= 0)
				{
					RECT bounds = {};

					this->lvFiles.GetItemRect(index, &bounds, LVIR_SELECTBOUNDS);
					point.x = bounds.left;
					point.y = bounds.top;
					this->lvFiles.MapWindowPoints(HWND_DESKTOP, &point, 1);
					indices.push_back(static_cast<size_t> (index));
				}
			}
			else
			{
				POINT clientPoint = point; ::MapWindowPoints(nullptr, this->lvFiles, &clientPoint, 1);
				index = this->lvFiles.HitTest(clientPoint, 0);
				if (index >= 0)
				{
					this->append_selected_indices(indices);
				}
			}

			int
				const focused = this->lvFiles.GetNextItem(-1, LVNI_FOCUSED);
			if (!indices.empty())
			{
				this->RightClick(indices, point, focused);
			}
		}

		return result;
	}

	class LockedResults
	{
		LockedResults(LockedResults
			const&);
		LockedResults& operator=(LockedResults
			const&);
		typedef std::vector<atomic_namespace::unique_lock<atomic_namespace::recursive_mutex>> IndicesLocks;
		CMainDlg* me;
		IndicesLocks indices_locks;	// this is to permit us to call unvolatile() below
	public:
		explicit LockedResults(CMainDlg& me) : me(&me) {}

		void operator()(size_t
			const index)
		{
			atomic_namespace::recursive_mutex* const m = &me->results.item_index(index)->get_mutex();
			bool found_lock = false;
			for (const auto& lock : indices_locks)
			{
				if (lock.mutex() == m)
				{
					found_lock = true;
					break;
				}
			}

			if (!found_lock)
			{
				indices_locks.emplace_back(*m);
			}
		}
	};

	// Implementation in main_dialog.cpp
	void RightClick(std::vector<size_t> const& indices, POINT const& point, int const focused);

	// Implementation in main_dialog.cpp
	void dump_or_save(std::vector<size_t> const& locked_indices, int const mode, int const single_column);

	void DoubleClick(int index)
	{
		Wow64Disable
			const wow64disable(true);
		NtfsIndex
			const volatile* const i = this->results.item_index(static_cast<size_t> (index));
		std::tvstring path;
		path = i->root_path(), lock(i)->get_path(this->results[static_cast<size_t> (index)].key(), path, false);
		remove_path_stream_and_trailing_sep(path);
		std::unique_ptr<std::pair<std::pair<CShellItemIDList, ATL::CComPtr<IShellFolder> >, std::vector< CShellItemIDList > >> p(new std::pair<std::pair<CShellItemIDList, ATL::CComPtr<IShellFolder> >, std::vector< CShellItemIDList >>());
		SFGAOF sfgao = 0;
		std::tstring
			const path_directory(path.begin(), dirname(path.begin(), path.end()));
		HRESULT hr = SHParseDisplayName(path_directory.c_str(), nullptr, &p->first.first, 0, &sfgao);
		if (hr == S_OK)
		{
			ATL::CComPtr<IShellFolder> desktop;
			hr = SHGetDesktopFolder(&desktop);
			if (hr == S_OK)
			{
				if (!ILIsEmpty(static_cast<LPCITEMIDLIST> (p->first.first)))
				{
					hr = desktop->BindToObject(p->first.first, nullptr, IID_IShellFolder, reinterpret_cast<void**> (&p->first.second));
				}
				else
				{
					hr = desktop.QueryInterface(&p->first.second);
				}
			}
		}

		if (hr == S_OK && basename(path.begin(), path.end()) != path.end())
		{
			p->second.resize(1);
			if (!path.empty())
			{
				hr = SHParseDisplayName(&path[0], nullptr, &p->second.back().m_pidl, sfgao, &sfgao);
			}
		}

		SHELLEXECUTEINFO shei = { sizeof(shei), SEE_MASK_INVOKEIDLIST | SEE_MASK_UNICODE, *this, nullptr, nullptr, p->second.empty() ? path_directory.c_str() : nullptr, path_directory.c_str(), SW_SHOWDEFAULT, 0, p->second.empty() ? nullptr : p->second.back().m_pidl
		};

		ShellExecuteEx(&shei);
	}

	LRESULT OnFilesDoubleClick(LPNMHDR pnmh)
	{
		// Wow64Disable wow64Disabled;
		WTL::CWaitCursor wait;
		if (this->lvFiles.GetSelectedCount() == 1)
		{
			this->DoubleClick(this->lvFiles.GetNextItem(-1, LVNI_SELECTED));
		}

		return 0;
	}

	void OnFileNameChange(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
	{
		if (!this->autocomplete_called)
		{
			if (SHAutoComplete(this->txtPattern, SHACF_FILESYS_ONLY | SHACF_USETAB) == S_OK)	// Needs CoInitialize() to have been called (use CoInit)
			{
				this->autocomplete_called = true;
			}
		}
	}

	LRESULT OnFileNameArrowKey(LPNMHDR pnmh)
	{
		CSearchPattern::KeyNotify* const p = (CSearchPattern::KeyNotify*)pnmh;
		if (p->vkey == VK_UP || p->vkey == VK_DOWN)
		{
			this->cmbDrive.SendMessage(p->hdr.code == CSearchPattern::CUN_KEYDOWN ? WM_KEYDOWN : WM_KEYUP, p->vkey, p->lParam);
		}
		else
		{
			if (p->hdr.code == CSearchPattern::CUN_KEYDOWN && p->vkey == VK_DOWN && this->lvFiles.GetItemCount() > 0)
			{
				this->lvFiles.SetFocus();
			}

			this->lvFiles.SendMessage(p->hdr.code == CSearchPattern::CUN_KEYDOWN ? WM_KEYDOWN : WM_KEYUP, p->vkey, p->lParam);
		}

		return 0;
	}

	LRESULT OnFilesKeyDown(LPNMHDR pnmh)
	{
		NMLVKEYDOWN* const p = (NMLVKEYDOWN*)pnmh;
		if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) >= 0)
		{
			if (p->wVKey == 'A')
			{
				LVITEM item = { LVIF_STATE, -1, -1, LVIS_SELECTED, LVIS_SELECTED
				};

				this->lvFiles.SetItemState(-1, &item);
			}

			if (p->wVKey == 'C')
			{
				WTL::CWaitCursor wait;
				typedef std::vector<size_t> Indices;
				std::vector<size_t> indices;
				this->append_selected_indices(indices);
				LockedResults locked_results(*this);
				std::for_each<Indices::const_iterator, LockedResults&>(indices.begin(), indices.end(), locked_results);
				bool
					const copy_path = GetKeyState(VK_SHIFT) < 0;
				this->dump_or_save(indices, copy_path ? 1 : 2, COLUMN_INDEX_PATH);
			}
		}

		if (p->wVKey == VK_UP && this->lvFiles.GetNextItem(-1, LVNI_FOCUSED) == 0)
		{
			this->txtPattern.SetFocus();
		}

		return 0;
	}

	size_t get_file_type_blocking(std::tvstring& name, size_t
		const name_offset, unsigned int
		const file_attributes) const	// TODO: should be 'volatile'
	{
		size_t result = 0;
		while (name.size() > name_offset && hasdirsep(name))
		{
			name.erase(name.end() - 1);
		}

		atomic_namespace::unique_lock<atomic_namespace::recursive_mutex>
			const lock(this->type_cache_mutex);
		this->type_cache_str.assign(name.begin() + static_cast<ptrdiff_t> (name_offset), basename(name.begin() + static_cast<ptrdiff_t> (name_offset), name.end()));
		size_t ext_offset, ext_length;
		if (file_attributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			ext_offset = name.size() - (!name.empty() && *(name.end() - 1) == _T('.'));
			ext_length = name.size() - static_cast<ptrdiff_t> (ext_offset);
		}
		else
		{
			// TODO: Name should not include alternate data stream...
			size_t
				const name_end = static_cast<size_t> (std::find(basename(name.begin(), name.end()), name.end(), _T(':')) - name.begin());
			ext_offset = fileext(name.begin() + static_cast<ptrdiff_t> (name_offset), name.begin() + static_cast<ptrdiff_t> (name_end)) - name.begin();
			ext_length = name_end - ext_offset;
		}

		if (ext_offset == name.size())
		{
			this->type_cache_str.append(_T("*"));
		}

		this->type_cache_str.append(name.data() + static_cast<ptrdiff_t> (ext_offset), ext_length);
		if (!this->type_cache_str.empty() && *(this->type_cache_str.end() - 1) == _T('.') && (this->type_cache_str.size() <= 1 || isdirsep(*(this->type_cache_str.end() - 2))))
		{
			this->type_cache_str.erase(this->type_cache_str.end() - 1, this->type_cache_str.end());
		}

		name.erase(name.begin() + static_cast<ptrdiff_t> (name_offset), name.end());
		if (!this->type_cache_str.empty())
		{
			unsigned int
				const usable_attributes = file_attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT) /*restrict this to prevent redundant storage */;
			// HACK: Encode the file attributes into the name, after a null terminator
			size_t
				const type_cache_str_length = this->type_cache_str.size();
			this->type_cache_str.push_back(_T('\0'));
			this->type_cache_str.insert(this->type_cache_str.end(), reinterpret_cast<TCHAR
				const*> (&usable_attributes), sizeof(usable_attributes) / sizeof(TCHAR));
			std::transform(this->type_cache_str.begin(), this->type_cache_str.begin() + static_cast<ptrdiff_t> (type_cache_str_length), this->type_cache_str.begin(), char_transformer<TCHAR, totupper < TCHAR>>());
			if (usable_attributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				adddirsep(this->type_cache_str);
			}

			TypeInfoCache::value_type::second_type& found = this->type_cache[this->type_cache_str];
			if (found.empty() && !this->type_cache_str.empty())
			{
				SHFILEINFO shfi = {};
				Wow64Disable
					const wow64disable(true);
				if (SHGetFileInfo(this->type_cache_str.c_str(), usable_attributes, &shfi, sizeof(shfi), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES))
				{
					found.append(shfi.szTypeName);
				}
			}

			name += found;
			result = found.size();
		}

		this->type_cache_str.clear();
		return result;
	}

	// Implementation in main_dialog.cpp
	void GetSubItemText(size_t const j, int const subitem, bool const for_ui, std::tvstring& text, bool const lock_index = true) const;

	LRESULT OnFilesIncrementalSearch(LPNMHDR pnmh)
	{
		NMLVFINDITEM* const pLV = (NMLVFINDITEM*)pnmh;
		if (pLV->lvfi.flags & (LVFI_STRING | LVFI_PARTIAL))
		{
			int
				const n = this->lvFiles.GetItemCount();
			pLV->lvfi.lParam = this->lvFiles.GetNextItem(-1, LVNI_FOCUSED);
			TCHAR
				const* needle = pLV->lvfi.psz;
			size_t needle_length = std::char_traits<TCHAR>::length(needle);
			while (needle_length > 1 && *(needle + 1) == *needle)
			{
				++needle;
				--needle_length;
			}

			string_matcher matcher(string_matcher::pattern_verbatim, string_matcher::pattern_option_case_insensitive, needle, needle_length);
			LockedResults locked_results(*this);
			std::tvstring text;
			for (int i = 0; i < n; ++i)
			{
				int
					const iItem = (pLV->lvfi.lParam + (needle_length > 1 ? 0 : 1) + i) % n;
				if (!(pLV->lvfi.flags & LVFI_WRAP) && iItem == 0 && i != 0)
				{
					break;
				}

				locked_results(static_cast<size_t> (iItem));
				text.clear();
				this->GetSubItemText(static_cast<size_t> (iItem), COLUMN_INDEX_NAME, true, text, false);
				bool
					const match = (pLV->lvfi.flags & (LVFI_PARTIAL | (0x0004 /*LVFI_SUBSTRING*/))) ?
					text.size() >= needle_length && matcher.is_match(text.data(), needle_length) :
					text.size() == needle_length && matcher.is_match(text.data(), needle_length);
				if (match)
				{
					pLV->lvfi.lParam = iItem;
					break;
				}
			}
		}

		return 0;
	}

	void get_subitem_text(size_t
		const item, int
		const subitem, std::tvstring& text)
	{
		text.clear();
		this->GetSubItemText(item, subitem, true, text);
	}

	LRESULT OnFilesGetDispInfo(LPNMHDR pnmh)
	{
		NMLVDISPINFO* const pLV = (NMLVDISPINFO*)pnmh;

		if ((this->lvFiles.GetStyle() & LVS_OWNERDATA) != 0 && (pLV->item.mask & LVIF_TEXT) != 0)
		{
			size_t
				const j = static_cast<size_t> (pLV->item.iItem);
			std::tvstring text;
			this->get_subitem_text(j, pLV->item.iSubItem, text);
			if (!text.empty())
			{
				_tcsncpy(pLV->item.pszText, text.c_str(), pLV->item.cchTextMax);
			}

			if (pLV->item.iSubItem == 0)
			{
				WTL::CRect rc;
				this->lvFiles.GetClientRect(&rc);
				WTL::CRect rcitem;
				if (this->lvFiles.GetItemRect(pLV->item.iItem, &rcitem, LVIR_ICON) && rcitem.IntersectRect(&rcitem, &rc))
				{
					std::tvstring path;
					this->GetSubItemText(j, COLUMN_INDEX_PATH, true, path);
					int iImage = this->CacheIcon(path, static_cast<int> (pLV->item.iItem), lock(this->results.item_index(j))->get_stdinfo(this->results[j].key().frs()).attributes(), GetMessageTime());
					if (iImage >= 0)
					{
						pLV->item.iImage = iImage;
					}
				}
			}
		}

		return 0;
	}

	void OnCancel(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
	{
		if (this->suppress_escapes <= 0)
		{
			if (this->EnterBackground())
			{
				this->ShowWindow(SW_HIDE);
			}
		}

		this->suppress_escapes = 0;
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		BOOL result = FALSE;
		if (this->m_hWnd)
		{
			if (this->accel)
			{
				if (this->accel.TranslateAccelerator(this->m_hWnd, pMsg))
				{
					result = TRUE;
				}
			}

			if (!result)
			{
				result = this->CWindow::IsDialogMessage(pMsg);
			}
		}

		return result;
	}

	LRESULT OnFilesListCustomDraw(LPNMHDR pnmh)
	{
		// Build colors struct from member variables (which may be customized from registry)
		uffs::FileAttributeColors
			const colors = { deletedColor, encryptedColor, compressedColor, directoryColor, hiddenColor, systemColor,
			                 static_cast<COLORREF>(RGB(GetRValue(compressedColor),
			                     (GetGValue(compressedColor) + GetBValue(compressedColor)) / 2,
			                     (GetGValue(compressedColor) + GetBValue(compressedColor)) / 2)) };
		LRESULT result;
		LPNMLVCUSTOMDRAW
			const pLV             = (LPNMLVCUSTOMDRAW)pnmh;
		if (pLV->nmcd.dwItemSpec < this->results.size())
		{
			size_t
				const iresult = static_cast<size_t> (pLV->nmcd.dwItemSpec);
			unsigned long
				const attrs = lock(this->results.item_index(iresult))->get_stdinfo(this->results[iresult].key().frs()).attributes();
			switch (pLV->nmcd.dwDrawStage)
			{
			case CDDS_PREPAINT:
				result = CDRF_NOTIFYITEMDRAW;
				break;
			case CDDS_ITEMPREPAINT:
				result = CDRF_NOTIFYSUBITEMDRAW;
				break;
			case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
				if (pLV->nmcd.dwItemSpec % 2 && false)
				{
					unsigned char
						const v = 0xF8;
					pLV->clrTextBk = RGB(v, v, v);
				}

				if ((this->small_image_list() == this->imgListLarge || this->small_image_list() == this->imgListExtraLarge) && pLV->iSubItem == 1)
				{
					result = 0x8 /*CDRF_DOERASE*/ | CDRF_NOTIFYPOSTPAINT;
				}
				else
				{
					// Use the centralized color lookup from file_attribute_colors.hpp
					COLORREF attrColor = colors.colorForAttributes(attrs);
					if (attrColor != 0)
					{
						pLV->clrText = attrColor;
					}

					result = CDRF_DODEFAULT;
				}

				break;
			case CDDS_ITEMPOSTPAINT | CDDS_SUBITEM:
				result = CDRF_SKIPDEFAULT;
				{
					std::tvstring itemText;
					this->GetSubItemText(static_cast<size_t> (pLV->nmcd.dwItemSpec), pLV->iSubItem, true, itemText);
					WTL::CDCHandle dc(pLV->nmcd.hdc);
					RECT rcTwips   = pLV->nmcd.rc;
					rcTwips.left   = (int)((rcTwips.left + 6) * 1440 / dc.GetDeviceCaps(LOGPIXELSX));
					rcTwips.right  = (int)(rcTwips.right * 1440      / dc.GetDeviceCaps(LOGPIXELSX));
					rcTwips.top    = (int)(rcTwips.top * 1440        / dc.GetDeviceCaps(LOGPIXELSY));
					rcTwips.bottom = (int)(rcTwips.bottom * 1440     / dc.GetDeviceCaps(LOGPIXELSY));
					int
						const savedDC = dc.SaveDC();
					{
						std::replace(itemText.begin(), itemText.end(), _T(' '), _T('\u00A0'));
						{
							size_t
								const prev_size = itemText.size();
							for (size_t j = 0; j != prev_size; ++j)
							{
								itemText.push_back(itemText[j]);
								if (itemText[j] == _T('\\'))
								{
									itemText.push_back(_T('\u200B'));
								}
							}

							itemText.erase(itemText.begin(), itemText.begin() + static_cast<ptrdiff_t> (prev_size));
						}

						if (!this->richEdit)
						{
							this->hRichEdit = LoadLibrary(_T("riched20.dll"));
							this->richEdit.Create(this->lvFiles, nullptr, 0, ES_MULTILINE, WS_EX_TRANSPARENT);
							this->richEdit.SetFont(this->lvFiles.GetFont());
						}
#ifdef _UNICODE
							this->richEdit.SetTextEx(itemText.c_str(), ST_DEFAULT, 1200); 
#else
							this->richEdit.SetTextEx(itemText.c_str(), ST_DEFAULT, CP_ACP); 
#endif
							CHARFORMAT format = { sizeof(format), CFM_COLOR, 0, 0, 0, 0
						};

						// Use centralized color lookup from file_attribute_colors.hpp
						COLORREF attrColor = colors.colorForAttributes(attrs);
						if (attrColor != 0)
						{
							format.crTextColor = attrColor;
						}
						else
						{
							bool
								const selected = (this->lvFiles.GetItemState(static_cast<int> (pLV->nmcd.dwItemSpec), LVIS_SELECTED) & LVIS_SELECTED) != 0;
							format.crTextColor = GetSysColor(selected && this->lvFiles.IsThemeNull() ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT);
						}

						this->richEdit.SetSel(0, -1);
						this->richEdit.SetSelectionCharFormat(format);
						FORMATRANGE formatRange = { dc, dc, rcTwips, rcTwips,
							{ 0, -1 }
						};

						this->richEdit.FormatRange(formatRange, FALSE);
						LONG height = formatRange.rc.bottom - formatRange.rc.top;
						formatRange.rc = formatRange.rcPage;
						formatRange.rc.top += (formatRange.rc.bottom - formatRange.rc.top - height) / 2;
						this->richEdit.FormatRange(formatRange, TRUE);

						this->richEdit.FormatRange(nullptr);
					}

					dc.RestoreDC(savedDC);
				}

				break;
			default:
				result = CDRF_DODEFAULT;
				break;
			}
		}
		else
		{
			result = CDRF_DODEFAULT;
		}

		return result;
	}

	void OnClose(UINT /*uNotifyCode*/ = 0, int nID = IDCANCEL, HWND /*hWnd*/ = nullptr)
	{
		this->DestroyWindow();
		PostQuitMessage(nID);
		// this->EndDialog(nID);
	}

	LRESULT OnDeviceChange(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam)
	{
		switch (wParam)
		{
		case DBT_DEVICEQUERYREMOVEFAILED: {}

										break;
		case DBT_DEVICEQUERYREMOVE:
		{
			DEV_BROADCAST_HDR
				const& header = *reinterpret_cast<DEV_BROADCAST_HDR*> (lParam);
			if (header.dbch_devicetype == DBT_DEVTYP_HANDLE)
			{
				(void) reinterpret_cast<DEV_BROADCAST_HANDLE
					const&> (header);
			}
		}

		break;
		case DBT_DEVICEREMOVECOMPLETE:
		{
			DEV_BROADCAST_HDR
				const& header = *reinterpret_cast<DEV_BROADCAST_HDR*> (lParam);
			if (header.dbch_devicetype == DBT_DEVTYP_HANDLE)
			{
				(void) reinterpret_cast<DEV_BROADCAST_HANDLE
					const&> (header);
			}
		}

		break;
		case DBT_DEVICEARRIVAL:
		{
			DEV_BROADCAST_HDR
				const& header = *reinterpret_cast<DEV_BROADCAST_HDR*> (lParam);
			if (header.dbch_devicetype == DBT_DEVTYP_VOLUME) {}
		}

		break;
		default:
			break;
		}

		return TRUE;
	}

	void OnSize(UINT nType, WTL::CSize size)
	{
		if (GetKeyState(VK_CONTROL) < 0)
		{
			this->PostMessage(WM_COMMAND, ID_VIEW_FITCOLUMNSTOWINDOW, 0);
		}
		else if (GetKeyState(VK_SHIFT) < 0)
		{
			this->PostMessage(WM_COMMAND, ID_VIEW_AUTOSIZECOLUMNS, 0);
		}

		this->SetMsgHandled(FALSE);
	}

	static bool ShouldWaitForWindowVisibleOnStartup()
	{
		STARTUPINFO si;
		GetStartupInfo(&si);
		return !((si.dwFlags & STARTF_USESHOWWINDOW) && si.wShowWindow == SW_HIDE);
	}

	void OnWindowPosChanged(LPWINDOWPOS lpWndPos)
	{
		if (lpWndPos->flags & SWP_SHOWWINDOW)
		{
			if (!this->_initialized)
			{
				this->_initialized = true;
				this->UpdateWindow();
				this->PostMessage(WM_READY);
			}
		}

		this->SetMsgHandled(FALSE);
	}

	LRESULT OnReady(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam)
	{
		this->Refresh(true);
		RegisterWaitForSingleObject(&this->hWait, hEvent, &WaitCallback, this->m_hWnd, INFINITE, WT_EXECUTEINUITHREAD);
		return 0;
	}

	LRESULT OnRefreshItems(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam)
	{
		this->update_and_flush_invalidated_rows();
		return 0;
	}

	bool ExitBackground()
	{
		SetPriorityClass(GetCurrentProcess(), 0x200000 /*PROCESS_MODE_BACKGROUND_END*/);
		NOTIFYICONDATA nid = { sizeof(nid), *this, 0
		};

		return !!Shell_NotifyIcon(NIM_DELETE, &nid);
	}

	bool EnterBackground()
	{
		bool result = this->CheckAndCreateIcon(false);
		SetPriorityClass(GetCurrentProcess(), 0x100000 /*PROCESS_MODE_BACKGROUND_BEGIN*/);
		return result;
	}

	BOOL CheckAndCreateIcon(bool checkVisible)
	{
		NOTIFYICONDATA nid = { sizeof(nid), *this, 0, NIF_MESSAGE | NIF_ICON | NIF_TIP, WM_NOTIFYICON, this->GetIcon(FALSE)
		};

		_tcsncpy(nid.szTip, this->stringLoader_(IDS_APPNAME), sizeof(nid.szTip) / sizeof(*nid.szTip));
		return (!checkVisible || (this->ShouldWaitForWindowVisibleOnStartup() && !this->IsWindowVisible())) && Shell_NotifyIcon(NIM_ADD, &nid);
	}

	LRESULT OnTaskbarCreated(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
	{
		this->CheckAndCreateIcon(true);
		return 0;
	}

	LRESULT OnNotifyIcon(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam)
	{
		if (lParam == WM_LBUTTONUP || lParam == WM_KEYUP)
		{
			this->ExitBackground();
			this->ShowWindow(SW_SHOW);
		}

		return 0;
	}

	void OnHelp(UINT /*uNotifyCode*/, int nID, CWindow /*wndCtl*/)
	{
		RefCountedCString body, title;
		unsigned int type = MB_OK | MB_ICONINFORMATION;
		switch (nID)
		{
		case ID_HELP_DONATE:
			body.Format(this->stringLoader_(IDS_HELP_DONATE_BODY), this->get_project_url(IDS_PROJECT_USER_FRIENDLY_URL).c_str());
			title = this->stringLoader_(IDS_HELP_DONATE_TITLE);
			type  = ((type & ~static_cast<unsigned int> (MB_OK)) | MB_OKCANCEL);
			break;
		case ID_HELP_TRANSLATION:
			body.Format(this->stringLoader_(IDS_HELP_TRANSLATION_BODY), static_cast<LPCTSTR> (get_ui_locale_name()));
			title = this->stringLoader_(IDS_HELP_TRANSLATION_TITLE);
			type  = ((type & ~static_cast<unsigned int> (MB_OK)) | MB_OKCANCEL);
			break;
		case ID_HELP_COPYING:
			body  = this->stringLoader_(IDS_HELP_COPYING_BODY);
			title = this->stringLoader_(IDS_HELP_COPYING_TITLE);
			break;
		case ID_HELP_NTFSMETADATA:
			body  = this->stringLoader_(IDS_HELP_NTFS_METADATA_BODY);
			title = this->stringLoader_(IDS_HELP_NTFS_METADATA_TITLE);
			break;
		case ID_HELP_SEARCHINGBYDEPTH:
			body  = this->stringLoader_(IDS_HELP_SEARCHING_BY_DEPTH_BODY);
			title = this->stringLoader_(IDS_HELP_SEARCHING_BY_DEPTH_TITLE);
			break;
		case ID_HELP_SORTINGBYBULKINESS:
			body  = this->stringLoader_(IDS_HELP_SORTING_BY_BULKINESS_BODY);
			title = this->stringLoader_(IDS_HELP_SORTING_BY_BULKINESS_TITLE);
			break;
		case ID_HELP_SORTINGBYDEPTH:
			body  = this->stringLoader_(IDS_HELP_SORTING_BY_DEPTH_BODY);
			title = this->stringLoader_(IDS_HELP_SORTING_BY_DEPTH_TITLE);
			break;
		case ID_HELP_USINGREGULAREXPRESSIONS:
			body  = this->stringLoader_(IDS_HELP_REGEX_BODY);
			title = this->stringLoader_(IDS_HELP_REGEX_TITLE);
			break;
		}

		if (!body.IsEmpty() || !title.IsEmpty())
		{
			int
				const r = this->MessageBox(static_cast<LPCTSTR> (body), static_cast<LPCTSTR> (title), type);
			switch (nID)
			{
			case ID_HELP_DONATE:
			case ID_HELP_TRANSLATION:
				if (r == IDOK)
				{
					this->open_project_page();
				}

				break;
			default:
				break;
			}
		}
	}

	void OnHelpBugs(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
	{
		long long
			const ticks = get_version(&__ImageBase);
		RefCountedCString body;
		body.Format(this->stringLoader_(IDS_TEXT_REPORT_ISSUES), this->get_project_url(IDS_PROJECT_USER_FRIENDLY_URL).c_str());
		{
			body += L"\u2022";  // Unicode bullet character
			body += this->stringLoader_(IDS_TEXT_SPACE);
			body += this->stringLoader_(IDS_TEXT_UI_LOCALE_NAME);
			body += this->stringLoader_(IDS_TEXT_SPACE);
			body += get_ui_locale_name();
		}

		body += _T("\r\n");
		{
			std::tvstring buf_localized, buf_invariant;
			SystemTimeToString(ticks, buf_localized, false, false);
			SystemTimeToString(ticks, buf_invariant, true, false);
			body += L"\u2022";  // Unicode bullet character
			body += this->stringLoader_(IDS_TEXT_SPACE);
			body += this->stringLoader_(IDS_TEXT_BUILD_DATE);
			body += this->stringLoader_(IDS_TEXT_SPACE);
			body += buf_localized.c_str();
			body += this->stringLoader_(IDS_TEXT_SPACE);
			body += this->stringLoader_(IDS_TEXT_PAREN_OPEN);
			body += buf_invariant.c_str();
			body += this->stringLoader_(IDS_TEXT_PAREN_CLOSE);
		}

		if (this->MessageBox(body, this->stringLoader_(IDS_HELP_BUGS_TITLE), MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
		{
			this->open_project_page();
		}
	}

	RefCountedCString get_project_url(unsigned short
		const id)
	{
		RefCountedCString result;
		result.Format(this->stringLoader_(id), this->stringLoader_(IDS_PROJECT_NAME).c_str());
		return result;
	}

	void open_project_page()
	{
		ShellExecute(nullptr, _T("open"), this->get_project_url(IDS_PROJECT_ROBUST_URL), nullptr, nullptr, SW_SHOWNORMAL);
	}

	void OnViewLargeIcons(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
	{
		bool
			const large = this->small_image_list() != this->imgListLarge;
		this->small_image_list(large ? this->imgListLarge : this->imgListSmall);
		this->lvFiles.RedrawWindow();
		this->GetMenu().CheckMenuItem(ID_VIEW_LARGEICONS, large ? MF_CHECKED : MF_UNCHECKED);
	}

	void OnViewGridlines(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
	{
		this->lvFiles.SetExtendedListViewStyle(this->lvFiles.GetExtendedListViewStyle() ^ LVS_EX_GRIDLINES);
		this->lvFiles.RedrawWindow();
		this->GetMenu().CheckMenuItem(ID_VIEW_GRIDLINES, (this->lvFiles.GetExtendedListViewStyle() & LVS_EX_GRIDLINES) ? MF_CHECKED : MF_UNCHECKED);
	}

	void OnViewFitColumns(UINT /*uNotifyCode*/, int nID, CWindow /*wndCtl*/)
		// #####
	{
		WTL::CListViewCtrl& wndListView = this->lvFiles;
		CSetRedraw no_redraw(wndListView, false);
		if (nID == ID_VIEW_FITCOLUMNSTOWINDOW)
		{
			RECT rect;
			wndListView.GetClientRect(&rect);
			int
				const client_width = (std::max)(1, (int)(rect.right - rect.left) - ((this->lvFiles.GetWindowLong(GWL_STYLE) & WS_VSCROLL) ? 0 : GetSystemMetrics(SM_CXVSCROLL)) - 1);

			WTL::CHeaderCtrl wndListViewHeader = wndListView.GetHeader();
			int oldTotalColumnsWidth;
			oldTotalColumnsWidth = 0;
			int columnCount;
			columnCount = wndListViewHeader.GetItemCount();
			for (int i = 0; i < columnCount; i++)
			{
				oldTotalColumnsWidth += wndListView.GetColumnWidth(i);
			}

			for (int i = 0; i < columnCount; i++)
			{
				int colWidth = wndListView.GetColumnWidth(i);
				int newWidth = MulDiv(colWidth, client_width, oldTotalColumnsWidth);
				newWidth = (std::max)(newWidth, 1);
				wndListView.SetColumnWidth(i, newWidth);
			}
		}
		else if (nID == ID_VIEW_AUTOSIZECOLUMNS)
		{
			struct TextGetter : WTL::CListViewCtrl
			{
				CMainDlg* me;
				TextGetter(CMainDlg& me) : WTL::CListViewCtrl(me.lvFiles), me(&me) {}

				static ptrdiff_t invoke(void* const me, int
					const item, int
					const subitem, ListViewAdapter::String& result)
				{
					TextGetter* const this_ = static_cast<TextGetter*> (static_cast<WTL::CListViewCtrl*> (me));
					this_->me->get_subitem_text(item, subitem, result);
					return static_cast<ptrdiff_t> (result.size());
				}
			}

			instance(*this);
			//autosize_columns(ListViewAdapter(instance, this->cached_column_header_widths, &TextGetter::invoke));
		}
	}

	void OnOptionsFastPathFiltering(UINT /*uNotifyCode*/, int nID, CWindow /*wndCtl*/)
	{
		this->trie_filtering = !this->trie_filtering;
		this->GetMenu().CheckMenuItem(ID_OPTIONS_FAST_PATH_SEARCH, this->trie_filtering ? MF_CHECKED : MF_UNCHECKED);
	}

	void OnRefresh(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
	{
		return this->Refresh(false);
	}

	// Implementation in main_dialog.cpp
	void Refresh(bool const initial);

#pragma warning(suppress: 4555)
	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_WM_SIZE(OnSize)	// must come BEFORE chained message maps
		CHAIN_MSG_MAP(CInvokeImpl < CMainDlg>)
		CHAIN_MSG_MAP(CDialogResize < CMainDlg>)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
		MSG_WM_CLOSE(OnClose)
		MESSAGE_HANDLER_EX(WM_DEVICECHANGE, OnDeviceChange)	// Don't use MSG_WM_DEVICECHANGE(); it's broken (uses DWORD)
		MESSAGE_HANDLER_EX(WM_NOTIFYICON, OnNotifyIcon)
		MESSAGE_HANDLER_EX(WM_READY, OnReady)
		MESSAGE_HANDLER_EX(WM_REFRESHITEMS, OnRefreshItems)
		MESSAGE_HANDLER_EX(WM_TASKBARCREATED(), OnTaskbarCreated)
		MESSAGE_HANDLER_EX(WM_MOUSEWHEEL, OnMouseWheel)
		MESSAGE_HANDLER_EX(WM_CONTEXTMENU, OnContextMenu)
		COMMAND_ID_HANDLER_EX(ID_HELP_BUGS, OnHelpBugs)
		COMMAND_ID_HANDLER_EX(ID_HELP_DONATE, OnHelp)
		COMMAND_ID_HANDLER_EX(ID_HELP_TRANSLATION, OnHelp)
		COMMAND_ID_HANDLER_EX(ID_HELP_COPYING, OnHelp)
		COMMAND_ID_HANDLER_EX(ID_HELP_NTFSMETADATA, OnHelp)
		COMMAND_ID_HANDLER_EX(ID_HELP_SEARCHINGBYDEPTH, OnHelp)
		COMMAND_ID_HANDLER_EX(ID_HELP_SORTINGBYBULKINESS, OnHelp)
		COMMAND_ID_HANDLER_EX(ID_HELP_SORTINGBYDEPTH, OnHelp)
		COMMAND_ID_HANDLER_EX(ID_HELP_USINGREGULAREXPRESSIONS, OnHelp)
		COMMAND_ID_HANDLER_EX(ID_VIEW_GRIDLINES, OnViewGridlines)
		COMMAND_ID_HANDLER_EX(ID_VIEW_LARGEICONS, OnViewLargeIcons)
		COMMAND_ID_HANDLER_EX(ID_VIEW_FITCOLUMNSTOWINDOW, OnViewFitColumns)
		COMMAND_ID_HANDLER_EX(ID_VIEW_AUTOSIZECOLUMNS, OnViewFitColumns)
		COMMAND_ID_HANDLER_EX(ID_OPTIONS_FAST_PATH_SEARCH, OnOptionsFastPathFiltering)
		COMMAND_ID_HANDLER_EX(ID_FILE_EXIT, OnClose)
		COMMAND_ID_HANDLER_EX(ID_ACCELERATOR40006, OnRefresh)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancel)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnOK)
		COMMAND_HANDLER_EX(IDC_BUTTON_BROWSE, BN_CLICKED, OnBrowse)
		COMMAND_HANDLER_EX(IDC_EDITFILENAME, EN_CHANGE, OnFileNameChange)
		NOTIFY_HANDLER_EX(IDC_LISTFILES, NM_CUSTOMDRAW, OnFilesListCustomDraw)
		NOTIFY_HANDLER_EX(IDC_LISTFILES, LVN_INCREMENTALSEARCH, OnFilesIncrementalSearch)
		NOTIFY_HANDLER_EX(IDC_LISTFILES, LVN_GETDISPINFO, OnFilesGetDispInfo)
		NOTIFY_HANDLER_EX(IDC_LISTFILES, LVN_COLUMNCLICK, OnFilesListColumnClick)
		NOTIFY_HANDLER_EX(IDC_LISTFILES, NM_DBLCLK, OnFilesDoubleClick)
		NOTIFY_HANDLER_EX(IDC_EDITFILENAME, CSearchPattern::CUN_KEYDOWN, OnFileNameArrowKey)
		NOTIFY_HANDLER_EX(IDC_LISTFILES, LVN_KEYDOWN, OnFilesKeyDown)
		END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(CMainDlg)
		DLGRESIZE_CONTROL(IDC_LISTFILES, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_EDITFILENAME, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_STATUS_BAR, DLSZ_SIZE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_BUTTON_BROWSE, DLSZ_MOVE_X)
	END_DLGRESIZE_MAP()
	enum
	{
		IDD = IDD_DIALOG1
	};

					};
