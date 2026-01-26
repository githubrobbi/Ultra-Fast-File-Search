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
	StringLoader LoadString;
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

	BOOL OnInitDialog(CWindow /*wndFocus*/, LPARAM /*lInitParam*/)
	{
		_Module.GetMessageLoop()->AddMessageFilter(this);
		this->setTopmostWindow.reset(::topmostWindow, this->m_hWnd);

		this->SetWindowText(this->LoadString(IDS_APPNAME));
		this->lvFiles.Attach(this->GetDlgItem(IDC_LISTFILES));
		this->btnBrowse.Attach(this->GetDlgItem(IDC_BUTTON_BROWSE));
		this->btnBrowse.SetWindowText(this->LoadString(IDS_BUTTON_BROWSE));
		this->btnOK.Attach(this->GetDlgItem(IDOK));
		this->btnOK.SetWindowText(this->LoadString(IDS_BUTTON_SEARCH));
		this->cmbDrive.Attach(this->GetDlgItem(IDC_LISTVOLUMES));
		this->accel.LoadAccelerators(IDR_ACCELERATOR1);
		this->txtPattern.SubclassWindow(this->GetDlgItem(IDC_EDITFILENAME));
		if (!this->txtPattern)
		{
			this->txtPattern.Attach(this->GetDlgItem(IDC_EDITFILENAME));
		}

		this->txtPattern.EnsureTrackingMouseHover();
		this->txtPattern.SetCueBannerText(this->LoadString(IDS_SEARCH_PATTERN_BANNER), true);
		WTL::CHeaderCtrl hdr = this->lvFiles.GetHeader();
		{
			int
				const icol = COLUMN_INDEX_NAME;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, 250, this->LoadString(IDS_COLUMN_NAME_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_PATH;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, 280, this->LoadString(IDS_COLUMN_PATH_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_TYPE;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_LEFT, 120, this->LoadString(IDS_COLUMN_TYPE_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_SIZE;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_RIGHT, 80, this->LoadString(IDS_COLUMN_SIZE_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_SIZE_ON_DISK;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_RIGHT, 80, this->LoadString(IDS_COLUMN_SIZE_ON_DISK_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_CREATION_TIME;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 140, this->LoadString(IDS_COLUMN_CREATION_TIME_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_MODIFICATION_TIME;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 140, this->LoadString(IDS_COLUMN_WRITE_TIME_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_ACCESS_TIME;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 140, this->LoadString(IDS_COLUMN_ACCESS_TIME_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_DESCENDENTS;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_RIGHT, 80, this->LoadString(IDS_COLUMN_DESCENDENTS_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_readonly;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_READONLY_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_archive;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_ARCHIVE_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_system;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_SYSTEM_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_hidden;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_HIDDEN_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_offline;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_OFFLINE_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_notcontentidx;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_NOTCONTENTIDX_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_noscrubdata;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_NOSCRUBDATA_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_integretystream;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_INTEGRETYSTREAM_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_pinned;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_PINNED_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_unpinned;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 25, this->LoadString(IDS_COLUMN_INDEX_IS_UNPINNED_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_directory;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 30, this->LoadString(IDS_COLUMN_INDEX_IS_DIRECTORY_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_compressed;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 80, this->LoadString(IDS_COLUMN_INDEX_IS_COMPRESSED_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_encrypted;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 70, this->LoadString(IDS_COLUMN_INDEX_IS_ENCRYPTED_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_sparsefile;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 50, this->LoadString(IDS_COLUMN_INDEX_IS_SPARSEFILE_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_is_reparsepoint;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 60, this->LoadString(IDS_COLUMN_INDEX_IS_REPARSEPOINT_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		{

			int
				const icol = COLUMN_INDEX_ATTRIBUTE;
			LVCOLUMN column = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, LVCFMT_CENTER, 60, this->LoadString(IDS_COLUMN_INDEX_IS_ATTRIBUTE_HEADER)
			};

			this->lvFiles.InsertColumn(icol, &column);
		}

		this->cmbDrive.SetCueBannerText(this->LoadString(IDS_SEARCH_VOLUME_BANNER));
		HINSTANCE hInstance = GetModuleHandle(nullptr);
		this->SetIcon((HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0), FALSE);
		this->SetIcon((HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0), TRUE);

		{

			const int IMAGE_LIST_INCREMENT = 0x100;
			this->imgListSmall.Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CXSMICON), ILC_COLOR32, 0, IMAGE_LIST_INCREMENT);
			this->imgListLarge.Create(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXICON), ILC_COLOR32, 0, IMAGE_LIST_INCREMENT);
			this->imgListExtraLarge.Create(48, 48, ILC_COLOR32, 0, IMAGE_LIST_INCREMENT);
		}

		this->lvFiles.OpenThemeData(VSCLASS_LISTVIEW);
		SetWindowTheme(this->lvFiles, _T("Explorer"), nullptr);
		if (false)
		{
			WTL::CFontHandle font = this->txtPattern.GetFont();
			LOGFONT logFont;
			if (font.GetLogFont(logFont))
			{
				logFont.lfHeight = logFont.lfHeight * 100 / 85;
				this->txtPattern.SetFont(WTL::CFontHandle().CreateFontIndirect(&logFont));
			}
		}

		this->trie_filtering = this->GetMenu().GetMenuState(ID_OPTIONS_FAST_PATH_SEARCH, MF_BYCOMMAND) & MFS_CHECKED;
		this->lvFiles.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | (this->GetMenu().GetMenuState(ID_VIEW_GRIDLINES, MF_BYCOMMAND) ? LVS_EX_GRIDLINES : 0) | LVS_EX_HEADERDRAGDROP | 0x80000000 /*LVS_EX_COLUMNOVERFLOW*/);
		{
			this->small_image_list((this->GetMenu().GetMenuState(ID_VIEW_LARGEICONS, MF_BYCOMMAND) & MFS_CHECKED) ? this->imgListLarge : this->imgListSmall);
			this->lvFiles.SetImageList(this->imgListLarge, LVSIL_NORMAL);
			this->lvFiles.SetImageList(this->imgListExtraLarge, LVSIL_NORMAL);
		}

		//this->SendMessage(WM_COMMAND, ID_VIEW_FITCOLUMNSTOWINDOW);

		this->statusbar = CreateStatusWindow(WS_CHILD | SBT_TOOLTIPS | WS_VISIBLE, nullptr, *this, IDC_STATUS_BAR);
		int
			const rcStatusPaneWidths[] = { 360, -1 };

		this->statusbar.SetParts(sizeof(rcStatusPaneWidths) / sizeof(*rcStatusPaneWidths), const_cast<int*> (rcStatusPaneWidths));
		this->statusbar.SetText(0, this->LoadString(IDS_STATUS_DEFAULT));
		WTL::CRect rcStatusPane1;
		this->statusbar.GetRect(1, &rcStatusPane1);
		//this->statusbarProgress.Create(this->statusbar, rcStatusPane1, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0);
		//this->statusbarProgress.SetRange(0, INT_MAX);
		//this->statusbarProgress.SetPos(INT_MAX / 2);
		WTL::CRect clientRect;
		if (this->lvFiles.GetClientRect(&clientRect))
		{
			clientRect.bottom -= rcStatusPane1.Height();
			this->lvFiles.ResizeClient(clientRect.Width(), clientRect.Height(), FALSE);
		}

		WTL::CRect my_client_rect;
		bool
			const unresize = this->GetClientRect(&my_client_rect) && this->template_size.cx > 0 && this->template_size.cy > 0 && my_client_rect.Size() != this->template_size && this->ResizeClient(this->template_size.cx, this->template_size.cy, FALSE);
		this->DlgResize_Init(false, false);
		if (unresize)
		{
			this->ResizeClient(my_client_rect.Width(), my_client_rect.Height(), FALSE);
		}

		// this->SetTimer(0, 15 * 60 *60,);

		std::vector<std::tvstring > path_names = get_volume_path_names();

		this->cmbDrive.SetCurSel(this->cmbDrive.AddString(this->LoadString(IDS_SEARCH_VOLUME_ALL)));
		for (const auto& path_name : path_names)
		{
			this->cmbDrive.AddString(path_name.c_str());
		}

		if (!this->ShouldWaitForWindowVisibleOnStartup())
		{
			if (!this->_initialized)
			{
				this->_initialized = true;
				this->PostMessage(WM_READY);
			}
		}

		return TRUE;
	}

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
				safe_stprintf(buf, this->LoadString(IDS_STATUS_SORTING_RESULTS), static_cast<std::tstring> (nformat_ui(this->results.size())).c_str());
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
					safe_stprintf(buf, this->LoadString(IDS_STATUS_SORTED_RESULTS), static_cast<std::tstring> (nformat_ui(this->results.size())).c_str(), (tend - tstart) * 1.0 / CLOCKS_PER_SEC);
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

	void Search()
	{
		WTL::CRegKeyEx key;
		if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer")) == ERROR_SUCCESS)
		{
			key.QueryDWORDValue(_T("AltColor"), compressedColor);
			key.QueryDWORDValue(_T("AltEncryptedColor"), encryptedColor);
			key.Close();
		}

		MatchOperation matchop;
		try
		{
			std::tstring pattern;
			{
				ATL::CComBSTR bstr;
				if (this->txtPattern.GetWindowText(bstr.m_str))
				{
					pattern.assign(bstr, bstr.Length());
				}
			}

			matchop.init(pattern);
		}

		catch (std::invalid_argument& ex)
		{
			RefCountedCString error = this->LoadString(IDS_INVALID_PATTERN);
			error += _T("\r\n");
			error += ex.what();
			this->MessageBox(error, this->LoadString(IDS_ERROR_TITLE), MB_ICONERROR);
			return;
		}

		bool preallocate = false;
		bool
			const shift_pressed = GetKeyState(VK_SHIFT) < 0;
		bool
			const control_pressed = GetKeyState(VK_CONTROL) < 0;
		int
			const selected = this->cmbDrive.GetCurSel();
		this->clear(false, false);
		WTL::CWaitCursor
			const wait_cursor;
		CProgressDialog dlg(this->m_hWnd);
		dlg.SetProgressTitle(this->LoadString(IDS_SEARCHING_TITLE));
		if (dlg.HasUserCancelled())
		{
			return;
		}

		using std::ceil;
		clock_t
			const tstart = clock();
		std::vector<uintptr_t> wait_handles;
		typedef NtfsIndex
			const volatile* index_pointer;
		std::vector<index_pointer> nonwait_indices, wait_indices, initial_wait_indices;
		// TODO: What if they exceed maximum wait objects?
		bool any_io_pending = false;
		size_t expected_results = 0;
		size_t overall_progress_numerator = 0, overall_progress_denominator = 0;
		for (int ii = this->cmbDrive.GetCount(); ii > 0 && ((void) --ii, true);)
		{
			if (intrusive_ptr < NtfsIndex>
				const p = static_cast<NtfsIndex*> (this->cmbDrive.GetItemDataPtr(ii)))
			{
				bool wait = false;
				if (selected == ii || selected == 0)
				{
					if (matchop.prematch(p->root_path()))
					{
						wait = true;
						wait_handles.push_back(p->finished_event());
						wait_indices.push_back(p.get());
						expected_results += p->expected_records();
						size_t
							const records_so_far = p->records_so_far();
						any_io_pending |= records_so_far < p->mft_capacity;
						overall_progress_denominator += p->mft_capacity * 2;
					}
				}

				if (!wait)
				{
					nonwait_indices.push_back(p.get());
				}
			}
		}

		initial_wait_indices = wait_indices;
		if (!any_io_pending)
		{
			overall_progress_denominator /= 2;
		}

		if (any_io_pending)
		{
			dlg.ForceShow();
		}

		if (preallocate)
		{
			try
			{
				this->results.reserve(this->results.size() + expected_results + expected_results / 8);
			}

			catch (std::bad_alloc&) {}
		}

		std::vector<IoPriority> set_priorities(nonwait_indices.size() + wait_indices.size());
		for (size_t i = 0; i != nonwait_indices.size(); ++i)
		{
			IoPriority(reinterpret_cast<uintptr_t> (nonwait_indices[i]->volume()), winnt::IoPriorityLow).swap(set_priorities.at(i));
		}

		for (size_t i = 0; i != wait_indices.size(); ++i)
		{
			IoPriority(reinterpret_cast<uintptr_t> (wait_indices[i]->volume()), winnt::IoPriorityLow).swap(set_priorities.at(nonwait_indices.size() + i));
		}

		IoPriority set_priority;
		Speed::second_type initial_time = Speed::second_type();
		Speed::first_type initial_average_amount = Speed::first_type();
		std::vector<Results> results_at_depths;
		results_at_depths.reserve(std::numeric_limits < unsigned short>::max() + 1);
		while (!wait_handles.empty())
		{
			if (uintptr_t
				const volume = reinterpret_cast<uintptr_t> (wait_indices.at(0)->volume()))
			{
				if (set_priority.volume() != volume)
				{
					IoPriority(volume, winnt::IoPriorityNormal).swap(set_priority);
				}
			}

			unsigned long
				const wait_result = dlg.WaitMessageLoop(wait_handles.empty() ? nullptr : &*wait_handles.begin(), wait_handles.size());
			if (wait_result == WAIT_TIMEOUT)
			{
				if (dlg.HasUserCancelled())
				{
					break;
				}

				if (dlg.ShouldUpdate())
				{
					basic_fast_ostringstream<TCHAR> ss;
					ss << this->LoadString(IDS_TEXT_READING_FILE_TABLES) << this->LoadString(IDS_TEXT_SPACE);
					bool any = false;
					unsigned long long temp_overall_progress_numerator = overall_progress_numerator;
					for (const auto& j : wait_indices)
					{
						size_t const records_so_far = j->records_so_far();
						temp_overall_progress_numerator += records_so_far;
						unsigned int const mft_capacity = j->mft_capacity;
						if (records_so_far != mft_capacity)
						{
							if (any)
							{
								ss << this->LoadString(IDS_TEXT_COMMA) << this->LoadString(IDS_TEXT_SPACE);
							}
							else
							{
								ss << this->LoadString(IDS_TEXT_SPACE);
							}

							ss << j->root_path() << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_PAREN_OPEN) << nformat_ui(j->records_so_far());
							// TODO: 'of' _really_ isn't a good thing to localize in isolation..
							ss << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_OF) << this->LoadString(IDS_TEXT_SPACE);
							// These MUST be separate statements since nformat_ui is used twice
							ss << nformat_ui(mft_capacity) << this->LoadString(IDS_TEXT_PAREN_CLOSE);
							any = true;
						}
					}

					bool
						const initial_speed = !initial_average_amount;
					Speed recent_speed, average_speed;
					for (const auto& idx : initial_wait_indices)
					{
						Speed const speed = idx->speed();
						average_speed.first += speed.first;
						average_speed.second += speed.second;
						if (initial_speed)
						{
							initial_average_amount += speed.first;
						}
					}

					clock_t
						const tnow = clock();
					if (initial_speed)
					{
						initial_time = tnow;
					}

					if (average_speed.first > initial_average_amount)
					{
						ss << _T('\n');
						ss << this->LoadString(IDS_TEXT_AVERAGE_SPEED) << this->LoadString(IDS_TEXT_COLON) << this->LoadString(IDS_TEXT_SPACE) <<
							nformat_ui(static_cast<size_t> ((average_speed.first - initial_average_amount) * static_cast<double> (CLOCKS_PER_SEC) / ((tnow != initial_time ? tnow - initial_time : 1) * static_cast<double>(1ULL << 20)))) <<
							this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_MIB_S);
						ss << this->LoadString(IDS_TEXT_SPACE);
						// These MUST be separate statements since nformat_ui is used twice
						ss << this->LoadString(IDS_TEXT_PAREN_OPEN) << nformat_ui(average_speed.first / (1 << 20)) << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_MIB_READ) << this->LoadString(IDS_TEXT_PAREN_CLOSE);
					}

					std::tstring
						const text = ss.str();
					dlg.SetProgressText(text);
					dlg.SetProgress(static_cast<long long> (temp_overall_progress_numerator), static_cast<long long> (overall_progress_denominator));
					dlg.Flush();
				}
			}
			else
			{
				if (wait_result < wait_handles.size())
				{
					index_pointer
						const i = wait_indices[wait_result];
					size_t current_progress_numerator = 0;
					size_t
						const current_progress_denominator = i->total_names_and_streams();
					std::tvstring root_path = i->root_path();
					std::tvstring current_path = matchop.get_current_path(root_path);
					unsigned int
						const task_result = i->get_finished();
					if (task_result != 0)
					{
						if (selected != 0)
						{
							ATL::CWindow(dlg.IsWindow() ? dlg.GetHWND() : this->m_hWnd).MessageBox(GetAnyErrorText(task_result), this->LoadString(IDS_ERROR_TITLE), MB_OK | MB_ICONERROR);
						}
					}

					try
					{
						lock(i)->matches([&dlg, &results_at_depths, &root_path, shift_pressed, this, i, &wait_indices, any_io_pending, &current_progress_numerator, current_progress_denominator,
							overall_progress_numerator, overall_progress_denominator, &matchop
						](TCHAR
							const* const name, size_t
							const name_length, bool
							const ascii, NtfsIndex::key_type
							const& key, size_t
							const depth)
							{
								unsigned long long
									const now = GetTickCount64();
								if (dlg.ShouldUpdate(now) || current_progress_denominator - current_progress_numerator <= 1)
								{
									if (dlg.HasUserCancelled(now))
									{
										throw CStructured_Exception(ERROR_CANCELLED, nullptr);
									}

									// this->lvFiles.SetItemCountEx(static_cast< int>(this->results.size()), 0), this->lvFiles.UpdateWindow();
									size_t temp_overall_progress_numerator = overall_progress_numerator;
									if (any_io_pending)
									{
										for (const auto& idx : wait_indices)
										{
											temp_overall_progress_numerator += idx->records_so_far();
										}
									}

									std::tvstring text(0x100 + root_path.size(), _T('\0'));
									text.resize(static_cast<size_t> (_sntprintf(&*text.begin(), text.size(), _T("%s%s%.*s%s%s%s%s%s%s%s%s%s\r\n"),
										this->LoadString(IDS_TEXT_SEARCHING).c_str(),
										this->LoadString(IDS_TEXT_SPACE).c_str(),
										static_cast<int> (root_path.size()), root_path.c_str(),
										this->LoadString(IDS_TEXT_SPACE).c_str(),
										this->LoadString(IDS_TEXT_PAREN_OPEN).c_str(),
										static_cast<std::tstring> (nformat_ui(current_progress_numerator)).c_str(),
										this->LoadString(IDS_TEXT_SPACE).c_str(),
										this->LoadString(IDS_TEXT_OF).c_str(),
										this->LoadString(IDS_TEXT_SPACE).c_str(),
										static_cast<std::tstring> (nformat_ui(current_progress_denominator)).c_str(),
										this->LoadString(IDS_TEXT_PAREN_CLOSE).c_str(),
										this->LoadString(IDS_TEXT_ELLIPSIS).c_str())));
									if (name_length)
									{
										append_directional(text, name, name_length, ascii ? -1 : 0);
									}

									dlg.SetProgressText(text);
									dlg.SetProgress(temp_overall_progress_numerator + static_cast<unsigned long long> (i->mft_capacity) * static_cast<unsigned long long> (current_progress_numerator) / static_cast<unsigned long long> (current_progress_denominator), static_cast<long long> (overall_progress_denominator));
									dlg.Flush();
								}

								++current_progress_numerator;
								if (current_progress_numerator > current_progress_denominator)
								{
									throw std::logic_error("current_progress_numerator > current_progress_denominator");
								}

								TCHAR
									const* const path_begin = name;
								size_t high_water_mark = 0, * phigh_water_mark = matchop.is_path_pattern && this->trie_filtering ? &high_water_mark : nullptr;
								bool
									const match = ascii ?
									matchop.matcher.is_match(static_cast<char
										const*> (static_cast<void
											const*> (path_begin)), name_length, phigh_water_mark) :
									matchop.matcher.is_match(path_begin, name_length, phigh_water_mark);
								if (match)
								{
									unsigned short
										const depth2 = static_cast<unsigned short> (depth * 2U) /*dividing by 2 later should not mess up the actual depths; it should only affect files vs. directory sub-depths */;
									if (shift_pressed)
									{
										if (depth2 >= results_at_depths.size())
										{
											results_at_depths.resize(depth2 + 1);
										}

										results_at_depths[depth2].push_back(i, Results::value_type(key, depth2));
									}
									else
									{
										this->results.push_back(i, Results::value_type(key, depth2));
									}
								}

								return match || !(matchop.is_path_pattern && phigh_water_mark && *phigh_water_mark < name_length);
							}, current_path, matchop.is_path_pattern, matchop.is_stream_pattern, control_pressed);
					}

					catch (CStructured_Exception& ex)
					{
						if (ex.GetSENumber() != ERROR_CANCELLED)
						{
							throw;
						}
					}

					if (any_io_pending)
					{
						overall_progress_numerator += i->mft_capacity;
					}

					if (current_progress_denominator)
					{
						overall_progress_numerator += static_cast<size_t> (static_cast<unsigned long long> (i->mft_capacity) * static_cast<unsigned long long> (current_progress_numerator) / static_cast<unsigned long long> (current_progress_denominator));
					}
				}

				wait_indices.erase(wait_indices.begin() + static_cast<ptrdiff_t> (wait_result));
				wait_handles.erase(wait_handles.begin() + static_cast<ptrdiff_t> (wait_result));
			}
		}

		{

			size_t size_to_reserve = 0;
			for (const auto& results_at_depth : results_at_depths)
			{
				size_to_reserve += results_at_depth.size();
			}

			this->results.reserve(this->results.size() + size_to_reserve);
			for (size_t j = results_at_depths.size(); j != 0 && ((void) --j, true);)
			{
				Results
					const& results_at_depth = results_at_depths[j];
				for (size_t i = results_at_depth.size(); i != 0 && ((void) --i, true);)
				{
					this->results.push_back(results_at_depth.item_index(i), results_at_depth[i]);
				}
			}

			this->SetItemCount(static_cast<int> (this->results.size()));
		}

		clock_t
			const tend = clock();
		TCHAR buf[0x100];
		safe_stprintf(buf, this->LoadString(IDS_STATUS_FOUND_RESULTS), static_cast<std::tstring> (nformat_ui(this->results.size())).c_str(), (tend - tstart) * 1.0 / CLOCKS_PER_SEC);
		this->statusbar.SetText(0, buf);
	}

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
		BROWSEINFO info = { this->m_hWnd, nullptr, &*path.begin(), this->LoadString(IDS_BROWSE_BODY), BIF_NONEWFOLDERBUTTON | BIF_USENEWUI | BIF_RETURNONLYFSDIRS | BIF_RETURNFSANCESTORS | BIF_DONTGOBELOWDOMAIN, Callback::BrowseCallbackProc, reinterpret_cast<LPARAM> (&callback)
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

	void RightClick(std::vector < size_t>
		const& indices, POINT
		const& point, int
		const focused)
	{
		Wow64Disable
			const wow64disable(true);
		LockedResults locked_results(*this);
		typedef std::vector<size_t> Indices;
		std::for_each<Indices::const_iterator, LockedResults&>(indices.begin(), indices.end(), locked_results);
		ATL::CComPtr<IShellFolder> desktop;
		HRESULT volatile hr = S_OK;
		UINT
			const minID = 1000;
		WTL::CMenu menu;
		menu.CreatePopupMenu();
		ATL::CComPtr<IContextMenu> contextMenu;
		std::unique_ptr<std::pair<std::pair<CShellItemIDList, ATL::CComPtr<IShellFolder> >, std::vector< CShellItemIDList > >> p(new std::pair<std::pair<CShellItemIDList, ATL::CComPtr<IShellFolder> >, std::vector< CShellItemIDList >>());
		p->second.reserve(indices.size());	// REQUIRED, to avoid copying CShellItemIDList objects (they're not copyable!)
		if (indices.size() <= (1 << 10))	// if not too many files... otherwise the shell context menu this will take a long time
		{
			SFGAOF sfgao = 0;
			std::tvstring path;
			for (size_t const iresult : indices)
			{
				NtfsIndex
					const* const index = this->results.item_index(iresult)->unvolatile() /*we are allowed to do this because the indices are locked */;
				path = index->root_path();
				if (index->get_path(this->results[iresult].key(), path, false))
				{
					remove_path_stream_and_trailing_sep(path);
				}

				CShellItemIDList itemIdList;
				hr = SHParseDisplayName(path.c_str(), nullptr, &itemIdList, sfgao, &sfgao);
				if (hr == S_OK)
				{
					if (p->first.first.IsNull())
					{
						p->first.first.Attach(ILClone(itemIdList));
						ILRemoveLastID(p->first.first);
					}

					while (!ILIsEmpty(static_cast<LPCITEMIDLIST> (p->first.first)) && !ILIsParent(p->first.first, itemIdList, FALSE))
					{
						ILRemoveLastID(p->first.first);
					}

					p->second.push_back(CShellItemIDList());
					p->second.back().Attach(itemIdList.Detach());
				}
			}

			if (hr == S_OK)
			{
				hr = SHGetDesktopFolder(&desktop);
				if (hr == S_OK)
				{
					if (!p->first.first.IsNull() && !ILIsEmpty(static_cast<LPCITEMIDLIST> (p->first.first)))
					{
						hr = desktop->BindToObject(p->first.first, nullptr, IID_IShellFolder, reinterpret_cast<void**> (&p->first.second));
					}
					else
					{
						hr = desktop.QueryInterface(&p->first.second);
					}
				}
			}

			if (hr == S_OK)
			{
				bool
					const desktop_relative = p->first.first.IsNull() || ILIsEmpty(static_cast<LPCITEMIDLIST> (p->first.first));
				std::vector<LPCITEMIDLIST> relative_item_ids(p->second.size());
				for (size_t i = 0; i < p->second.size(); ++i)
				{
					relative_item_ids[i] = desktop_relative ? static_cast<LPCITEMIDLIST> (p->second[i]) : ILFindChild(p->first.first, p->second[i]);
				}

				GUID
					const IID_IContextMenu = { 0x000214E4L, 0, 0,
						{
							0xC0, 0, 0, 0, 0, 0, 0, 0x46
						}
				};

				hr = (desktop_relative ? desktop : p->first.second)->GetUIObjectOf(*
					this,
					static_cast<UINT> (relative_item_ids.size()),
					relative_item_ids.empty() ? nullptr : &relative_item_ids[0],
					IID_IContextMenu,
					nullptr, &reinterpret_cast<void*&> (contextMenu.p));
			}

			if (hr == S_OK)
			{
				hr = contextMenu->QueryContextMenu(menu, 0, minID, UINT_MAX, 0x80 /*CMF_ITEMMENU*/);
			}
		}

		unsigned int ninserted = 0;
		UINT
			const
			openContainingFolderId = minID - 1,
			fileIdId = minID - 2,
			copyId = minID - 3,
			copyPathId = minID - 4,
			copyTableId = minID - 5,
			dumpId = minID - 6;

		if (indices.size() == 1)
		{
			MENUITEMINFO mii2 = { sizeof(mii2), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, MFS_ENABLED, openContainingFolderId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_OPEN_CONTAINING_FOLDER)
			};

			menu.InsertMenuItem(ninserted++, TRUE, &mii2);

			if (false)
			{
				menu.SetMenuDefaultItem(openContainingFolderId, FALSE);
			}
		}

		if (0 <= focused && static_cast<size_t> (focused) < this->results.size())
		{
			{ 	RefCountedCString text = this->LoadString(IDS_MENU_FILE_NUMBER);
			text += static_cast<std::tstring> (nformat_ui(this->results[static_cast<size_t> (focused)].key().frs())).c_str();
			MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, MFS_DISABLED, fileIdId, nullptr, nullptr, nullptr, 0, text
			};

			menu.InsertMenuItem(ninserted++, TRUE, &mii);
			}

			{

				MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, 0, copyId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_COPY)
				};

				menu.InsertMenuItem(ninserted++, TRUE, &mii);
			}

			{

				MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, 0, copyPathId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_COPY_PATHS)
				};

				menu.InsertMenuItem(ninserted++, TRUE, &mii);
			}

			{

				MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, 0, copyTableId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_COPY_TABLE)
				};

				menu.InsertMenuItem(ninserted++, TRUE, &mii);
			}

			{

				MENUITEMINFO mii = { sizeof(mii), MIIM_ID | MIIM_STRING | MIIM_STATE, MFT_STRING, 0, dumpId, nullptr, nullptr, nullptr, 0, this->LoadString(IDS_MENU_DUMP_TO_TABLE)
				};

				menu.InsertMenuItem(ninserted++, TRUE, &mii);
			}
		}

		if (contextMenu && ninserted)
		{
			MENUITEMINFO mii = { sizeof(mii), 0, MFT_MENUBREAK
			};

			menu.InsertMenuItem(ninserted, TRUE, &mii);
		}

		UINT id = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | (GetKeyState(VK_SHIFT) < 0 ? CMF_EXTENDEDVERBS : 0) |
			(GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN | TPM_HORNEGANIMATION : TPM_LEFTALIGN | TPM_HORPOSANIMATION),
			point.x, point.y, *this);
		if (!id)
		{
			// User cancelled
		}
		else if (id == openContainingFolderId)
		{
			if (QueueUserWorkItem(&SHOpenFolderAndSelectItemsThread, p.get(), WT_EXECUTEINUITHREAD))
			{
				p.release();
			}
		}
		else if (id == dumpId || id == copyId || id == copyPathId || id == copyTableId)
		{
			this->dump_or_save(indices, id == dumpId ? 0 : id == copyId ? 2 : 1, id == dumpId || id == copyTableId ? -1 : COLUMN_INDEX_PATH);
		}
		else if (id >= minID)
		{
			CMINVOKECOMMANDINFO cmd = { sizeof(cmd), CMIC_MASK_ASYNCOK, *this, reinterpret_cast<LPCSTR> (static_cast<uintptr_t> (id - minID)), nullptr, nullptr, SW_SHOW
			};

			hr = contextMenu ? contextMenu->InvokeCommand(&cmd) : S_FALSE;
			if (hr == S_OK) {}
			else
			{
				this->MessageBox(GetAnyErrorText(hr), this->LoadString(IDS_ERROR_TITLE), MB_OK | MB_ICONERROR);
			}
		}
	}

	void dump_or_save(std::vector < size_t>
		const& locked_indices, int
		const mode /*0 if dump to file, 1 if copy to clipboard, 2 if copy to clipboard as shell file list */, int
		const single_column)
	{
		std::tstring file_dialog_save_options;
		if (mode <= 0)
		{
			std::tstring
				const null_char(1, _T('\0'));	// Do NOT convert this to TCHAR because 'wchar_t' might get interpreted as 'unsigned short' depending on compiler flags
			file_dialog_save_options += this->LoadString(IDS_SAVE_OPTION_UTF8_CSV);
			file_dialog_save_options += null_char;
			file_dialog_save_options += _T("*.csv");
			file_dialog_save_options += null_char;

			file_dialog_save_options += this->LoadString(IDS_SAVE_OPTION_UTF8_TSV);
			file_dialog_save_options += null_char;
			file_dialog_save_options += _T("*.tsv");
			file_dialog_save_options += null_char;

			file_dialog_save_options += null_char;
		}

		WTL::CFileDialog fdlg(FALSE, _T("csv"), nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, file_dialog_save_options.c_str(), *this);
		fdlg.m_ofn.lpfnHook = nullptr;
		fdlg.m_ofn.Flags &= ~OFN_ENABLEHOOK;
		fdlg.m_ofn.lpstrTitle = this->LoadString(IDS_SAVE_TABLE_TITLE);
		if (mode <= 0 ? GetSaveFileName(&fdlg.m_ofn) : (locked_indices.size() <= 1 << 16 || this->MessageBox(this->LoadString(IDS_COPY_MAY_USE_TOO_MUCH_MEMORY_BODY), this->LoadString(IDS_WARNING_TITLE), MB_OKCANCEL | MB_ICONWARNING) == IDOK))
		{
			bool
				const tabsep = mode > 0 || fdlg.m_ofn.nFilterIndex > 1;
			WTL::CWaitCursor wait;
			int
				const ncolumns = this->lvFiles.GetHeader().GetItemCount();
			File
				const output = { mode <= 0 ? _topen(fdlg.m_ofn.lpstrFile, _O_BINARY | _O_TRUNC | _O_CREAT | _O_RDWR | _O_SEQUENTIAL, _S_IREAD | _S_IWRITE) : -1
			};

			if (mode > 0 || output != -1)
			{
				bool
					const shell_file_list = mode == 2;
				if (shell_file_list)
				{
					assert(single_column == COLUMN_INDEX_PATH && "code below assumes path column is the only one specified");
				}

				struct Clipboard
				{
					bool success;
					~Clipboard()
					{
						if (success)
						{
							::CloseClipboard();
						}
					}

					explicit Clipboard(HWND owner) : success(owner ? !!::OpenClipboard(owner) : false) {}

					operator bool() const
					{
						return this->success;
					}
				};

				Clipboard clipboard(output ? nullptr : this->m_hWnd);
				if (clipboard)
				{
					EmptyClipboard();
				}

				CProgressDialog dlg(this->m_hWnd);
				dlg.SetProgressTitle(this->LoadString(output ? IDS_DUMPING_TITLE : IDS_COPYING_TITLE));
				if (locked_indices.size() > 1 && dlg.HasUserCancelled())
				{
					return;
				}

				std::string line_buffer_utf8;
				SingleMovableGlobalAllocator global_alloc;
				std::tvstring line_buffer(static_cast<std::tvstring::allocator_type> (output ? nullptr : &global_alloc));
				size_t
					const buffer_size = 1 << 22;
				line_buffer.reserve(buffer_size);	// this is necessary since MSVC STL reallocates poorly, degenerating into O(n^2)
				if (shell_file_list)
				{
					WTL::CPoint pt;
					GetCursorPos(&pt);
					DROPFILES df = { sizeof(df), pt, FALSE, sizeof(*line_buffer.data()) > sizeof(char)
					};

					line_buffer.append(reinterpret_cast<TCHAR
						const*> (&df), sizeof(df) / sizeof(TCHAR));
				}

				if (!output && (single_column == COLUMN_INDEX_PATH || single_column < 0))
				{
					line_buffer.reserve(locked_indices.size() * MAX_PATH * (single_column >= 0 ? 2 : 3) / 4);
				}

				std::vector<int> displayed_columns(ncolumns, -1);
				this->lvFiles.GetColumnOrderArray(static_cast<int> (displayed_columns.size()), &*displayed_columns.begin());
				unsigned long long nwritten_since_update = 0;
				unsigned long long prev_update_time = GetTickCount64();
				try
				{
					bool warned_about_ads = false;
					for (size_t i = 0; i < locked_indices.size(); ++i)
					{
						bool should_flush = i + 1 >= locked_indices.size();
						size_t
							const entry_begin_offset = line_buffer.size();
						bool any = false;
						bool is_ads = false;
						for (int c = 0; c < (single_column >= 0 ? 1 : ncolumns); ++c)
						{
							int
								const j = single_column >= 0 ? single_column : displayed_columns[c];
							if (j == COLUMN_INDEX_NAME)
							{
								continue;
							}

							if (any)
							{
								line_buffer.push_back(tabsep ? _T('\t') : _T(','));
							}

							size_t
								const begin_offset = line_buffer.size();
							this->GetSubItemText(locked_indices[i], j, false, line_buffer, false);
							if (j == COLUMN_INDEX_PATH)
							{
								size_t last_colon_index = line_buffer.size();
								for (size_t k = begin_offset; k != line_buffer.size(); ++k)
								{
									if (line_buffer[k] == _T(':'))
									{
										last_colon_index = k;
									}
								}

								TCHAR
									const first_char = begin_offset < line_buffer.size() ? line_buffer[begin_offset] : _T('\0');
								// TODO: This is broken currently
								is_ads = is_ads || (begin_offset <= last_colon_index && last_colon_index < line_buffer.size() && !(last_colon_index == begin_offset + 1 && ((_T('a') <= first_char && first_char <= _T('z')) || (_T('A') <= first_char && first_char <= _T('Z'))) && line_buffer.size() > last_colon_index + 1 && line_buffer.at(last_colon_index + 1) == _T('\\'))) /*TODO: This will fail if we later support paths like \\.\C: */;
								unsigned long long
									const update_time = GetTickCount64();
								if (dlg.ShouldUpdate(update_time) || i + 1 == locked_indices.size())
								{
									if (dlg.HasUserCancelled(update_time))
									{
										throw CStructured_Exception(ERROR_CANCELLED, nullptr);
									}

									should_flush = true;
									basic_fast_ostringstream<TCHAR> ss;
									ss << this->LoadString(output ? IDS_TEXT_DUMPING_SELECTION : IDS_TEXT_COPYING_SELECTION) << this->LoadString(IDS_TEXT_SPACE);
									ss << nformat_ui(i + 1);
									ss << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_OF) << this->LoadString(IDS_TEXT_SPACE);
									ss << nformat_ui(locked_indices.size());
									if (update_time != prev_update_time)
									{
										ss << this->LoadString(IDS_TEXT_SPACE);
										ss << this->LoadString(IDS_TEXT_PAREN_OPEN);
										ss << nformat_ui(nwritten_since_update * 1000U / ((update_time - prev_update_time) * 1ULL << 20));
										ss << this->LoadString(IDS_TEXT_SPACE) << this->LoadString(IDS_TEXT_MIB_S);
										ss << this->LoadString(IDS_TEXT_PAREN_CLOSE);
									}

									ss << this->LoadString(IDS_TEXT_COLON);
									ss << _T('\n');
									ss.append(line_buffer.data() + static_cast<ptrdiff_t> (begin_offset), line_buffer.size() - begin_offset);
									std::tstring
										const& text = ss.str();
									dlg.SetProgressText(text);
									dlg.SetProgress(static_cast<long long> (i), static_cast<long long> (locked_indices.size()));
									dlg.Flush();
								}
							}

							// NOTE: We assume there are no double-quotes here, so we don't handle escaping for that! This is a valid assumption for this program.
							if (tabsep)
							{
								bool may_contain_tabs = false;
								if (may_contain_tabs && line_buffer.find(_T('\t'), begin_offset) != std::tstring::npos)
								{
									line_buffer.insert(line_buffer.begin() + static_cast<ptrdiff_t> (begin_offset), _T('\"'));
									line_buffer.push_back(_T('\"'));
								}
							}
							else
							{
								if (line_buffer.find(_T(','), begin_offset) != std::tstring::npos ||
									line_buffer.find(_T('\''), begin_offset) != std::tstring::npos)
								{
									line_buffer.insert(line_buffer.begin() + static_cast<ptrdiff_t> (begin_offset), _T('\"'));
									line_buffer.push_back(_T('\"'));
								}
							}

							any = true;
						}

						if (shell_file_list)
						{
							line_buffer.push_back(_T('\0'));
						}
						else if (i < locked_indices.size() - 1)
						{
							line_buffer.push_back(_T('\r'));
							line_buffer.push_back(_T('\n'));
						}

						if (shell_file_list && is_ads)
						{
							if (!warned_about_ads)
							{
								unsigned long long
									const tick_before = GetTickCount64();
								ATL::CWindow(dlg.IsWindow() ? dlg.GetHWND() : this->m_hWnd).MessageBox(this->LoadString(IDS_COPYING_ADS_PROBLEM_BODY), this->LoadString(IDS_WARNING_TITLE), MB_OK | MB_ICONWARNING);
								prev_update_time += GetTickCount64() - tick_before;
								warned_about_ads = true;
							}

							line_buffer.erase(line_buffer.begin() + static_cast<ptrdiff_t> (entry_begin_offset), line_buffer.end());
						}

						should_flush |= line_buffer.size() >= buffer_size;
						if (should_flush)
						{
							if (output)
							{
#if defined(_UNICODE) &&_UNICODE
									using std::max;
								line_buffer_utf8.resize(max(line_buffer_utf8.size(), (line_buffer.size() + 1) * 6), _T('\0'));
								int
									const cch = WideCharToMultiByte(CP_UTF8, 0, line_buffer.empty() ? nullptr : &line_buffer[0], static_cast<int> (line_buffer.size()), &line_buffer_utf8[0], static_cast<int> (line_buffer_utf8.size()), nullptr, nullptr);
								if (cch > 0)
								{
									nwritten_since_update += _write(output, line_buffer_utf8.data(), sizeof(*line_buffer_utf8.data()) * static_cast<size_t> (cch));
								}
#else
									nwritten_since_update += _write(output, line_buffer.data(), sizeof(*line_buffer.data()) * line_buffer.size()); 
#endif
									line_buffer.clear();
							}
							else
							{
								nwritten_since_update = line_buffer.size() * sizeof(*line_buffer.begin());
							}
						}
					}

					if (clipboard)
					{
						unsigned int
							const format = shell_file_list ? CF_HDROP : sizeof(*line_buffer.data()) > sizeof(char) ? CF_UNICODETEXT : CF_TEXT;
						HGLOBAL const hGlobal = GlobalHandle(line_buffer.c_str());
						if (hGlobal)
						if (HGLOBAL
							const resized = GlobalReAlloc(hGlobal, (line_buffer.size() + 1) * sizeof(*line_buffer.data()), 0))
						{
							HANDLE
								const result = SetClipboardData(format, resized);
							// Clear the string, because we put it in an invalid state -- the underlying buffer is too small now!
							std::tvstring(line_buffer.get_allocator()).swap(line_buffer);
							if (result == resized)
							{
								(void)global_alloc.disown(result);
							}
						}
					}
				}

				catch (CStructured_Exception& ex)
				{
					if (ex.GetSENumber() != ERROR_CANCELLED)
					{
						throw;
					}
				}
			}
		}
	}

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

	void GetSubItemText(size_t
		const j, int
		const subitem, bool
		const for_ui, std::tvstring& text, bool
		const lock_index = true) const
	{
		lock_ptr < NtfsIndex
			const > i(this->results.item_index(j), lock_index);
		if (i->records_so_far() < this->results[j].key().frs())
		{
			__debugbreak();
		}

		NFormat
			const& nformat = for_ui ? nformat_ui : nformat_io;
		long long svalue;
		unsigned long long uvalue;
		//std::string positive; positive.assign("+");
		NtfsIndex::key_type
			const key = this->results[j].key();
		NtfsIndex::key_type::frs_type
			const frs = key.frs();
		switch (subitem)
		{
			//#####
		case COLUMN_INDEX_NAME:
			i->get_path(key, text, true);
			deldirsep(text);
			break;
		case COLUMN_INDEX_PATH:
			text += i->root_path();
			i->get_path(key, text, false);
			break;
		case COLUMN_INDEX_TYPE:
		{
			size_t
				const k = text.size(); text += i->root_path(); unsigned int attributes = 0; i->get_path(key, text, true, &attributes); this->get_file_type_blocking(text, k, attributes);
		}

		break;
		case COLUMN_INDEX_SIZE:
			uvalue = static_cast<unsigned long long> (i->get_sizes(key).length);
			text += nformat(uvalue);
			break;
		case COLUMN_INDEX_SIZE_ON_DISK:
			uvalue = static_cast<unsigned long long> (i->get_sizes(key).allocated);
			text += nformat(uvalue);
			break;
		case COLUMN_INDEX_CREATION_TIME:
			svalue = i->get_stdinfo(frs).created;
			SystemTimeToString(svalue, text, !for_ui);
			break;
		case COLUMN_INDEX_MODIFICATION_TIME:
			svalue = i->get_stdinfo(frs).written;
			SystemTimeToString(svalue, text, !for_ui);
			break;
		case COLUMN_INDEX_ACCESS_TIME:
			svalue = i->get_stdinfo(frs).accessed;
			SystemTimeToString(svalue, text, !for_ui);
			break;
		case COLUMN_INDEX_DESCENDENTS:
			uvalue = static_cast<unsigned long long> (i->get_sizes(key).treesize);
			if (uvalue)
			{
				text += nformat(uvalue - 1);
			}

			break;
		case COLUMN_INDEX_is_readonly:
			if (i->get_stdinfo(frs).is_readonly        > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_archive:
			if (i->get_stdinfo(frs).is_archive         > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_system:
			if (i->get_stdinfo(frs).is_system          > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_hidden:
			if (i->get_stdinfo(frs).is_hidden          > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_offline:
			if (i->get_stdinfo(frs).is_offline         > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_notcontentidx:
			if (i->get_stdinfo(frs).is_notcontentidx   > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_noscrubdata:
			if (i->get_stdinfo(frs).is_noscrubdata     > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_integretystream:
			if (i->get_stdinfo(frs).is_integretystream > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_pinned:
			if (i->get_stdinfo(frs).is_pinned          > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_unpinned:
			if (i->get_stdinfo(frs).is_unpinned        > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_directory:
			if (i->get_stdinfo(frs).is_directory       > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_compressed:
			if (i->get_stdinfo(frs).is_compressed      > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_encrypted:
			if (i->get_stdinfo(frs).is_encrypted       > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_sparsefile:
			if (i->get_stdinfo(frs).is_sparsefile      > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_is_reparsepoint:
			if (i->get_stdinfo(frs).is_reparsepoint    > 0) text.push_back(_T('+'));
			break;
		case COLUMN_INDEX_ATTRIBUTE:
			uvalue = i->get_stdinfo(frs).attributes();
			text += nformat(uvalue);
			break;

			// #####
		default:
			break;
		}
	}

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

		_tcsncpy(nid.szTip, this->LoadString(IDS_APPNAME), sizeof(nid.szTip) / sizeof(*nid.szTip));
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
			body.Format(this->LoadString(IDS_HELP_DONATE_BODY), this->get_project_url(IDS_PROJECT_USER_FRIENDLY_URL).c_str());
			title = this->LoadString(IDS_HELP_DONATE_TITLE);
			type  = ((type & ~static_cast<unsigned int> (MB_OK)) | MB_OKCANCEL);
			break;
		case ID_HELP_TRANSLATION:
			body.Format(this->LoadString(IDS_HELP_TRANSLATION_BODY), static_cast<LPCTSTR> (get_ui_locale_name()));
			title = this->LoadString(IDS_HELP_TRANSLATION_TITLE);
			type  = ((type & ~static_cast<unsigned int> (MB_OK)) | MB_OKCANCEL);
			break;
		case ID_HELP_COPYING:
			body  = this->LoadString(IDS_HELP_COPYING_BODY);
			title = this->LoadString(IDS_HELP_COPYING_TITLE);
			break;
		case ID_HELP_NTFSMETADATA:
			body  = this->LoadString(IDS_HELP_NTFS_METADATA_BODY);
			title = this->LoadString(IDS_HELP_NTFS_METADATA_TITLE);
			break;
		case ID_HELP_SEARCHINGBYDEPTH:
			body  = this->LoadString(IDS_HELP_SEARCHING_BY_DEPTH_BODY);
			title = this->LoadString(IDS_HELP_SEARCHING_BY_DEPTH_TITLE);
			break;
		case ID_HELP_SORTINGBYBULKINESS:
			body  = this->LoadString(IDS_HELP_SORTING_BY_BULKINESS_BODY);
			title = this->LoadString(IDS_HELP_SORTING_BY_BULKINESS_TITLE);
			break;
		case ID_HELP_SORTINGBYDEPTH:
			body  = this->LoadString(IDS_HELP_SORTING_BY_DEPTH_BODY);
			title = this->LoadString(IDS_HELP_SORTING_BY_DEPTH_TITLE);
			break;
		case ID_HELP_USINGREGULAREXPRESSIONS:
			body  = this->LoadString(IDS_HELP_REGEX_BODY);
			title = this->LoadString(IDS_HELP_REGEX_TITLE);
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
		body.Format(this->LoadString(IDS_TEXT_REPORT_ISSUES), this->get_project_url(IDS_PROJECT_USER_FRIENDLY_URL).c_str());
		{
			body += L"\u2022";  // Unicode bullet character
			body += this->LoadString(IDS_TEXT_SPACE);
			body += this->LoadString(IDS_TEXT_UI_LOCALE_NAME);
			body += this->LoadString(IDS_TEXT_SPACE);
			body += get_ui_locale_name();
		}

		body += _T("\r\n");
		{
			std::tvstring buf_localized, buf_invariant;
			SystemTimeToString(ticks, buf_localized, false, false);
			SystemTimeToString(ticks, buf_invariant, true, false);
			body += L"\u2022";  // Unicode bullet character
			body += this->LoadString(IDS_TEXT_SPACE);
			body += this->LoadString(IDS_TEXT_BUILD_DATE);
			body += this->LoadString(IDS_TEXT_SPACE);
			body += buf_localized.c_str();
			body += this->LoadString(IDS_TEXT_SPACE);
			body += this->LoadString(IDS_TEXT_PAREN_OPEN);
			body += buf_invariant.c_str();
			body += this->LoadString(IDS_TEXT_PAREN_CLOSE);
		}

		if (this->MessageBox(body, this->LoadString(IDS_HELP_BUGS_TITLE), MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
		{
			this->open_project_page();
		}
	}

	RefCountedCString get_project_url(unsigned short
		const id)
	{
		RefCountedCString result;
		result.Format(this->LoadString(id), this->LoadString(IDS_PROJECT_NAME).c_str());
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

	void Refresh(bool
		const initial)
	{
		if (!initial && this->indices_created < this->cmbDrive.GetCount() - 1)
		{
			// Ignore the request due to potential race condition... at least wait until all the threads have started!
			// Otherwise, if the user presses F5 we will delete some entries, which will invalidate threads' copies of their index in the combobox
			return;
		}

		this->clear(true, true);
		int
			const selected = this->cmbDrive.GetCurSel();
		for (int ii = 0; ii < this->cmbDrive.GetCount(); ++ii)
		{
			if (selected == 0 || ii == selected)
			{
				intrusive_ptr<NtfsIndex> q = static_cast<NtfsIndex*> (this->cmbDrive.GetItemDataPtr(ii));
				if (q || (initial && ii != 0))
				{
					std::tvstring path_name;
					if (initial)
					{
						WTL::CString path_name_;
						this->cmbDrive.GetLBText(ii, path_name_);
						path_name = static_cast<LPCTSTR> (path_name_);
					}
					else
					{
						path_name = q->root_path();
						q->cancel();
						if (this->cmbDrive.SetItemDataPtr(ii, nullptr) != CB_ERR)
						{
							--this->indices_created;
							intrusive_ptr_release(q.get());
						}
					}

					q.reset(new NtfsIndex(path_name), true);
					struct OverlappedNtfsMftReadPayloadDerived : OverlappedNtfsMftReadPayload
					{
						HWND m_hWnd;
						explicit OverlappedNtfsMftReadPayloadDerived(IoCompletionPort volatile& iocp, intrusive_ptr < NtfsIndex volatile > p, HWND
							const& m_hWnd, Handle
							const& closing_event) : OverlappedNtfsMftReadPayload(iocp, p, closing_event), m_hWnd(m_hWnd) {}

						void preopen() override
						{
							this->OverlappedNtfsMftReadPayload::preopen();
							if (this->m_hWnd)
							{
								DEV_BROADCAST_HANDLE dev = { sizeof(dev), DBT_DEVTYP_HANDLE, 0, p->volume(), reinterpret_cast<HDEVNOTIFY> (this->m_hWnd)
								};

								dev.dbch_hdevnotify = RegisterDeviceNotification(this->m_hWnd, &dev, DEVICE_NOTIFY_WINDOW_HANDLE);
							}
						}
					};

					intrusive_ptr<OverlappedNtfsMftReadPayload> p(new OverlappedNtfsMftReadPayloadDerived(this->iocp, q, this->m_hWnd, this->closing_event));
					this->iocp.post(0, static_cast<uintptr_t> (ii), p);
					if (this->cmbDrive.SetItemDataPtr(ii, q.get()) != CB_ERR)
					{
						(void)q.detach();  // Intentionally release ownership
						++this->indices_created;
					}
				}
			}
		}
	}

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
