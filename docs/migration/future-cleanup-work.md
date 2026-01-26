# Future Cleanup Work

> **Status**: In Progress
> **Created**: 2026-01-25
> **Last Updated**: 2026-01-26
> **Related**: [Refactoring Milestones](refactoring-milestones.md) (Phases 1-7 Complete)

---

## Executive Summary

The initial 7-phase refactoring is complete, reducing the monolith from 14,155 to 4,306 lines. However, significant architectural and organizational issues remain. This document outlines the **next wave of improvements** to bring the codebase to modern C++ standards.

---

## Progress Tracker

| Wave | Status | Notes |
|------|--------|-------|
| Wave 1: Quick Wins | ✅ COMPLETE | SwiftSearch removed, icon renamed |
| Wave 2: Architecture | 🔄 IN PROGRESS | Phase 8.5/9.x extractions ongoing |
| Wave 3: Infrastructure | ⏳ Not Started | |
| Wave 4: Polish | ⏳ Not Started | |

---

## Current State (Post-Wave 2 Progress)

| Metric | Value | Target | Change |
|--------|-------|--------|--------|
| Monolith (`UltraFastFileSearch.cpp`) | **995 lines** | < 500 lines | **-3,311 lines (77% reduction)** |
| `main_dialog.hpp` | 3,910 lines | Split into multiple files | - |
| `ntfs_index.hpp` | 1,936 lines | Split into .hpp/.cpp | - |
| `.cpp` compilation units | 7 files | 15+ files | +3 (string_utils.cpp, mft_diagnostics.cpp, listview_adapter.cpp) |
| Extracted utility headers | 20 files | - | +10 NEW |
| Unit tests | 0 | Full coverage | - |
| Third-party deps in source | 3 (CLI11, boost, wtl) | 0 (use package manager) | - |

### Recent Extractions (2026-01-26)
- ✅ Removed duplicate code (ILIsEmpty, WTL namespace) - saved 17 lines
- ✅ `src/gui/listview_adapter.hpp/.cpp` - ImageListAdapter, ListViewAdapter, autosize_columns (~520 lines)
- ✅ `src/util/x64_launcher.hpp` - get_app_guid, extract_and_run_if_needed (~145 lines)
- ✅ `src/gui/string_loader.hpp` - RefCountedCString, StringLoader (~110 lines)
- ✅ `src/gui/listview_hooks.hpp` - CDisableListViewUnnecessaryMessages, CSetRedraw (~105 lines)
- ✅ `src/util/nt_user_call_hook.hpp` - Added HookedNtUserProps (~43 lines)

---

## 🔴 Critical: Header-Only Anti-Pattern

### Problem
Almost all code is in `.hpp` files with full implementations:

| File | Lines | Issue |
|------|-------|-------|
| `src/gui/main_dialog.hpp` | 3,910 | Entire GUI in one header |
| `src/index/ntfs_index.hpp` | 1,936 | Entire index engine in header |
| `src/cli/cli_main.hpp` | 1,155 | CLI entry point in header |
| `src/io/mft_reader.hpp` | 511 | MFT reader in header |

### Impact
- **Slow compilation**: Every change recompiles everything
- **No separate compilation**: Can't build modules independently
- **Untestable**: Can't unit test individual components
- **Binary bloat**: Template instantiations duplicated

### Solution: Phase 8 - Split Headers into .hpp/.cpp

| Task | Priority | Effort | Status |
|------|----------|--------|--------|
| 8.1 Split `ntfs_index.hpp` → `.hpp` + `.cpp` | High | 4h | ⏳ BLOCKING 8.5/8.6 |
| 8.2 Split `mft_reader.hpp` → `.hpp` + `.cpp` | High | 2h | ⏳ |
| 8.3 Split `io_completion_port.hpp` → `.hpp` + `.cpp` | Medium | 2h | ⏳ |
| 8.4 Split `main_dialog.hpp` → multiple files | High | 6h | ⏳ |
| 8.5 Convert `cli_main.hpp` → `cli_main.cpp` | Medium | 1h | ❌ BLOCKED (needs 8.1) |
| 8.6 Convert `gui_main.hpp` → `gui_main.cpp` | Medium | 1h | ❌ BLOCKED (needs 8.1) |

