# GUI Implementation

## Introduction

This document provides exhaustive detail on how Ultra Fast File Search implements its Windows GUI. After reading this document, you should be able to:

1. Build a virtual list view that displays millions of items efficiently
2. Implement owner-draw custom rendering with file attribute colors
3. Create thread-safe UI updates from background threads
4. Integrate Windows Shell context menus and drag-drop
5. Implement background icon loading with caching

---

## Overview: GUI Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         GUI Architecture                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      CMainDlg                                    │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │ CModifiedDialogImpl<CMainDlg>                           │    │   │
│  │  │   - Dialog template management                          │    │   │
│  │  │   - RTL layout support                                  │    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │ WTL::CDialogResize<CMainDlg>                            │    │   │
│  │  │   - Automatic control resizing                          │    │   │
│  │  │   - Anchor points for controls                          │    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │ CInvokeImpl<CMainDlg>                                   │    │   │
│  │  │   - Thread-safe UI updates                              │    │   │
│  │  │   - PostMessage-based invocation                        │    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  │  ┌─────────────────────────────────────────────────────────┐    │   │
│  │  │ WTL::CMessageFilter                                     │    │   │
│  │  │   - Keyboard accelerators                               │    │   │
│  │  │   - Message preprocessing                               │    │   │
│  │  └─────────────────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Child Controls:                                                        │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐   │
│  │ CSearchPattern│ │ CComboBox   │ │CThemedListView│ │ CStatusBar  │   │
│  │ (Edit control)│ │ (Drives)    │ │ (Results)    │ │ (Status)    │   │
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘   │
│                                                                         │
│  Background Services:                                                   │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ BackgroundWorker (iconLoader)  │  OleIoCompletionPort (iocp)     │  │
│  │   - Async icon extraction      │    - Async MFT reading          │  │
│  │   - Priority queue             │    - COM-aware threads          │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## CMainDlg: Main Window Class

### Class Definition

```cpp
class CMainDlg :
    public CModifiedDialogImpl<CMainDlg>,      // Dialog base
    public WTL::CDialogResize<CMainDlg>,       // Resizing support
    public CInvokeImpl<CMainDlg>,              // Thread-safe invocation
    private WTL::CMessageFilter                 // Message filtering
{
    // Control IDs
    enum { IDC_STATUS_BAR = 1100 };

    // Column indices for list view
    enum {
        COLUMN_INDEX_NAME,
        COLUMN_INDEX_PATH,
        COLUMN_INDEX_TYPE,
        COLUMN_INDEX_SIZE,
        COLUMN_INDEX_SIZE_ON_DISK,
        COLUMN_INDEX_CREATION_TIME,
        COLUMN_INDEX_MODIFICATION_TIME,
        COLUMN_INDEX_ACCESS_TIME,
        COLUMN_INDEX_DESCENDENTS,
        COLUMN_INDEX_is_readonly,
        COLUMN_INDEX_is_archive,
        COLUMN_INDEX_is_system,
        COLUMN_INDEX_is_hidden,
        COLUMN_INDEX_is_offline,
        COLUMN_INDEX_is_not_indexed,
        COLUMN_INDEX_is_pinned,
        COLUMN_INDEX_is_unpinned,
        COLUMN_INDEX_is_directory,
        COLUMN_INDEX_is_compressed,
        COLUMN_INDEX_is_encrypted,
        COLUMN_INDEX_is_sparsefile,
        COLUMN_INDEX_is_reparsepoint,
        COLUMN_INDEX_ATTRIBUTE
    };

    // Member variables
    Results results;                           // Search results
    CThemedListViewCtrl lvFiles;               // Virtual list view
    WTL::CStatusBarCtrl statusbar;             // Status bar
    WTL::CComboBox cmbDrive;                   // Drive selector
    CSearchPattern txtPattern;                 // Search pattern edit

    // Image lists for icons
    WTL::CImageList imgListSmall;              // 16x16 icons
    WTL::CImageList imgListLarge;              // 32x32 icons
    WTL::CImageList imgListExtraLarge;         // 48x48 icons
    WTL::CImageList _small_image_list;         // Currently active small list

    // Background services
    intrusive_ptr<BackgroundWorker> iconLoader;  // Async icon loading
    OleIoCompletionPort iocp;                    // Async I/O
    Handle closing_event;                        // Shutdown signal

    // Caching
    ShellInfoCache cache;                      // Icon/type cache
    std::map<std::tvstring, std::tvstring> type_cache;  // File type names

    // Colors for file attributes
    COLORREF deletedColor;      // Gray for deleted
    COLORREF encryptedColor;    // Green for encrypted
    COLORREF compressedColor;   // Blue for compressed
    COLORREF directoryColor;    // Orange for directories
    COLORREF hiddenColor;       // Pink for hidden
    COLORREF systemColor;       // Red for system

    // Sorting state
    struct {
        int column;
        unsigned char variation;
        int counter;
    } last_sort;
};
```

