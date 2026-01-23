# Search Algorithm

## Introduction

This document provides exhaustive detail on how Ultra Fast File Search executes searches and manages results. After reading this document, you should be able to:

1. Understand how search queries are parsed and normalized
2. Implement the MatchOperation pattern matching setup
3. Execute searches across multiple volumes
4. Collect and store search results efficiently
5. Sort results by various criteria

---

## Overview: Search Pipeline

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Search Pipeline                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. User Input                                                          │
│     ┌─────────────────────────────────────────────────────────────┐    │
│     │ txtPattern.GetWindowText() → "*.cpp"                        │    │
│     └─────────────────────────────────────────────────────────────┘    │
│                              │                                          │
│                              ▼                                          │
│  2. Pattern Analysis (MatchOperation::init)                             │
│     ┌─────────────────────────────────────────────────────────────┐    │
│     │ - Detect regex prefix (>)                                   │    │
│     │ - Detect path pattern (\ or **)                             │    │
│     │ - Detect stream pattern (:)                                 │    │
│     │ - Extract root path optimization                            │    │
│     │ - Create string_matcher                                     │    │
│     └─────────────────────────────────────────────────────────────┘    │
│                              │                                          │
│                              ▼                                          │
│  3. Volume Selection                                                    │
│     ┌─────────────────────────────────────────────────────────────┐    │
│     │ - Filter volumes by prematch()                              │    │
│     │ - Wait for index completion                                 │    │
│     │ - Build wait_handles list                                   │    │
│     └─────────────────────────────────────────────────────────────┘    │
│                              │                                          │
│                              ▼                                          │
│  4. Index Traversal (NtfsIndex::matches)                                │
│     ┌─────────────────────────────────────────────────────────────┐    │
│     │ - Depth-first traversal from root                           │    │
│     │ - Build path incrementally                                  │    │
│     │ - Apply string_matcher::is_match()                          │    │
│     │ - Collect matching results                                  │    │
│     └─────────────────────────────────────────────────────────────┘    │
│                              │                                          │
│                              ▼                                          │
│  5. Result Collection                                                   │
│     ┌─────────────────────────────────────────────────────────────┐    │
│     │ - Store SearchResult (key + depth)                          │    │
│     │ - Track index references                                    │    │
│     │ - Merge results from multiple volumes                       │    │
│     └─────────────────────────────────────────────────────────────┘    │
│                              │                                          │
│                              ▼                                          │
│  6. Display                                                             │
│     ┌─────────────────────────────────────────────────────────────┐    │
│     │ - Update ListView item count                                │    │
│     │ - Provide data via LVN_GETDISPINFO                          │    │
│     └─────────────────────────────────────────────────────────────┘    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## MatchOperation: Query Analysis

### Structure Definition

```cpp
struct MatchOperation {
    value_initialized<bool> is_regex;           // Pattern starts with '>'
    value_initialized<bool> is_path_pattern;    // Contains '\' or '**'
    value_initialized<bool> is_stream_pattern;  // Contains ':'
    value_initialized<bool> requires_root_path_match;  // Optimization flag

    std::tvstring root_path_optimized_away;     // Prefix for volume filtering
    string_matcher matcher;                      // Compiled pattern matcher

    MatchOperation() {}

    void init(std::tstring pattern);
    bool prematch(std::tvstring const& root_path) const;
};
```

### Pattern Initialization