**Estimated Total**: 16 hours

> **Note (2026-01-26)**: Task 8.5 was attempted but failed. The issue is that `cli_main.hpp`
> includes `ntfs_index.hpp`, which depends on types from the monolith's `namespace ntfs`
> (e.g., `ATTRIBUTE_RECORD_HEADER`, `FILE_RECORD_SEGMENT_HEADER`). These types are NOT in
> the extracted `uffs::ntfs` namespace in `ntfs_types.hpp`. To unblock 8.5/8.6, we must first
> consolidate the NTFS types by moving the monolith's `namespace ntfs` content to `ntfs_types.hpp`.

---

## 🔴 Critical: Monolith Still Too Large

### Problem
`UltraFastFileSearch.cpp` is still 4,306 lines containing:
- Progress dialog implementation
- Volume path utilities
- MFT dump/benchmark tools
- String conversion utilities
- Error handling utilities
- Miscellaneous helper functions

### Solution: Phase 9 - Complete Monolith Decomposition

| Task | Target File | Lines | Status |
|------|-------------|-------|--------|
| 9.1 Extract `CProgressDialog` | `src/gui/progress_dialog.hpp` | ~344 | ✅ DONE |
| 9.2 Extract `SearchResult`/`Results` | `src/search/search_results.hpp` | ~145 | ✅ DONE |
| 9.3 Extract MFT diagnostics | `src/cli/mft_diagnostics.cpp` | ~715 | ✅ DONE |
| 9.4 Extract MFT benchmark tool | (merged with 9.3) | - | ✅ DONE |
| 9.5 Extract string utilities | `src/util/string_utils.hpp/.cpp` | ~100 | ✅ DONE |
| 9.6 Extract error handling | `src/util/error_utils.hpp` | ~50 | ✅ DONE (previously) |
| 9.7 Extract time utilities | `src/util/time_utils.hpp` | ~130 | ✅ DONE |
| 9.8 Extract NFormat | `src/util/nformat_ext.hpp` | ~90 | ✅ DONE |
| 9.9 Extract UTF converter | `src/util/utf_convert.hpp` | ~55 | ✅ DONE |
| 9.10 Extract MatchOperation | `src/search/match_operation.hpp` | ~130 | ✅ DONE |
| 9.11 Extract volume utilities | `src/util/volume_utils.hpp` | ~80 | ✅ DONE |
| 9.12 Reduce monolith to entry point only | `UltraFastFileSearch.cpp` | < 500 | 🔄 IN PROGRESS (995 lines) |
| 9.13 Extract ListViewAdapter + autosize_columns | `src/gui/listview_adapter.hpp/.cpp` | ~520 | ✅ DONE |
| 9.14 Extract x64 launcher | `src/util/x64_launcher.hpp` | ~145 | ✅ DONE |
| 9.15 Extract RefCountedCString + StringLoader | `src/gui/string_loader.hpp` | ~110 | ✅ DONE |
| 9.16 Extract listview hooks | `src/gui/listview_hooks.hpp` | ~105 | ✅ DONE |
| 9.17 Move HookedNtUserProps | `src/util/nt_user_call_hook.hpp` | ~43 | ✅ DONE |

**Estimated Total**: 8 hours (~1h remaining)

### Remaining Monolith Content (~995 lines)

| Lines | Content | Extraction Target | Priority |
|-------|---------|-------------------|----------|
| 1-150 | Includes, config, macros | Keep (entry point setup) | - |
| 150-500 | Utility functions (ILIsEmpty, DisplayError, isdevnull, NormalizePath, etc.) | `src/util/misc_utils.hpp` | Medium |
| 500-740 | Locale/cluster utilities, hook includes | Keep (orchestration) | - |
| 740-790 | Extracted header includes | Keep (orchestration) | - |
| 790-870 | My_Stable_sort, main_dialog include | Keep (GUI entry) | - |
| 870-940 | PACKAGE_VERSION, PrintVersion, s2ws | `src/util/version_info.hpp` | Low |
| 940-995 | benchmark_index_build, CLI/GUI includes | Keep (entry points) | - |