### Constructor

```cpp
CMainDlg(HANDLE const hEvent, bool const rtl) :
    CModifiedDialogImpl<CMainDlg>(true, rtl),
    iconLoader(BackgroundWorker::create(true, global_exception_handler)),
    closing_event(CreateEvent(NULL, TRUE, FALSE, NULL)),
    nformat_ui(std::locale("")),
    nformat_io(std::locale()),
    lcid(GetThreadLocale()),
    hEvent(hEvent),
    deletedColor(RGB(0xC0, 0xC0, 0xC0)),
    encryptedColor(RGB(0, 0xFF, 0)),
    compressedColor(RGB(0, 0, 0xFF)),
    directoryColor(RGB(0xFF, 0x99, 0x33)),
    hiddenColor(RGB(0xFF, 0x99, 0x99)),
    systemColor(RGB(0xFF, 0, 0))
{
    this->time_zone_bias = get_time_zone_bias();
}
```

---

## Virtual List View

### Why Virtual List View?

A virtual list view (owner-data) only stores the data for visible items, not all items. This is essential for displaying millions of search results:

| Approach | Memory for 1M items | Scroll Performance |
|----------|--------------------|--------------------|
| Standard ListView | ~100 MB+ | Slow (all items in memory) |
| Virtual ListView | ~10 KB | Instant (only visible items) |

### Setup

```cpp
BOOL OnInitDialog(HWND hWnd, LPARAM lParam) {
    // Create list view with LVS_OWNERDATA style
    this->lvFiles.SubclassWindow(GetDlgItem(IDC_LISTFILES));

    // Enable virtual mode and extended styles
    this->lvFiles.SetExtendedListViewStyle(
        LVS_EX_DOUBLEBUFFER |      // Flicker-free drawing
        LVS_EX_FULLROWSELECT |     // Select entire row
        LVS_EX_LABELTIP |          // Show tooltips for truncated text
        LVS_EX_HEADERDRAGDROP |    // Reorderable columns
        LVS_EX_GRIDLINES           // Optional grid lines
    );

    // Set up image lists
    this->imgListSmall.Create(16, 16, ILC_COLOR32 | ILC_MASK, 100, 100);
    this->imgListLarge.Create(32, 32, ILC_COLOR32 | ILC_MASK, 100, 100);
    this->imgListExtraLarge.Create(48, 48, ILC_COLOR32 | ILC_MASK, 100, 100);

    this->lvFiles.SetImageList(this->imgListSmall, LVSIL_SMALL);
    this->lvFiles.SetImageList(this->imgListLarge, LVSIL_NORMAL);

    // Add columns
    this->lvFiles.InsertColumn(COLUMN_INDEX_NAME, _T("Name"), LVCFMT_LEFT, 200);
    this->lvFiles.InsertColumn(COLUMN_INDEX_PATH, _T("Path"), LVCFMT_LEFT, 300);
    this->lvFiles.InsertColumn(COLUMN_INDEX_TYPE, _T("Type"), LVCFMT_LEFT, 100);
    this->lvFiles.InsertColumn(COLUMN_INDEX_SIZE, _T("Size"), LVCFMT_RIGHT, 80);
    // ... more columns

    return TRUE;
}
```