```cpp
void MatchOperation::init(std::tstring pattern) {
    // 1. Detect regex mode (prefix with '>')
    is_regex = !pattern.empty() && *pattern.begin() == _T('>');
    if (is_regex) {
        pattern.erase(pattern.begin());
    }

    // 2. Detect path pattern (contains path separators or globstar)
    is_path_pattern = is_regex ||
        ~pattern.find(_T('\\')) ||
        ~pattern.find(_T("**"));

    // 3. Detect stream pattern (contains colon for ADS)
    is_stream_pattern = is_regex || ~pattern.find(_T(':'));

    // 4. Check if pattern starts with a specific drive/path
    requires_root_path_match =
        is_path_pattern &&
        !is_regex &&
        pattern.size() >= 2 &&
        *(pattern.begin() + 0) != _T('*') &&
        *(pattern.begin() + 0) != _T('?') &&
        *(pattern.begin() + 1) != _T('*') &&
        *(pattern.begin() + 1) != _T('?');

    // 5. Extract root path prefix for optimization
    if (requires_root_path_match) {
        root_path_optimized_away.insert(root_path_optimized_away.end(),
            pattern.begin(),
            std::find(pattern.begin(), pattern.end(), _T('\\'))
        );
        pattern.erase(pattern.begin(),
            pattern.begin() + root_path_optimized_away.size()
        );
    }



### Volume Pre-matching

Before searching a volume, UFFS checks if the pattern could possibly match:

```cpp
bool MatchOperation::prematch(std::tvstring const& root_path) const {
    return !requires_root_path_match ||
           (root_path.size() >= root_path_optimized_away.size() &&
            std::equal(root_path.begin(),
                root_path.begin() + root_path_optimized_away.size(),
                root_path_optimized_away.begin()
            )
           );
}
```

**Example**: Pattern `C:\Windows\*.dll` extracts `C:` as `root_path_optimized_away`. When searching:
- Volume `C:` → prematch returns true (search proceeds)
- Volume `D:` → prematch returns false (volume skipped)

---

## Search Execution

### The Search() Method

```cpp
void CMainDlg::Search() {
    // 1. Initialize pattern matcher
    MatchOperation matchop;
    try {
        std::tstring pattern;
        ATL::CComBSTR bstr;
        if (this->txtPattern.GetWindowText(bstr.m_str)) {
            pattern.assign(bstr, bstr.Length());
        }
        matchop.init(pattern);
    } catch (std::invalid_argument& ex) {
        // Handle invalid regex
        this->MessageBox(GetAnyErrorText(ex), IDS_ERROR_TITLE, MB_ICONERROR);
        return;
    }

    // 2. Collect volumes to search
    std::vector<uintptr_t> wait_handles;
    std::vector<NtfsIndex const volatile*> wait_indices;

    int selected = this->cmbDrive.GetCurSel();
    for (int ii = this->cmbDrive.GetCount(); ii > 0; --ii) {
        if (intrusive_ptr<NtfsIndex> const p =
            static_cast<NtfsIndex*>(this->cmbDrive.GetItemDataPtr(ii))) {

            if (selected == ii || selected == 0) {  // 0 = "All Drives"
                if (matchop.prematch(p->root_path())) {
                    wait_handles.push_back(p->finished_event());
                    wait_indices.push_back(p.get());
                }
            }
        }
    }

    // 3. Wait for indices and search
    // ... (see Volume Coordination section)
}
```

### Volume Coordination

UFFS waits for volume indices to complete before searching:

```cpp
// Wait for any index to complete
DWORD wait_result = WaitForMultipleObjects(
    static_cast<DWORD>(wait_handles.size()),
    &wait_handles[0],
    FALSE,  // Wait for ANY (not all)
    100     // 100ms timeout for UI responsiveness
);

if (wait_result < wait_handles.size()) {
    // An index is ready - search it
    NtfsIndex const volatile* i = wait_indices[wait_result];

    // Check for indexing errors
    unsigned int task_result = i->get_finished();
    if (task_result != 0) {
        MessageBox(GetAnyErrorText(task_result), IDS_ERROR_TITLE, MB_ICONERROR);
    }

    // Execute search on this volume
    std::tvstring root_path = i->root_path();
    lock(i)->matches([&](TCHAR const* name, size_t name_length,
                         bool ascii, NtfsIndex::key_type const& key,
                         size_t depth) {
        // Pattern matching callback
        // ...
    }, path, matchop.is_path_pattern, matchop.is_stream_pattern, false);
}
```

---

## Index Traversal

### The matches() Template

The core search uses depth-first traversal:

```cpp
template<class F>
void NtfsIndex::matches(F func, std::tvstring& path,
                        bool match_paths, bool match_streams,
                        bool match_attributes) const {
    Matcher<F&> matcher = {
        this, func, match_paths, match_streams, match_attributes, &path, 0
    };

    // Start from root directory (FRS 5)
    return matcher(0x000000000005);
}
```

### The Matcher Functor

```cpp
template<class F>
struct Matcher {
    NtfsIndex const* me;
    F func;
    bool match_paths;
    bool match_streams;
    bool match_attributes;
    std::tvstring* path;
    size_t basename_index_in_path;
    NameInfo name;
    size_t depth;