**Next extraction candidates (in order):**
1. **Utility functions** (~200 lines) - Miscellaneous helpers to `src/util/misc_utils.hpp`
2. **PACKAGE_VERSION/PrintVersion** (~70 lines) - Version info to `src/util/version_info.hpp`

---

## 🟠 Major: Third-Party Dependency Management

### Problem
Third-party libraries are embedded in the repository:

```
/boost/                    # 100+ MB, entire Boost library
/wtl/                      # Windows Template Library
UltraFastFileSearch-code/CLI11.hpp  # Single-header CLI parser
```

### Issues
- Repository bloat (boost alone is huge)
- Version management is manual
- No clear separation of project vs dependencies
- Difficult to update dependencies

### Solution: Phase 10 - Dependency Management

| Task | Priority | Approach |
|------|----------|----------|
| 10.1 Move to `external/` or `third_party/` directory | High | Reorganize |
| 10.2 Add vcpkg/conan manifest | Medium | Package manager |
| 10.3 Use git submodules for boost/wtl | Medium | Submodules |
| 10.4 Document dependency versions | High | README update |
| 10.5 Add `.gitignore` for build artifacts | High | Cleanup |

**Estimated Total**: 4 hours

---

## 🟠 Major: No Test Infrastructure

### Problem
- No unit tests
- No integration tests
- Only `test_cli_compatibility.bat` for manual testing
- No CI/CD pipeline

### Solution: Phase 11 - Test Infrastructure

| Task | Priority | Framework |
|------|----------|-----------|
| 11.1 Add Google Test or Catch2 | High | Testing framework |
| 11.2 Create `tests/` directory structure | High | Organization |
| 11.3 Write unit tests for `src/util/` | High | Start simple |
| 11.4 Write unit tests for `src/core/` | Medium | Core logic |
| 11.5 Write integration tests for CLI | Medium | End-to-end |
| 11.6 Add GitHub Actions CI | Medium | Automation |

**Estimated Total**: 12 hours

---

## 🟠 Major: Build Artifacts in Repository

### Problem
Binary files committed to repository:
- `uffs.exe` (root)
- `uffs.com` (root)
- `bin/x64/` directory

### Solution
```gitignore
# Add to .gitignore
*.exe
*.com
*.obj
*.pdb
bin/
x64/
Debug/
Release/
```

**Estimated Total**: 0.5 hours

---

## 🟡 Moderate: Mixed File Organization

### Problem
Files are scattered between legacy and new locations:

```
UltraFastFileSearch-code/
├── BackgroundWorker.hpp      ← Legacy (root level)
├── CDlgTemplate.hpp          ← Legacy (root level)
├── nformat.hpp               ← Legacy (root level)
├── path.hpp                  ← Legacy (root level)
├── string_matcher.cpp/hpp    ← Legacy (root level)
├── src/
│   ├── cli/                  ← New structure ✓
│   ├── core/                 ← New structure ✓
│   ├── gui/                  ← New structure ✓
│   ├── index/                ← New structure ✓
│   ├── io/                   ← New structure ✓
│   └── util/                 ← New structure ✓
```

### Solution: Phase 12 - Complete File Reorganization

| Task | From | To |
|------|------|-----|
| 12.1 Move `BackgroundWorker.hpp` | root | `src/util/` |
| 12.2 Move `CDlgTemplate.hpp` | root | `src/gui/` |
| 12.3 Move `nformat.hpp` | root | `src/util/` |
| 12.4 Move `path.hpp` | root | `src/util/` |
| 12.5 Move `string_matcher.*` | root | `src/search/` |
| 12.6 Move `ShellItemIDList.hpp` | root | `src/util/` |
| 12.7 Move `CommandLineParser.*` | root | `src/cli/` |
| 12.8 Update all include paths | - | - |