### Setting Item Count

```cpp
BOOL SetItemCount(int n, bool assume_existing_are_valid = false) {
    BOOL result;

    // Flush any pending row invalidations
    this->update_and_flush_invalidated_rows();

    if (this->lvFiles.GetStyle() & LVS_OWNERDATA) {
        // Virtual mode: just set the count
        result = this->lvFiles.SetItemCount(n);
    }

    return result;
}

void update_and_flush_invalidated_rows() {
    // Sort invalidated rows for efficient batch redraw
    std::sort(this->invalidated_rows.begin(), this->invalidated_rows.end());

    // Coalesce adjacent rows into ranges
    for (size_t i = 0; i != this->invalidated_rows.size(); ++i) {
        int j1 = this->invalidated_rows[i], j2 = j1;

        // Extend range while rows are consecutive
        while (i + 1 < this->invalidated_rows.size() &&
               this->invalidated_rows[i + 1] == j2 + 1) {
            ++i;
            ++j2;
        }

        // Redraw the range
        this->lvFiles.RedrawItems(j1, j2);
    }

    this->invalidated_rows.clear();
}
```

### LVN_GETDISPINFO: Providing Data on Demand

Windows sends `LVN_GETDISPINFO` when it needs to display an item:

```cpp
LRESULT OnFilesGetDispInfo(LPNMHDR pnmh) {
    NMLVDISPINFO* const pLV = (NMLVDISPINFO*)pnmh;

    size_t const j = static_cast<size_t>(pLV->item.iItem);
    if (j >= this->results.size()) return 0;

    // Lock the result for thread-safe access
    LockedResults locked_results(*this);
    locked_results(j);

    // Provide text if requested
    if (pLV->item.mask & LVIF_TEXT) {
        this->GetSubItemText(j, pLV->item.iSubItem, true, this->temp_text);
        pLV->item.pszText = const_cast<TCHAR*>(this->temp_text.c_str());
    }

    // Provide icon if requested (first column only)
    if ((pLV->item.mask & LVIF_IMAGE) && pLV->item.iSubItem == 0) {
        WTL::CRect rc, rcitem;
        this->lvFiles.GetClientRect(&rc);
        if (this->lvFiles.GetItemRect(pLV->item.iItem, &rcitem, LVIR_ICON) &&
            rcitem.IntersectRect(&rcitem, &rc)) {

            std::tvstring path;
            this->GetSubItemText(j, COLUMN_INDEX_PATH, true, path);
            unsigned long attrs = lock(this->results.item_index(j))
                ->get_stdinfo(this->results[j].key().frs()).attributes();

            int iImage = this->CacheIcon(path, pLV->item.iItem, attrs, GetMessageTime());
            if (iImage >= 0) {
                pLV->item.iImage = iImage;
            }
        }
    }

    return 0;
}
```

---

## Owner-Draw Custom Rendering

### NM_CUSTOMDRAW: Custom Colors

UFFS uses custom draw to color-code files based on their attributes:

```cpp
LRESULT OnFilesListCustomDraw(LPNMHDR pnmh) {
    LRESULT result = CDRF_DODEFAULT;

    // Color definitions
    COLORREF const deletedColor    = RGB(0xC0, 0xC0, 0xC0);  // Gray
    COLORREF const encryptedColor  = RGB(0x00, 0xFF, 0x00);  // Green
    COLORREF const compressedColor = RGB(0x00, 0x00, 0xFF);  // Blue
    COLORREF const directoryColor  = RGB(0xFF, 0x99, 0x33);  // Orange
    COLORREF const hiddenColor     = RGB(0xFF, 0x99, 0x99);  // Pink
    COLORREF const systemColor     = RGB(0xFF, 0x00, 0x00);  // Red
    COLORREF const sparseColor     = RGB(0x00, 0x7F, 0x7F);  // Teal

    LPNMLVCUSTOMDRAW const pLV = (LPNMLVCUSTOMDRAW)pnmh;

    if (pLV->nmcd.dwItemSpec >= this->results.size()) {
        return result;
    }

    size_t const iresult = static_cast<size_t>(pLV->nmcd.dwItemSpec);
    unsigned long const attrs = lock(this->results.item_index(iresult))
        ->get_stdinfo(this->results[iresult].key().frs()).attributes();

    switch (pLV->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            // Request per-item notifications
            result = CDRF_NOTIFYITEMDRAW;
            break;

        case CDDS_ITEMPREPAINT:
            // Request per-subitem notifications
            result = CDRF_NOTIFYSUBITEMDRAW;
            break;

        case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
            // Set text color based on attributes (priority order)
            if (attrs & FILE_ATTRIBUTE_ENCRYPTED) {
                pLV->clrText = encryptedColor;
            } else if (attrs & FILE_ATTRIBUTE_COMPRESSED) {
                pLV->clrText = compressedColor;
            } else if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                pLV->clrText = directoryColor;
            } else if (attrs & FILE_ATTRIBUTE_HIDDEN) {
                pLV->clrText = hiddenColor;
            } else if (attrs & FILE_ATTRIBUTE_SYSTEM) {
                pLV->clrText = systemColor;
            } else if (attrs & FILE_ATTRIBUTE_SPARSE_FILE) {
                pLV->clrText = sparseColor;
            }

            result = CDRF_NEWFONT;
            break;
    }

    return result;
}
```

---

## Thread-Safe UI Updates: CInvokeImpl

### The Problem

Background threads (icon loading, MFT reading) need to update the UI, but Windows UI controls can only be modified from the thread that created them.

### Solution: PostMessage-Based Invocation

```cpp
template<typename T>
class CInvokeImpl {
    // Custom message for invoking callbacks
    static UINT const WM_INVOKE = WM_USER + 100;

    // Thunk type for callbacks
    typedef std::function<void()> Thunk;

    // Queue of pending callbacks
    std::deque<Thunk> pending_invokes;
    atomic_namespace::recursive_mutex invoke_mutex;

public:
    // Called from any thread to execute code on UI thread
    template<typename F>
    void invoke(F&& func) {
        {
            unique_lock<recursive_mutex> guard(this->invoke_mutex);
            this->pending_invokes.push_back(std::forward<F>(func));
        }

        // Post message to UI thread
        static_cast<T*>(this)->PostMessage(WM_INVOKE);
    }

    // Message handler (runs on UI thread)
    LRESULT OnInvoke(UINT, WPARAM, LPARAM) {
        Thunk thunk;
        {
            unique_lock<recursive_mutex> guard(this->invoke_mutex);
            if (!this->pending_invokes.empty()) {
                thunk = std::move(this->pending_invokes.front());
                this->pending_invokes.pop_front();
            }
        }

        // Execute callback on UI thread
        if (thunk) {
            thunk();
        }

        return 0;
    }

    // Add to message map
    BEGIN_MSG_MAP_EX(CInvokeImpl)
        MESSAGE_HANDLER_EX(WM_INVOKE, OnInvoke)
    END_MSG_MAP()
};
```

### Usage Example

```cpp
// In background icon loader callback
void IconLoaderCallback::operator()() {
    // This runs on background thread
    SHFILEINFO sfi = {};
    BOOL success = SHGetFileInfo(path.c_str(), fileAttributes, &sfi,
                                  sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON);

    // Create callback for UI thread
    MainThreadCallback mtc = { this_, path, WTL::CIcon(sfi.hIcon), iItem, success };

    // Invoke on UI thread
    this_->invoke([mtc = std::move(mtc)]() mutable {
        mtc();  // This runs on UI thread
    });
}
```

---

## Background Icon Loading

### Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Icon Loading Pipeline                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  LVN_GETDISPINFO                                                        │
│       │                                                                 │
│       ▼                                                                 │
│  ┌─────────────────┐     Cache Hit?     ┌─────────────────┐            │
│  │   CacheIcon()   │ ─────────────────► │ Return cached   │            │
│  └─────────────────┘        Yes         │ icon index      │            │
│       │ No                              └─────────────────┘            │
│       ▼                                                                 │
│  ┌─────────────────┐                                                   │
│  │ Add placeholder │                                                   │
│  │ to cache        │                                                   │
│  └─────────────────┘                                                   │
│       │                                                                 │
│       ▼                                                                 │
│  ┌─────────────────┐                    ┌─────────────────┐            │
│  │ Post to         │ ──────────────────►│ BackgroundWorker│            │
│  │ iconLoader      │                    │ (priority queue)│            │
│  └─────────────────┘                    └─────────────────┘            │
│                                                │                        │
│                                                ▼                        │
│                                         ┌─────────────────┐            │
│                                         │ SHGetFileInfo() │            │
│                                         │ (COM required)  │            │
│                                         └─────────────────┘            │
│                                                │                        │
│                                                ▼                        │
│  ┌─────────────────┐                    ┌─────────────────┐            │
│  │ Update cache    │ ◄──────────────────│ invoke() to     │            │
│  │ Invalidate row  │    UI Thread       │ UI thread       │            │
│  └─────────────────┘                    └─────────────────┘            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Icon Cache Structure

```cpp
struct CacheInfo {
    int iIconSmall;      // Index in small image list
    int iIconLarge;      // Index in large image list
    std::tvstring type;  // File type description

    CacheInfo(size_t index) : iIconSmall(index), iIconLarge(index) {}
};

// Cache keyed by reversed path (for efficient prefix matching)
typedef std::map<std::tvstring, CacheInfo> ShellInfoCache;
ShellInfoCache cache;
```

### CacheIcon Implementation

```cpp
int CacheIcon(std::tvstring path, int iItem, unsigned long fileAttributes,
              DWORD timestamp) {
    // Reverse path for cache key (enables prefix matching)
    std::reverse(path.begin(), path.end());

    // Check cache
    ShellInfoCache::iterator entry = this->cache.find(path);

    if (entry != this->cache.end()) {
        // Cache hit
        return entry->second.iIconSmall;
    }

    // Cache miss - add placeholder and queue background load
    entry = this->cache.insert(this->cache.end(),
        ShellInfoCache::value_type(path, CacheInfo(this->cache.size())));
    std::reverse(path.begin(), path.end());

    // Get icon sizes
    SIZE iconSmallSize, iconLargeSize;
    this->imgListSmall.GetIconSize(iconSmallSize);
    this->imgListLarge.GetIconSize(iconLargeSize);

    // Queue background icon load
    IconLoaderCallback callback = {
        this, path, iconSmallSize, iconLargeSize, fileAttributes, iItem
    };
    this->iconLoader->add(callback, timestamp);

    return entry->second.iIconSmall;
}
```



---

## Shell Integration

### Context Menus

UFFS integrates with Windows Explorer's context menus using the Shell API:

```cpp
void OnContextMenu(HWND hWnd, WTL::CPoint point) {
    // Get selected items
    std::vector<std::tvstring> selectedPaths;
    int iItem = -1;
    while ((iItem = this->lvFiles.GetNextItem(iItem, LVNI_SELECTED)) != -1) {
        std::tvstring path;
        this->GetSubItemText(iItem, COLUMN_INDEX_PATH, true, path);
        selectedPaths.push_back(path);
    }

    if (selectedPaths.empty()) return;

    // Create IShellFolder for parent
    CComPtr<IShellFolder> pDesktop;
    SHGetDesktopFolder(&pDesktop);

    // Build PIDL array for selected items
    std::vector<LPITEMIDLIST> pidls;
    for (const auto& path : selectedPaths) {
        LPITEMIDLIST pidl;
        pDesktop->ParseDisplayName(hWnd, NULL,
            const_cast<LPWSTR>(path.c_str()), NULL, &pidl, NULL);
        pidls.push_back(pidl);
    }

    // Get context menu
    CComPtr<IContextMenu> pContextMenu;
    pDesktop->GetUIObjectOf(hWnd, pidls.size(),
        (LPCITEMIDLIST*)pidls.data(), IID_IContextMenu, NULL,
        (void**)&pContextMenu);

    // Create and populate menu
    HMENU hMenu = CreatePopupMenu();
    pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL);

    // Show menu and get selection
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             point.x, point.y, 0, hWnd, NULL);

    if (cmd > 0) {
        // Execute selected command
        CMINVOKECOMMANDINFO ici = { sizeof(ici) };
        ici.hwnd = hWnd;
        ici.lpVerb = MAKEINTRESOURCEA(cmd - 1);
        ici.nShow = SW_SHOWNORMAL;
        pContextMenu->InvokeCommand(&ici);
    }

    DestroyMenu(hMenu);

    // Free PIDLs
    for (auto pidl : pidls) {
        CoTaskMemFree(pidl);
    }
}
```

### Drag and Drop

UFFS supports dragging files from the results list:

```cpp
class CDropSource : public IDropSource {
    ULONG refCount;

public:
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) {
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() { return ++refCount; }
    STDMETHOD_(ULONG, Release)() {
        if (--refCount == 0) { delete this; return 0; }
        return refCount;
    }

    // IDropSource
    STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD grfKeyState) {
        if (fEscapePressed) return DRAGDROP_S_CANCEL;
        if (!(grfKeyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }

    STDMETHOD(GiveFeedback)(DWORD dwEffect) {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }
};

void OnBeginDrag(LPNMHDR pnmh) {
    LPNMLISTVIEW pLV = (LPNMLISTVIEW)pnmh;

    // Build list of selected file paths
    std::vector<std::tvstring> paths;
    int iItem = -1;
    while ((iItem = this->lvFiles.GetNextItem(iItem, LVNI_SELECTED)) != -1) {
        std::tvstring path;
        this->GetSubItemText(iItem, COLUMN_INDEX_PATH, true, path);
        paths.push_back(path);
    }

    // Create data object with file paths
    CComPtr<IDataObject> pDataObject;
    CreateDataObjectFromPaths(paths, &pDataObject);

    // Create drop source
    CDropSource* pDropSource = new CDropSource();

    // Initiate drag operation
    DWORD dwEffect;
    DoDragDrop(pDataObject, pDropSource,
               DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK,
               &dwEffect);

    pDropSource->Release();
}
```

---

## Sorting

### Column Click Handler

```cpp
LRESULT OnFilesColumnClick(LPNMHDR pnmh) {
    LPNMLISTVIEW const pLV = (LPNMLISTVIEW)pnmh;
    int const column = pLV->iSubItem;

    // Determine sort variation (ascending/descending/natural)
    unsigned char variation = 0;
    if (this->last_sort.column == column) {
        // Same column - cycle through variations
        variation = (this->last_sort.variation + 1) % 3;
    }

    // Update sort state
    this->last_sort.column = column;
    this->last_sort.variation = variation;
    ++this->last_sort.counter;

    // Perform sort
    this->SortResults(column, variation);

    // Update header to show sort indicator
    this->UpdateSortHeader(column, variation);

    // Refresh list
    this->lvFiles.Invalidate();

    return 0;
}
```

### Sort Implementation