    // Entry point: process a file/directory by FRS
    void operator()(key_type::frs_type frs) {
        if (frs >= me->records_lookup.size()) return;

        TCHAR const dirsep = getdirsep();
        std::tvstring temp;

        Records::value_type const* i = me->find(frs);
        unsigned short ji = 0;

        // Iterate through all names (hard links)
        for (LinkInfo const* j = me->nameinfo(i); j;
             j = me->nameinfo(j->next_entry), ++ji) {

            size_t old_basename_index = basename_index_in_path;
            basename_index_in_path = path->size();

            // Build path component
            temp.clear();
            append_directional(temp, &dirsep, 1, 0);

            if (!(match_paths && frs == 0x00000005)) {  // Skip root "."
                append_directional(temp, &me->names[j->name.offset()],
                                   j->name.length, j->name.ascii() ? -1 : 0);
            }

            // Process this entry
            this->operator()(frs, ji, temp.data(), temp.size());

            basename_index_in_path = old_basename_index;
        }
    }

    // Process a specific name of a file/directory
    void operator()(key_type::frs_type frs, key_type::name_info_type name_info,
                    TCHAR const stream_prefix[], size_t stream_prefix_size) {
        Records::value_type const* i = me->find(frs);

        // Append path component
        path->insert(path->end(), stream_prefix, stream_prefix + stream_prefix_size);

        // Process streams
        unsigned short ki = 0;
        for (StreamInfo const* k = me->streaminfo(i); k;
             k = me->streaminfo(k->next_entry), ++ki) {

            // Build stream suffix if needed
            std::tvstring stream_name;
            if (match_streams && k->name.offset() != negative_one) {
                stream_name.push_back(_T(':'));
                append_directional(stream_name, &me->names[k->name.offset()],
                                   k->name.length, k->name.ascii() ? -1 : 0);
            }

            // Invoke callback with full path
            func(path->data(), path->size() + stream_name.size(),
                 false,  // Not ASCII (path is always TCHAR)
                 key_type(frs, name_info, ki),
                 depth);
        }

        // Recurse into children (for directories)
        if (i->stdinfo.attributes() & FILE_ATTRIBUTE_DIRECTORY) {
            ++depth;
            for (ChildInfo const* c = me->childinfo(i); c;
                 c = me->childinfo(c->next_entry)) {
                this->operator()(c->record_number);
            }
            --depth;
        }

        // Remove path component
        path->resize(basename_index_in_path);
    }
};
```

### Traversal Visualization

```
Root (FRS 5)
├── Windows (FRS 50)
│   ├── System32 (FRS 100)
│   │   ├── notepad.exe (FRS 12345)
│   │   └── calc.exe (FRS 12346)
│   └── Fonts (FRS 101)
└── Users (FRS 60)
    └── ...

