# Source Refactoring Milestones

> **Tracking document for the UltraFastFileSearch source code refactoring project**
> **Related**: [Source Refactoring Plan](source-refactoring-plan.md)

---

## Overview

| Metric | Value |
|--------|-------|
| **Total Phases** | 7 |
| **Estimated Hours** | 25-40 |
| **Start Date** | _TBD_ |
| **Target Completion** | _TBD_ |
| **Current Phase** | Not Started |
| **Overall Progress** | 0% |

---

## Phase Progress Tracker

### Phase 1: Cleanup
| Status | 🔴 Not Started |
|--------|----------------|
| **Branch** | `refactoring/phase-1-cleanup` |
| **Estimated** | 2 hours |
| **Actual** | — |
| **Assignee** | — |

| Step | Task | Status | Notes |
|------|------|--------|-------|
| 1.1 | Remove duplicate includes | ⬜ | Lines 61-70 |
| 1.2 | Fix syntax error on line 73 | ⬜ | Missing newline |
| 1.3 | Organize includes into groups | ⬜ | Add section comments |
| 1.4 | Delete file.cpp | ⬜ | Unused duplicate |
| 1.5 | Add #pragma once to all headers | ⬜ | 10 files to check |
| 1.6 | Commit and push | ⬜ | — |

**Verification Checklist:**
- [ ] Build succeeds (Release)
- [ ] Build succeeds (Debug)
- [ ] `uffs.com --help` works
- [ ] `uffs.com --benchmark-index=C` matches baseline

---

### Phase 2: Extract NTFS Types
| Status | 🔴 Not Started |
|--------|----------------|
| **Branch** | `refactoring/phase-2-ntfs-types` |
| **Estimated** | 3 hours |
| **Actual** | — |
| **Assignee** | — |

| Step | Task | Status | Notes |
|------|------|--------|-------|
| 2.1 | Create src/core directory | ⬜ | — |
| 2.2 | Create ntfs_types.hpp | ⬜ | ~150 lines |
| 2.3 | Add to project file | ⬜ | .vcxproj |
| 2.4 | Include in main file | ⬜ | Test build |
| 2.5 | Verify build | ⬜ | — |
| 2.6 | Commit and push | ⬜ | — |

**Verification Checklist:**
- [ ] Build succeeds (Release)
- [ ] Build succeeds (Debug)
- [ ] Benchmark matches baseline

---

### Phase 3: Extract Utilities
| Status | 🔴 Not Started |
|--------|----------------|
| **Branch** | `refactoring/phase-3-utilities` |
| **Estimated** | 3 hours |
| **Actual** | — |
| **Assignee** | — |

| Step | Task | Status | Notes |
|------|------|--------|-------|
| 3.1 | Create src/util directory | ⬜ | — |
| 3.2 | Create handle.hpp | ⬜ | Lines 2432-2491 |
| 3.3 | Create intrusive_ptr.hpp | ⬜ | Lines 2872-3050 |
| 3.4 | Create ref_counted.hpp | ⬜ | Lines 3052-3082 |
| 3.5 | Update main file includes | ⬜ | — |
| 3.6 | Verify build | ⬜ | — |
| 3.7 | Commit and push | ⬜ | — |

**Verification Checklist:**
- [ ] Build succeeds (Release)
- [ ] Build succeeds (Debug)
- [ ] Benchmark matches baseline

---

### Phase 4: Extract I/O Layer
| Status | 🔴 Not Started |
|--------|----------------|
| **Branch** | `refactoring/phase-4-io-layer` |
| **Estimated** | 6 hours |
| **Actual** | — |
| **Assignee** | — |

| Step | Task | Status | Notes |
|------|------|--------|-------|
| 4.1 | Create src/io directory | ⬜ | — |
| 4.2 | Create iocp.hpp | ⬜ | Lines 6740-7050 |
| 4.3 | Create overlapped.hpp | ⬜ | Lines 3084-3175 |
| 4.4 | Create io_priority.hpp | ⬜ | Lines 2493-2870 |
| 4.5 | Create mft_reader.hpp | ⬜ | Lines 7078-7560 |
| 4.6 | Update main file includes | ⬜ | — |
| 4.7 | Verify build | ⬜ | — |
| 4.8 | Commit and push | ⬜ | — |

**Verification Checklist:**
- [ ] Build succeeds (Release)
- [ ] Build succeeds (Debug)
- [ ] Benchmark matches baseline
- [ ] Async I/O still works correctly

---

### Phase 5: Extract NtfsIndex
| Status | 🔴 Not Started |
|--------|----------------|
| **Branch** | `refactoring/phase-5-ntfs-index` |
| **Estimated** | 6 hours |
| **Actual** | — |
| **Assignee** | — |

| Step | Task | Status | Notes |
|------|------|--------|-------|
| 5.1 | Create src/index directory | ⬜ | — |
| 5.2 | Identify NtfsIndex dependencies | ⬜ | Document deps |
| 5.3 | Create ntfs_index.hpp | ⬜ | Lines 3587-5565 |
| 5.4 | Gradual migration | ⬜ | Test after each move |
| 5.5 | Verify build | ⬜ | — |
| 5.6 | Commit and push | ⬜ | — |

**Verification Checklist:**
- [ ] Build succeeds (Release)
- [ ] Build succeeds (Debug)
- [ ] Benchmark matches baseline
- [ ] Index building works correctly

---