```cpp
void SortResults(int column, unsigned char variation) {
    // Create comparator based on column
    auto comparator = [this, column, variation](
        const Result& a, const Result& b) -> bool {

        std::tvstring textA, textB;
        this->GetSubItemText(&a, column, textA);
        this->GetSubItemText(&b, column, textB);

        int cmp = 0;

        switch (column) {
            case COLUMN_INDEX_NAME:
            case COLUMN_INDEX_PATH:
            case COLUMN_INDEX_TYPE:
                // String comparison (case-insensitive)
                cmp = _tcsicmp(textA.c_str(), textB.c_str());
                break;

            case COLUMN_INDEX_SIZE:
            case COLUMN_INDEX_SIZE_ON_DISK:
                // Numeric comparison
                {
                    auto sizeA = lock(this->results.item_index(&a - &this->results[0]))
                        ->get_stdinfo(a.key().frs()).size();
                    auto sizeB = lock(this->results.item_index(&b - &this->results[0]))
                        ->get_stdinfo(b.key().frs()).size();
                    cmp = (sizeA < sizeB) ? -1 : (sizeA > sizeB) ? 1 : 0;
                }
                break;

            case COLUMN_INDEX_CREATION_TIME:
            case COLUMN_INDEX_MODIFICATION_TIME:
            case COLUMN_INDEX_ACCESS_TIME:
                // Timestamp comparison
                {
                    auto timeA = GetFileTime(a, column);
                    auto timeB = GetFileTime(b, column);
                    cmp = (timeA < timeB) ? -1 : (timeA > timeB) ? 1 : 0;
                }
                break;
        }

        // Apply sort direction
        if (variation == 1) cmp = -cmp;  // Descending

        return cmp < 0;
    };

    // Sort results
    std::stable_sort(this->results.begin(), this->results.end(), comparator);
}
```


---

## Dialog Resizing

### WTL::CDialogResize

UFFS uses WTL's dialog resize support for automatic control layout:

```cpp
class CMainDlg : public WTL::CDialogResize<CMainDlg> {
    BEGIN_DLGRESIZE_MAP(CMainDlg)
        // Search pattern edit - stretch horizontally
        DLGRESIZE_CONTROL(IDC_EDITFILENAME, DLSZ_SIZE_X)

        // Drive combo - move with right edge
        DLGRESIZE_CONTROL(IDC_COMBODRIVE, DLSZ_MOVE_X)

        // Results list - stretch both directions
        DLGRESIZE_CONTROL(IDC_LISTFILES, DLSZ_SIZE_X | DLSZ_SIZE_Y)

        // Status bar - stretch horizontally, move to bottom
        DLGRESIZE_CONTROL(IDC_STATUS_BAR, DLSZ_SIZE_X | DLSZ_MOVE_Y)
    END_DLGRESIZE_MAP()

    BOOL OnInitDialog(HWND hWnd, LPARAM lParam) {
        // Initialize resize support
        DlgResize_Init(true, true, WS_THICKFRAME | WS_CLIPCHILDREN);

        // ... rest of initialization
        return TRUE;
    }
};
```

### Resize Flags

| Flag | Behavior |
|------|----------|
| `DLSZ_SIZE_X` | Control width changes with dialog width |
| `DLSZ_SIZE_Y` | Control height changes with dialog height |
| `DLSZ_MOVE_X` | Control moves horizontally with dialog width |
| `DLSZ_MOVE_Y` | Control moves vertically with dialog height |
| `DLSZ_REPAINT` | Repaint control after resize |

---

## Status Bar

### Setup

```cpp
BOOL OnInitDialog(HWND hWnd, LPARAM lParam) {
    // Create status bar
    this->statusbar.Create(m_hWnd, NULL, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, IDC_STATUS_BAR);

    // Set up parts
    int parts[] = { 200, 400, -1 };  // Widths (-1 = fill remaining)
    this->statusbar.SetParts(3, parts);

    // Set initial text
    this->statusbar.SetText(0, _T("Ready"));
    this->statusbar.SetText(1, _T("0 files"));
    this->statusbar.SetText(2, _T(""));

    return TRUE;
}
```

### Updating Status