Depth-first order:
1. FRS 5 (root)      depth=0  path=""
2. FRS 50 (Windows)  depth=1  path="\Windows"
3. FRS 100 (System32) depth=2  path="\Windows\System32"
4. FRS 12345 (notepad.exe) depth=3  path="\Windows\System32\notepad.exe"
5. FRS 12346 (calc.exe) depth=3  path="\Windows\System32\calc.exe"
6. FRS 101 (Fonts)   depth=2  path="\Windows\Fonts"
7. FRS 60 (Users)    depth=1  path="\Users"
...
```

---

## Pattern Matching Callback

### The Match Lambda

During traversal, each file is tested against the pattern:

```cpp
lock(i)->matches([&](TCHAR const* name, size_t name_length,
                     bool ascii, NtfsIndex::key_type const& key,
                     size_t depth) {
    // Progress update
    unsigned long const now = GetTickCount();
    if (dlg.ShouldUpdate(now)) {
        if (dlg.HasUserCancelled(now)) {
            throw CStructured_Exception(ERROR_CANCELLED, NULL);
        }
        // Update progress bar
    }

    ++current_progress_numerator;

    // Apply pattern matching
    TCHAR const* path_begin = name;
    size_t high_water_mark = 0;
    size_t* phigh_water_mark = matchop.is_path_pattern && trie_filtering
                               ? &high_water_mark : NULL;

    bool match = ascii
        ? matchop.matcher.is_match(
            static_cast<char const*>(static_cast<void const*>(path_begin)),
            name_length, phigh_water_mark)
        : matchop.matcher.is_match(path_begin, name_length, phigh_water_mark);

    if (match) {
        // Store result
        unsigned short depth2 = static_cast<unsigned short>(depth * 2U);

        if (shift_pressed) {
            // Group by depth
            if (depth2 >= results_at_depths.size()) {
                results_at_depths.resize(depth2 + 1);
            }
            results_at_depths[depth2].push_back(i, Results::value_type(key, depth2));
        } else {
            // Flat results
            results.push_back(i, Results::value_type(key, depth2));
        }
    }
}, path, matchop.is_path_pattern, matchop.is_stream_pattern, false);
```

### High Water Mark Optimization

For path patterns, the high water mark tracks how far into the pattern was matched:

```cpp
// Pattern: **\System32\*.dll
// Path: C:\Windows\System32\kernel32.dll

// During matching:
// "C:" matches "**" → high_water_mark = 2
// "\Windows" matches "**" → high_water_mark = 2
// "\System32" matches "\System32" → high_water_mark = 10
// "\kernel32.dll" matches "\*.dll" → high_water_mark = 16 (full match)
```

This allows early termination when a path cannot possibly match.

---

## Results Storage

### SearchResult Structure

```cpp
#pragma pack(push, 1)
struct SearchResult {
    typedef unsigned short index_type;   // Volume index
    typedef unsigned short depth_type;   // Directory depth

    explicit SearchResult(NtfsIndex::key_type const key, depth_type const depth)
        : _key(key), _depth(depth) {}

    NtfsIndex::key_type key() const { return this->_key; }
    depth_type depth() const { return static_cast<depth_type>(this->_depth); }

    NtfsIndex::key_type::index_type index() const { return this->_key.index(); }
    void index(NtfsIndex::key_type::index_type const value) {
        this->_key.index(value);
    }

private:
    NtfsIndex::key_type _key;  // FRS + name_info + stream_info
    depth_type _depth;         // Directory depth
};
#pragma pack(pop)
```

**Size**: ~10 bytes per result (packed)

### Results Container

```cpp
class Results : memheap_vector<SearchResult> {
    typedef Results this_type;
    typedef memheap_vector<value_type, allocator_type> base_type;
    typedef std::vector<intrusive_ptr<NtfsIndex volatile const>> Indexes;
    typedef std::vector<std::pair<Indexes::value_type::value_type*,
                                  SearchResult::index_type>> IndicesInUse;

    Indexes indexes;           // Keep volume indices alive
    IndicesInUse indices_in_use;  // Map volume pointer to index

public:
    // Reverse iteration (results stored in reverse order)
    typedef base_type::reverse_iterator iterator;
    typedef base_type::const_reverse_iterator const_iterator;

    iterator begin() { return this->base_type::rbegin(); }
    iterator end() { return this->base_type::rend(); }

    // Save volume index and return compact index
    SearchResult::index_type save_index(Indexes::value_type::element_type* index) {
        IndicesInUse::iterator j = std::lower_bound(
            indices_in_use.begin(), indices_in_use.end(),
            std::make_pair(index, IndicesInUse::value_type::second_type())
        );

        if (j == indices_in_use.end() || j->first != index) {
            indexes.push_back(index);
            j = indices_in_use.insert(j, IndicesInUse::value_type(
                index, static_cast<IndicesInUse::value_type::second_type>(
                    indexes.size() - 1)));
        }

        return j->second;
    }

    // Add result with volume tracking
    void push_back(Indexes::value_type::element_type* index,
                   base_type::const_reference value) {
        this->base_type::push_back(value);
        (*(this->base_type::end() - 1)).index(this->save_index(index));
    }

