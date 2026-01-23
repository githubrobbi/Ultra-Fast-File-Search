# Boost Upgrade Plan: 1.73.0 → 1.90.0

## Overview

| Item | Current | Target |
|------|---------|--------|
| **Boost Version** | 1.73.0 (May 2020) | 1.90.0 (December 2025) |
| **Libraries Used** | xpressive, algorithm/string, algorithm/searching | Same (all header-only) |
| **Breaking Changes Risk** | Low | Header-only libs are stable |

## Boost Components Used in UFFS

| Component | Header | Purpose |
|-----------|--------|---------|
| Xpressive | `<boost/xpressive/xpressive_dynamic.hpp>` | Dynamic regex engine |
| Algorithm | `<boost/algorithm/string.hpp>` | String algorithms |
| Algorithm | `<boost/algorithm/searching/boyer_moore_horspool.hpp>` | Fast string search |

## Current Boost Defines (targetver.h)

```cpp
#define BOOST_ALL_NO_LIB 1              // No auto-linking (header-only)
#define BOOST_ALLOW_DEPRECATED_HEADERS 1 // Allow deprecated headers
#define BOOST_DISABLE_ASSERTS 1          // Disable runtime asserts
#define BOOST_EXCEPTION_DISABLE 1        // Disable exception support
#define BOOST_NO_CXX11_CHAR16_T          // Legacy workaround
#define BOOST_NO_CXX11_CHAR32_T          // Legacy workaround
#define BOOST_NO_SWPRINTF                // Legacy workaround
#define BOOST_REGEX_NO_FILEITER          // Disable file iterators
```

---

## Implementation Milestones

### Phase 1: Preparation
- [ ] **1.1** Download Boost 1.90.0 from https://www.boost.org/users/history/version_1_90_0.html
- [ ] **1.2** Extract to a temporary location (e.g., `boost_1_90_0_test/`)
- [ ] **1.3** Back up current `boost/` folder

### Phase 2: Initial Testing (Conservative)
- [ ] **2.1** Update `UFFS_BOOST` environment variable to point to new Boost
- [ ] **2.2** Attempt compilation with all existing defines intact
- [ ] **2.3** Document any compilation errors

### Phase 3: Address Deprecated Headers
- [ ] **3.1** Remove `BOOST_ALLOW_DEPRECATED_HEADERS` define temporarily
- [ ] **3.2** Compile and identify deprecated header warnings
- [ ] **3.3** Update any deprecated includes if needed
- [ ] **3.4** Re-enable `BOOST_ALLOW_DEPRECATED_HEADERS` if issues remain

### Phase 4: Clean Up Legacy Defines
- [ ] **4.1** Remove `BOOST_NO_CXX11_CHAR16_T` (VS 2017+ has full C++11)
- [ ] **4.2** Remove `BOOST_NO_CXX11_CHAR32_T` (VS 2017+ has full C++11)
- [ ] **4.3** Evaluate if `BOOST_NO_SWPRINTF` is still needed
- [ ] **4.4** Test compilation after each removal

### Phase 5: Final Integration
- [ ] **5.1** Replace `boost/` folder with new Boost 1.90.0
- [ ] **5.2** Full rebuild (Debug + Release, x64)
- [ ] **5.3** Run application and test regex functionality
- [ ] **5.4** Update documentation (README.md, architecture docs)

### Phase 6: Verification
- [ ] **6.1** Test basic file search
- [ ] **6.2** Test regex search patterns
- [ ] **6.3** Test wildcard patterns
- [ ] **6.4** Verify no performance regression

---

## Download Information

**Boost 1.90.0 (December 10, 2025)**

| Platform | File | SHA256 |
|----------|------|--------|
| Windows | `boost_1_90_0.7z` | `78413237decc94989bffd4c5e213cc4bf49ad32db3ed1efd1f2283bd6fb695b2` |
| Windows | `boost_1_90_0.zip` | `bdc79f179d1a4a60c10fe764172946d0eeafad65e576a8703c4d89d49949973c` |

Download: https://www.boost.org/users/history/version_1_90_0.html

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Xpressive API changes | Very Low | High | Xpressive is mature, stable |
| Deprecated headers | Medium | Low | Use `BOOST_ALLOW_DEPRECATED_HEADERS` |
| Compile errors | Low | Medium | Incremental testing |
| Runtime issues | Very Low | High | Thorough testing |

---

## Rollback Plan

If issues arise:
1. Restore backed-up `boost/` folder
2. Revert any changes to `targetver.h`
3. Rebuild with original Boost 1.73.0

---

## Notes

- All Boost libraries used are **header-only** - no need to build Boost
- Boost 1.90.0 supports Visual Studio 2017, 2019, 2022, and 2026
- The `boost/xpressive` library has been stable since ~2008
- The `boost/algorithm` library is also very stable

---

## Post-Upgrade Checklist

- [ ] Update `docs/architecture/09-build-system.md` version reference
- [ ] Update `docs/architecture/01-architecture-overview.md` version reference
- [ ] Update `README.md` Boost version reference
- [ ] Commit changes with message: "Upgrade Boost from 1.73.0 to 1.90.0"
- [ ] Tag release if appropriate

---

## Version History Between 1.73.0 and 1.90.0

Key releases with potential impact:

| Version | Date | Notable Changes |
|---------|------|-----------------|
| 1.74.0 | Aug 2020 | Minor updates |
| 1.75.0 | Dec 2020 | JSON library added |
| 1.80.0 | Aug 2022 | URL library added |
| 1.85.0 | Apr 2024 | Various fixes |
| 1.87.0 | Dec 2024 | Remove deprecated items |
| 1.90.0 | Dec 2025 | Latest stable |

**Note**: The libraries used by UFFS (xpressive, algorithm) have had minimal changes across all these versions.