```cpp
void UpdateStatus() {
    // Format result count
    std::tvstring countText;
    countText = nformat_ui(this->results.size()) + _T(" files");
    this->statusbar.SetText(1, countText.c_str());

    // Show search time
    if (this->searchTime > 0) {
        std::tvstring timeText;
        timeText = nformat_ui(this->searchTime) + _T(" ms");
        this->statusbar.SetText(2, timeText.c_str());
    }
}
```

---

## Keyboard Accelerators

### Message Filter

```cpp
class CMainDlg : private WTL::CMessageFilter {
    HACCEL hAccel;

    BOOL OnInitDialog(HWND hWnd, LPARAM lParam) {
        // Load accelerator table
        this->hAccel = LoadAccelerators(_Module.GetResourceInstance(),
                                        MAKEINTRESOURCE(IDR_ACCELERATOR));

        // Add to message loop
        CMessageLoop* pLoop = _Module.GetMessageLoop();
        pLoop->AddMessageFilter(this);

        return TRUE;
    }

    // WTL::CMessageFilter
    BOOL PreTranslateMessage(MSG* pMsg) override {
        // Process accelerators
        if (this->hAccel &&
            TranslateAccelerator(m_hWnd, this->hAccel, pMsg)) {
            return TRUE;
        }

        // Let dialog handle Tab, Enter, Escape
        return IsDialogMessage(pMsg);
    }
};
```

### Common Accelerators

| Key | Action |
|-----|--------|
| Ctrl+F | Focus search box |
| Ctrl+C | Copy selected paths |
| Ctrl+A | Select all |
| Enter | Open selected file |
| Delete | Delete selected files |
| F5 | Refresh |
| Escape | Clear search / Close |

---

## RTL (Right-to-Left) Support

### CModifiedDialogImpl

UFFS supports RTL languages through a custom dialog implementation:

```cpp
template<typename T>
class CModifiedDialogImpl : public WTL::CDialogImpl<T> {
    bool const rtl;

public:
    CModifiedDialogImpl(bool modal, bool rtl) : rtl(rtl) {}

    HWND Create(HWND hWndParent) {
        if (rtl) {
            // Modify dialog template for RTL
            return CreateDialogIndirectParam(
                _Module.GetResourceInstance(),
                GetRTLDialogTemplate(),
                hWndParent,
                T::DialogProc,
                (LPARAM)this);
        }
        return WTL::CDialogImpl<T>::Create(hWndParent);
    }

private:
    DLGTEMPLATE* GetRTLDialogTemplate() {
        // Load and modify template to add WS_EX_LAYOUTRTL
        // ... template modification code
    }
};
```

### RTL Layout Considerations

1. **Window styles**: Add `WS_EX_LAYOUTRTL` to main window
2. **List view columns**: Reverse column order
3. **Text alignment**: Right-align text in controls
4. **Icons**: Mirror icons if needed
5. **Scroll bars**: Appear on left side

---

## Summary

The UFFS GUI achieves high performance through:

1. **Virtual list view**: Only visible items consume memory
2. **Owner-draw**: Custom colors without subclassing
3. **Background icon loading**: Non-blocking icon extraction
4. **Thread-safe invocation**: Safe UI updates from any thread
5. **Efficient caching**: Icons and file types cached by path
6. **Batch invalidation**: Coalesced row redraws

Key implementation patterns:

| Pattern | Purpose |
|---------|---------|
| `LVS_OWNERDATA` | Virtual list view mode |
| `LVN_GETDISPINFO` | On-demand data provision |
| `NM_CUSTOMDRAW` | Custom item rendering |
| `CInvokeImpl` | Thread-safe UI updates |
| `BackgroundWorker` | Async icon loading |
| `CDialogResize` | Automatic layout |

---

## See Also

- [01-overview.md](01-overview.md) - System architecture overview
- [02-ntfs-mft-reading.md](02-ntfs-mft-reading.md) - MFT reading implementation
- [03-indexing-engine.md](03-indexing-engine.md) - Index data structures
- [04-search-algorithm.md](04-search-algorithm.md) - Search implementation
- [05-threading-model.md](05-threading-model.md) - Threading architecture