    // Get volume for result
    Indexes::value_type::element_type* ith_index(value_type::index_type i) const {
        return this->indexes[i];
    }
};
```

### Memory Layout

```
Results container:
┌─────────────────────────────────────────────────────────────────────────┐
│ base_type (memheap_vector<SearchResult>)                                │
├─────────────────────────────────────────────────────────────────────────┤
│ [SearchResult 0] [SearchResult 1] [SearchResult 2] ... [SearchResult N] │
│     10 bytes         10 bytes         10 bytes             10 bytes     │
└─────────────────────────────────────────────────────────────────────────┘

indexes vector:
┌─────────────────────────────────────────────────────────────────────────┐
│ [NtfsIndex* C:] [NtfsIndex* D:] [NtfsIndex* E:]                         │
│      index 0         index 1         index 2                            │
└─────────────────────────────────────────────────────────────────────────┘

indices_in_use (sorted by pointer for binary search):
┌─────────────────────────────────────────────────────────────────────────┐
│ [(0x1000, 0)] [(0x2000, 1)] [(0x3000, 2)]                               │
│   C: → 0        D: → 1        E: → 2                                    │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Result Sorting

### ResultCompare Template

Results can be sorted by various columns:

```cpp
template<int SubItem>
struct ResultCompare {
    ResultCompareBase* base;

    typedef ResultCompareBase::result_type result_type;

    result_type operator()(Results::value_type const& v) const {
        base->check_cancelled(&v);

        result_type result = result_type();

        // Include depth in sort key if grouping
        result.first = (base->variation & 0x2)
            ? static_cast<typename result_type::second_type>(~v.depth())
            : typename result_type::second_type();

        NtfsIndex const* p = base->this_->results.ith_index(v.index())->unvolatile();
        NtfsIndex::key_type key = v.key();

        switch (SubItem) {
            case COLUMN_INDEX_SIZE:
            case COLUMN_INDEX_SIZE_ON_DISK:
            case COLUMN_INDEX_DESCENDENTS: {
                NtfsIndex::size_info const& info = p->get_sizes(key);
                switch (SubItem) {
                    case COLUMN_INDEX_SIZE:
                        result.second = (base->variation & 0x1)
                            ? static_cast<unsigned long long>(info.length - info.allocated)
                              - (1ULL << 63)  // Sort by space saved
                            : static_cast<unsigned long long>(info.length);
                        break;
                    case COLUMN_INDEX_SIZE_ON_DISK:
                        result.second = (base->variation & 0x1)
                            ? info.bulkiness
                            : info.allocated;
                        break;
                    case COLUMN_INDEX_DESCENDENTS:
                        result.second = info.treesize;
                        break;
                }
                break;
            }

            case COLUMN_INDEX_CREATION_TIME:
            case COLUMN_INDEX_MODIFICATION_TIME:
            case COLUMN_INDEX_ACCESS_TIME: {
                NtfsIndex::standard_info const& info = p->get_stdinfo(key.frs());
                switch (SubItem) {
                    case COLUMN_INDEX_CREATION_TIME:
                        result.second = (base->variation & 0x1)
                            ? /* sort by FRS order */
                              ((static_cast<unsigned long long>(key.frs()) << 32) |
                               (static_cast<unsigned long long>(~key.name_info() & 0xFFFF) << 16) |
                               (static_cast<unsigned long long>(~key.stream_info() & 0xFFFF)))
                            : info.created;
                        break;
                    case COLUMN_INDEX_MODIFICATION_TIME:
                        result.second = info.written;
                        break;
                    case COLUMN_INDEX_ACCESS_TIME:
                        result.second = info.accessed;
                        break;
                }
                break;
            }
        }

        return result;
    }
};
```

### Sort Columns