### Phase 6: Separate GUI from CLI
| Status | 🔴 Not Started |
|--------|----------------|
| **Branch** | `refactoring/phase-6-gui-cli-split` |
| **Estimated** | 8 hours |
| **Actual** | — |
| **Assignee** | — |

| Step | Task | Status | Notes |
|------|------|--------|-------|
| 6.1 | Create src/cli directory | ⬜ | — |
| 6.2 | Create src/gui directory | ⬜ | — |
| 6.3 | Create cli_main.cpp | ⬜ | Lines 12892-14068 |
| 6.4 | Create gui_main.cpp | ⬜ | Lines 14073-14156 |
| 6.5 | Extract CMainDlg | ⬜ | Lines 7842-11960 |
| 6.6 | Update project file | ⬜ | Configure entry points |
| 6.7 | Verify build | ⬜ | — |
| 6.8 | Commit and push | ⬜ | — |

**Verification Checklist:**
- [ ] Build succeeds (Release)
- [ ] Build succeeds (Debug)
- [ ] CLI tool works (`uffs.com`)
- [ ] GUI tool works (`uffs.exe`)
- [ ] Both produce same search results

---

### Phase 7: Modernize C++ Style
| Status | 🔴 Not Started |
|--------|----------------|
| **Branch** | `refactoring/phase-7-modernize` |
| **Estimated** | 6 hours |
| **Actual** | — |
| **Assignee** | — |

| Step | Task | Status | Notes |
|------|------|--------|-------|
| 7.1 | Replace NULL with nullptr | ⬜ | Use regex find/replace |
| 7.2 | Use auto for complex types | ⬜ | Iterators, etc. |
| 7.3 | Use range-based for loops | ⬜ | Where index not needed |
| 7.4 | Use enum class | ⬜ | Replace plain enums |
| 7.5 | Add [[nodiscard]] | ⬜ | Important return values |
| 7.6 | Verify build | ⬜ | — |
| 7.7 | Commit and push | ⬜ | — |

**Verification Checklist:**
- [ ] Build succeeds (Release)
- [ ] Build succeeds (Debug)
- [ ] No new warnings
- [ ] Benchmark matches baseline

---

## Summary Dashboard

| Phase | Name | Est. Hours | Status | Progress |
|-------|------|------------|--------|----------|
| 1 | Cleanup | 2 | 🔴 Not Started | 0/6 |
| 2 | Extract NTFS Types | 3 | 🔴 Not Started | 0/6 |
| 3 | Extract Utilities | 3 | 🔴 Not Started | 0/7 |
| 4 | Extract I/O Layer | 6 | 🔴 Not Started | 0/8 |
| 5 | Extract NtfsIndex | 6 | 🔴 Not Started | 0/6 |
| 6 | Separate GUI/CLI | 8 | 🔴 Not Started | 0/8 |
| 7 | Modernize C++ | 6 | 🔴 Not Started | 0/7 |
| **Total** | | **34** | | **0/48** |

### Status Legend

| Symbol | Meaning |
|--------|---------|
| 🔴 | Not Started |
| 🟡 | In Progress |
| 🟢 | Complete |
| 🔵 | Blocked |
| ⬜ | Task not started |
| 🔲 | Task in progress |
| ✅ | Task complete |
| ❌ | Task blocked/failed |

---

## Baseline Measurements

Record these BEFORE starting any refactoring:

### Build Times

| Configuration | Time | Date Recorded |
|---------------|------|---------------|
| Release | — | — |
| Debug | — | — |

### Benchmark Results

| Test | Result | Date Recorded |
|------|--------|---------------|
| `--benchmark-index=C` | — | — |
| `--benchmark-index=D` | — | — |

### Search Results

| Query | Result Count | Date Recorded |
|-------|--------------|---------------|
| `*.txt` on C: | — | — |
| `*.exe` on C: | — | — |

---

## Change Log

| Date | Phase | Change | Author |
|------|-------|--------|--------|
| — | — | — | — |

---

## Blockers & Issues

| ID | Phase | Issue | Status | Resolution |
|----|-------|-------|--------|------------|
| — | — | — | — | — |

---

## Notes

### How to Update This Document

1. **Starting a phase**: Change status from 🔴 to 🟡
2. **Completing a step**: Change ⬜ to ✅
3. **Completing a phase**: Change status from 🟡 to 🟢
4. **Recording time**: Fill in "Actual" hours
5. **Blocking issue**: Add to Blockers table, change status to 🔵

### Git Workflow

```bash
# Starting a new phase
git checkout main
git pull
git checkout -b refactoring/phase-X-name

# After each step
git add -A
git commit -m "Phase X Step Y: description"

# Completing a phase
git push origin refactoring/phase-X-name
# Create PR, get review, merge to main
```

### Rollback Procedure

If a phase introduces bugs:

```bash
# Revert to main
git checkout main
git branch -D refactoring/phase-X-name

# Start fresh
git checkout -b refactoring/phase-X-name-v2
```

---

## Completion Criteria

A phase is considered **complete** when:

1. ✅ All steps are marked complete
2. ✅ All verification checklist items pass
3. ✅ Code is committed and pushed
4. ✅ PR is merged to main (if using PRs)
5. ✅ Actual hours are recorded
6. ✅ Change log is updated

The **entire refactoring** is complete when:

1. ✅ All 7 phases are complete
2. ✅ Final benchmark matches baseline (within 5%)
3. ✅ All tests pass
4. ✅ Code review approved
5. ✅ Documentation updated

