#pragma once

// ============================================================================
// GUI Entry Point Documentation
// ============================================================================
// This header documents the GUI entry point for UltraFastFileSearch.
// The actual implementation remains in UltraFastFileSearch.cpp for now.
//
// Location in UltraFastFileSearch.cpp: Lines 14076-14183 (~107 lines)
// ============================================================================

// ============================================================================
// OVERVIEW
// ============================================================================
// The GUI entry point (_tWinMain()) provides the Windows GUI interface.
// It initializes WTL/ATL, handles localization, and creates the main dialog.
//
// Entry Point:
//   int __stdcall _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nShowCmd)
//
// Output:
//   - Windows GUI application (uffs.exe)
//   - Main dialog with search interface
//   - ListView for results display
//
// ============================================================================
// DEPENDENCIES
// ============================================================================
// The GUI main function depends on:
//
// 1. WTL/ATL Framework
//    - CAppModule (_Module global)
//    - CMessageLoop for message handling
//    - Dialog and control classes
//
// 2. CMainDlg class (lines 7869-11960)
//    - Main application dialog
//    - Search interface
//    - Results display
//
// 3. Helper functions:
//    - extract_and_run_if_needed() - MUI extraction
//    - get_app_guid() - Application GUID
//    - get_ui_locale_name() - Localization
//    - basename(), dirname() - Path utilities
//    - getdirsep() - Directory separator
//
// 4. Resources:
//    - IDS_APPNAME - Application name string
//    - Dialog templates
//    - Icons and bitmaps
//
// ============================================================================
// MAIN FUNCTION STRUCTURE
// ============================================================================
// 1. Initialization (lines 14076-14092)
//    - Get command-line arguments
//    - Call extract_and_run_if_needed()
//    - Check for early exit
//
// 2. RTL Layout Detection (lines 14097-14112)
//    - Check LOCALE_IREADINGLAYOUT
//    - Set process default layout if RTL
//
// 3. Module Initialization (lines 14114-14117)
//    - Initialize _Module with hInstance
//
// 4. Single Instance Check (lines 14119-14174)
//    - Create named event for single instance
//    - If first instance:
//      a. Create message loop
//      b. Load MUI resources
//      c. Create and show CMainDlg
//      d. Run message loop
//    - If second instance:
//      a. Signal first instance
//      b. Exit
//
// 5. Cleanup (lines 14177-14183)
//    - Terminate _Module
//    - Return result
//
// ============================================================================
// MUI (Multilingual User Interface) SUPPORT
// ============================================================================
// The GUI supports localized resources through MUI:
//
// Search paths for .mui files:
//   1. <module_dir>/<locale>/<module_name>.mui
//   2. <module_dir>/<module_name>.<locale>.mui
//   3. <module_dir>/<locale>/<module_name>.<locale>.mui
//   4. <module_dir>/<module_name>.mui
//
// ============================================================================
// SINGLE INSTANCE BEHAVIOR
// ============================================================================
// The application uses a named event to ensure single instance:
//   - Event name: "Local\<AppName>.<GUID>"
//   - First instance creates event and runs
//   - Second instance pulses event and exits
//   - First instance can respond to pulse (e.g., bring to front)
//
// ============================================================================
// FUTURE EXTRACTION PLAN
// ============================================================================
// To fully extract the GUI entry point:
//
// 1. Create gui_main.cpp with _tWinMain() function
// 2. Extract CMainDlg to src/gui/main_dialog.hpp/cpp
// 3. Move helper functions to appropriate modules
// 4. Update includes to use extracted headers
// 5. Update project file to compile gui_main.cpp
// 6. Verify build and test functionality
//
// Estimated effort: 2-3 hours (smaller than CLI)
// Risk: Low (well-contained WTL code)
// ============================================================================

namespace uffs {
namespace gui {

// Forward declaration for future extraction
// int run_gui(HINSTANCE hInstance, int nShowCmd);

} // namespace gui
} // namespace uffs

