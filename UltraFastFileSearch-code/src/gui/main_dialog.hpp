#pragma once

// ============================================================================
// CMainDlg Class Documentation
// ============================================================================
// This header documents the main dialog class for UltraFastFileSearch GUI.
// The actual implementation remains in UltraFastFileSearch.cpp for now.
//
// Location in UltraFastFileSearch.cpp: Lines 7869-11974 (~4105 lines)
// ============================================================================

// ============================================================================
// OVERVIEW
// ============================================================================
// CMainDlg is the main application dialog for the UFFS GUI.
// It provides the search interface, results display, and user interaction.
//
// Inheritance:
//   - CModifiedDialogImpl<CMainDlg>  : Custom dialog implementation
//   - WTL::CDialogResize<CMainDlg>   : Resizable dialog support
//   - CInvokeImpl<CMainDlg>          : Cross-thread invocation
//   - WTL::CMessageFilter            : Message filtering
//
// ============================================================================
// KEY COMPONENTS
// ============================================================================
//
// 1. Column Indices (lines 7878-7905)
//    - COLUMN_INDEX_NAME, COLUMN_INDEX_PATH, COLUMN_INDEX_TYPE
//    - COLUMN_INDEX_SIZE, COLUMN_INDEX_SIZE_ON_DISK
//    - COLUMN_INDEX_CREATION_TIME, COLUMN_INDEX_MODIFICATION_TIME
//    - COLUMN_INDEX_ACCESS_TIME, COLUMN_INDEX_DESCENDENTS
//    - Various attribute columns (readonly, archive, system, hidden, etc.)
//
// 2. Nested Classes:
//    - CThemedListViewCtrl (line 7918): Themed ListView control
//    - CSearchPattern (lines 7923-8031): Custom edit control for search input
//    - CacheInfo (lines 8033-8046): Icon/type caching structure
//
// 3. Member Variables (approximate locations):
//    - m_hEvent: Single instance event handle
//    - m_indices: Vector of NtfsIndex pointers
//    - m_results: Search results
//    - m_iocp: IoCompletionPort for async I/O
//    - m_listView: Results ListView control
//    - m_searchPattern: Search input control
//    - m_statusBar: Status bar control
//
// ============================================================================
// MESSAGE HANDLERS
// ============================================================================
// The dialog handles numerous Windows messages:
//
// - WM_INITDIALOG: Initialize dialog, create controls
// - WM_DESTROY: Cleanup resources
// - WM_SIZE: Handle resizing
// - WM_TIMER: Background processing
// - WM_DEVICECHANGE: Drive add/remove detection
// - WM_CONTEXTMENU: Right-click context menu
// - WM_NOTIFY: ListView notifications
//
// ============================================================================
// KEY METHODS
// ============================================================================
//
// Initialization:
//   - OnInitDialog(): Set up controls, start indexing
//   - InitializeListView(): Configure ListView columns
//   - StartIndexing(): Begin background index building
//
// Search:
//   - OnSearchPatternChange(): Handle search input changes
//   - PerformSearch(): Execute search against indices
//   - UpdateResults(): Refresh ListView with results
//
// Results Display:
//   - OnGetDispInfo(): Provide data for virtual ListView
//   - FormatFileSize(): Format size for display
//   - FormatDateTime(): Format timestamps
//
// Context Menu:
//   - OnContextMenu(): Show right-click menu
//   - OnOpenFile(): Open selected file
//   - OnOpenFolder(): Open containing folder
//   - OnCopyPath(): Copy path to clipboard
//
// ============================================================================
// DEPENDENCIES
// ============================================================================
// CMainDlg depends on:
//
// 1. NtfsIndex class (lines 3587-5565)
//    - File system indexing
//    - Search functionality
//
// 2. IoCompletionPort class (lines 6740-7050)
//    - Async MFT reading
//
// 3. Results class (lines 7603-7726)
//    - Search result storage
//
// 4. WTL/ATL Framework
//    - Dialog, ListView, StatusBar controls
//    - Message handling infrastructure
//
// 5. Resources (resource.h)
//    - Dialog template (IDD_MAINDLG)
//    - String resources
//    - Icons and bitmaps
//
// ============================================================================
// FUTURE EXTRACTION PLAN
// ============================================================================
// To fully extract CMainDlg:
//
// 1. Create main_dialog.cpp with class implementation
// 2. Move nested classes to separate files if needed
// 3. Extract helper functions to utilities
// 4. Update includes to use extracted headers
// 5. Update project file to compile main_dialog.cpp
// 6. Verify build and test GUI functionality
//
// Estimated effort: 6-8 hours (largest class in codebase)
// Risk: Medium-High (complex WTL interactions)
//
// Recommended approach:
//   - Extract in stages, testing after each
//   - Start with simple methods, move to complex ones
//   - Keep message map in sync with implementation
// ============================================================================

namespace uffs {
namespace gui {

// Forward declaration for future extraction
// class MainDialog;

} // namespace gui
} // namespace uffs