**Estimated Total**: 3 hours

---

## 🟡 Moderate: Naming Inconsistency

### Problem
Mixed naming conventions:
- `BackgroundWorker.hpp` (PascalCase)
- `string_matcher.hpp` (snake_case)
- `nformat.hpp` (lowercase)
- `Search Drive.ico` (spaces in filename)

### Solution: Phase 13 - Standardize Naming

**Recommended Convention**: `snake_case.hpp` for all source files

| Current | Proposed | Status |
|---------|----------|--------|
| `BackgroundWorker.hpp` | `background_worker.hpp` | ⏳ |
| `CDlgTemplate.hpp` | `dialog_template.hpp` | ⏳ |
| `CModifiedDialogImpl.hpp` | `modified_dialog_impl.hpp` | ⏳ |
| `NtUserCallHook.hpp` | `nt_user_call_hook.hpp` | ⏳ |
| `ShellItemIDList.hpp` | `shell_item_id_list.hpp` | ⏳ |
| `Search Drive.ico` | `search_drive.ico` | ✅ DONE |

**Estimated Total**: 2 hours (1.5h remaining)

---

## 🟡 Moderate: Warning Suppression Cleanup

### Problem
`stdafx.h` contains ~50 `#pragma warning(disable: ...)` directives.
`docs/problem` shows hundreds of unresolved warnings.

### Solution: Phase 14 - Address Compiler Warnings

| Task | Priority |
|------|----------|
| 14.1 Review each suppressed warning | Medium |
| 14.2 Fix warnings where possible | Medium |
| 14.3 Document intentional suppressions | Low |
| 14.4 Enable `/W4` or `/Wall` for new code | Medium |
| 14.5 Add hint file for IntelliSense macros | Low |

**Estimated Total**: 4 hours

---

## ~~🟡 Moderate: Remove Duplicate Codebase~~ ✅ COMPLETE

### ~~Problem~~
~~`swiftsearch-code-4043bc.../` contains the original SwiftSearch code.~~

### Resolution
- ✅ Deleted `swiftsearch-code-4043bc4cfb4b47216cd70c8229912728017f4b63/` directory
- ✅ Deleted `Original Packages/swiftsearch-code-4043bc-2023-03-24.zip`
- ✅ Updated README.md directory tree

**Completed**: 2026-01-25

---

## 📋 Complete Task Summary

| Phase | Name | Priority | Effort | Impact |
|-------|------|----------|--------|--------|
| 8 | Split Headers → .hpp/.cpp | 🔴 Critical | 16h | High |
| 9 | Complete Monolith Decomposition | 🔴 Critical | 8h | High |
| 10 | Dependency Management | 🟠 Major | 4h | Medium |
| 11 | Test Infrastructure | 🟠 Major | 12h | High |
| 12 | Complete File Reorganization | 🟡 Moderate | 3h | Medium |
| 13 | Standardize Naming | 🟡 Moderate | 2h | Low |
| 14 | Warning Cleanup | 🟡 Moderate | 4h | Medium |
| - | Build Artifacts Cleanup | 🟠 Major | 0.5h | Low |
| - | Remove Duplicate Codebase | 🟡 Moderate | 0.5h | Low |
| **Total** | | | **50h** | |

---

## Recommended Execution Order

### Wave 1: Quick Wins (1-2 hours) ✅ COMPLETE
1. ~~Add build artifacts to `.gitignore`~~ (User: keep tracked intentionally)
2. ✅ Remove/archive duplicate SwiftSearch codebase
3. ✅ Rename files with spaces (`Search Drive.ico` → `search_drive.ico`)