| Column | SubItem | Sort Key |
|--------|---------|----------|
| Name | COLUMN_INDEX_NAME | Alphabetical (path string) |
| Size | COLUMN_INDEX_SIZE | File length |
| Size on Disk | COLUMN_INDEX_SIZE_ON_DISK | Allocated size |
| Descendants | COLUMN_INDEX_DESCENDENTS | Tree size (directories) |
| Created | COLUMN_INDEX_CREATION_TIME | Creation timestamp |
| Modified | COLUMN_INDEX_MODIFICATION_TIME | Write timestamp |
| Accessed | COLUMN_INDEX_ACCESS_TIME | Access timestamp |

### Variation Flags

The `variation` field modifies sort behavior:

| Bit | Meaning |
|-----|---------|
| 0x1 | Alternate sort (e.g., space saved instead of size) |
| 0x2 | Group by depth |

---

## Path Normalization

### NormalizePath Function

User input paths are normalized before use:

```cpp
std::tvstring NormalizePath(std::tvstring const& path) {
    std::tvstring result;
    bool wasSep = false;
    bool isCurrentlyOnPrefix = true;

    for (size_t i = 0; i < path.size(); i++) {
        TCHAR const& c = path[i];
        isCurrentlyOnPrefix &= isdirsep(c);

        // Remove duplicate separators (except at start)
        if (isCurrentlyOnPrefix || !wasSep || !isdirsep(c)) {
            result.push_back(c);
        }

        wasSep = isdirsep(c);
    }

    // Make path absolute if relative
    if (!isrooted(result.begin(), result.end())) {
        std::tvstring currentDir(32 * 1024, _T('\0'));
        currentDir.resize(GetCurrentDirectory(
            static_cast<DWORD>(currentDir.size()), &currentDir[0]));
        adddirsep(currentDir);
        result = currentDir + result;
    }

    remove_path_stream_and_trailing_sep(result);
    return result;
}
```

### Normalization Examples

| Input | Output |
|-------|--------|
| `C:\Windows\\System32\` | `C:\Windows\System32` |
| `Windows\System32` | `C:\Users\...\Windows\System32` |
| `\\server\share\` | `\\server\share` |
| `C:\file.txt:stream` | `C:\file.txt` |

---

## Progress Reporting

### During Search

```cpp
// In match callback
unsigned long const now = GetTickCount();
if (dlg.ShouldUpdate(now) || current_progress_denominator - current_progress_numerator <= 1) {
    if (dlg.HasUserCancelled(now)) {
        throw CStructured_Exception(ERROR_CANCELLED, NULL);
    }

    // Calculate overall progress
    size_t temp_overall_progress_numerator = overall_progress_numerator;
    for (size_t k = 0; k < wait_indices.size(); ++k) {
        temp_overall_progress_numerator += wait_indices[k]->records_so_far();
    }

    // Update progress dialog
    dlg.SetProgress(temp_overall_progress_numerator, overall_progress_denominator);
    dlg.SetProgressText(current_path);
    dlg.Flush();
}
```

### Cancellation

Users can cancel searches via the progress dialog:

```cpp
if (dlg.HasUserCancelled(now)) {
    throw CStructured_Exception(ERROR_CANCELLED, NULL);
}
```

The exception is caught at the search level, and partial results are displayed.

---

## Summary

The UFFS search algorithm achieves high performance through:

1. **Pattern analysis**: Detect pattern type and optimize volume selection
2. **Volume pre-matching**: Skip volumes that cannot match
3. **Depth-first traversal**: Efficient index traversal with incremental path building
4. **Compact results**: 10 bytes per result with volume index deduplication
5. **Lazy path resolution**: Full paths built only when displayed
6. **Cancellable operations**: Progress tracking with user cancellation

Key implementation patterns:

| Pattern | Purpose |
|---------|---------|
| `MatchOperation` | Query analysis and optimization |
| `Matcher` template | Depth-first index traversal |
| `Results` container | Compact result storage |
| `ResultCompare` | Multi-column sorting |
| `NormalizePath` | Input sanitization |

---

## See Also

- [05-pattern-matching-engine.md](05-pattern-matching-engine.md) - Pattern matching details
- [07-indexing-engine.md](07-indexing-engine.md) - Index structure
- [06-gui-implementation.md](06-gui-implementation.md) - Result display
- [03-concurrency-deep-dive.md](03-concurrency-deep-dive.md) - Threading model