### Wave 2: Architecture (24 hours) 🔄 IN PROGRESS
1. Phase 8: Split headers into .hpp/.cpp
2. Phase 9: Complete monolith decomposition
   - ✅ Step 1: Extract string utilities (Task 9.5) - `src/util/string_utils.hpp/.cpp`
   - ✅ Step 2: Consolidate NTFS types (Task 8.1) - Moved ~375 lines to `ntfs_types.hpp`
   - ✅ Step 3: Decouple CLI from GUI - Extracted `SystemTimeToString` to `time_utils.hpp`
   - ✅ Step 4: Extract MFT diagnostics (Task 9.3) - `src/cli/mft_diagnostics.cpp` (~715 lines)
   - ✅ Step 5: Extract CProgressDialog (Task 9.1) - `src/gui/progress_dialog.hpp` (~344 lines)
   - ✅ Step 6: Extract SearchResult/Results (Task 9.2) - `src/search/search_results.hpp` (~145 lines)
   - 🔄 Step 7: Continue monolith reduction (2,077 → <500 lines)
   - ⏳ Step 8: Convert `cli_main.hpp` → `cli_main.cpp` (Task 8.5) - Deferred until headers self-contained
   - ⏳ Step 9: Convert `gui_main.hpp` → `gui_main.cpp` (Task 8.6) - Deferred

### Wave 3: Infrastructure (16 hours)
1. Phase 10: Dependency management
2. Phase 11: Test infrastructure

### Wave 4: Polish (9 hours)
1. Phase 12: File reorganization
2. Phase 13: Naming standardization
3. Phase 14: Warning cleanup

---

## Success Criteria

The codebase is considered "modern" when:

- [ ] No source file exceeds 500 lines
- [ ] All code is in proper `.cpp` compilation units (not header-only)
- [ ] Unit test coverage > 60%
- [ ] CI/CD pipeline runs on every PR
- [ ] Third-party dependencies managed via package manager
- [ ] Consistent naming convention throughout
- [ ] Zero warnings with `/W4`
- [ ] Build time < 30 seconds (incremental)

---

## Notes

### Why This Matters

1. **Maintainability**: Smaller files are easier to understand and modify
2. **Testability**: Separate compilation units can be unit tested
3. **Build Speed**: Incremental builds only recompile changed files
4. **Onboarding**: New developers can understand the codebase faster
5. **Collaboration**: Multiple developers can work on different modules

### Risk Mitigation

- Create a branch for each phase
- Run full test suite after each change
- Keep baseline benchmarks for performance regression testing
- Document all breaking changes

---

## Appendix: Ideal Final Structure

```
UltraFastFileSearch/
├── CMakeLists.txt              # Modern build system
├── vcpkg.json                  # Dependency manifest
├── .github/
│   └── workflows/
│       └── ci.yml              # CI/CD pipeline
├── src/
│   ├── core/
│   │   ├── ntfs_types.hpp/cpp
│   │   └── volume.hpp/cpp
│   ├── index/
│   │   ├── ntfs_index.hpp/cpp
│   │   └── index_builder.hpp/cpp
│   ├── io/
│   │   ├── io_completion_port.hpp/cpp
│   │   ├── mft_reader.hpp/cpp
│   │   └── overlapped.hpp/cpp
│   ├── search/
│   │   ├── pattern_matcher.hpp/cpp
│   │   └── search_engine.hpp/cpp
│   ├── util/
│   │   ├── handle.hpp
│   │   ├── intrusive_ptr.hpp
│   │   └── string_utils.hpp/cpp
│   ├── cli/
│   │   ├── cli_main.cpp
│   │   └── command_line_parser.hpp/cpp
│   └── gui/
│       ├── gui_main.cpp
│       ├── main_dialog.hpp/cpp
│       └── progress_dialog.hpp/cpp
├── tests/
│   ├── unit/
│   │   ├── test_ntfs_types.cpp
│   │   ├── test_pattern_matcher.cpp
│   │   └── test_handle.cpp
│   └── integration/
│       └── test_cli.cpp
├── resources/
│   ├── resource.h
│   └── UltraFastFileSearch.rc
├── docs/
│   └── ...
└── external/                   # Or managed via vcpkg
    ├── CLI11/
    ├── boost/
    └── wtl/
```
