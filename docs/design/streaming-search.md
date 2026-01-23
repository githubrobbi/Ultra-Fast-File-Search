# Streaming Search Design Document

## Overview

This document describes the design for **streaming search** functionality in UFFS, allowing search results to be output progressively as files are indexed, rather than waiting for complete index construction.

## Motivation

### Current Behavior
1. User initiates search
2. UFFS waits for MFT indexing to complete (can take 5-30 seconds on large volumes)
3. Search executes against complete index
4. All results displayed at once

### Proposed Behavior
1. User initiates search with `--stream` flag (CLI) or checkbox (GUI)
2. Results begin appearing within milliseconds
3. More results appear as indexing progresses
4. Final results match non-streaming behavior

### Benefits
- **Faster time-to-first-result**: Users see matches immediately
- **Better perceived performance**: Application feels more responsive
- **Pipeline-friendly**: CLI output can be piped to other tools immediately
- **Large volume support**: Useful when indexing takes significant time

---

## Current Architecture Analysis

### Existing Streaming-Friendly Components

| Component | Current State | Streaming Ready? |
|-----------|--------------|------------------|
| `NtfsIndex::matches()` | Callback-based traversal | ✅ Yes |
| `NtfsIndex::_mutex` | Recursive mutex with lock_ptr | ✅ Yes |
| `_records_so_far` | Atomic progress counter | ✅ Yes |
| `BackgroundWorker` | Async task queue | ✅ Yes |
| MFT I/O | Overlapped async reads | ✅ Yes |
| CLI output | Direct stdout writes | ✅ Yes |
| GUI ListView | Virtual mode (LVS_OWNERDATA) | ⚠️ Needs batching |

### Current Search Flow

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Start Search   │────▶│ Wait for Index   │────▶│ Execute Search  │
│                 │     │ (finished_event) │     │ (matches())     │
└─────────────────┘     └──────────────────┘     └─────────────────┘
                               ▲                        │
                               │                        ▼
                        Blocks until              ┌─────────────────┐
                        100% complete             │ Display Results │
                                                  └─────────────────┘
```

### Proposed Streaming Flow

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Start Search   │────▶│ Search Available │────▶│ Output Results  │
│                 │     │ Records          │     │ Immediately     │
└─────────────────┘     └──────────────────┘     └─────────────────┘
                               │                        │
                               ▼                        │
                        ┌──────────────────┐            │
                        │ More Records     │◀───────────┘
                        │ Available?       │
                        └──────────────────┘
                               │ Yes
                               ▼
                        ┌──────────────────┐
                        │ Search New       │
                        │ Records Only     │
                        └──────────────────┘
```

---

## Technical Challenges

### Challenge 1: Incomplete Parent Chains

**Problem**: When a file record is indexed, its parent directory may not yet be indexed. The current `matches()` traverses from root (FRS 5) downward, so orphaned files are unreachable.

**Example**:
```
Time T1: File "notepad.exe" (FRS 12345) indexed, parent=100
Time T2: Directory "System32" (FRS 100) not yet indexed
Result:  notepad.exe is invisible to tree traversal
```

**Solutions**:

| Approach | Pros | Cons |
|----------|------|------|
| A. Flat iteration | Simple, immediate results | No paths until post-processing |
| B. Deferred path resolution | Full paths eventually | Complex bookkeeping |
| C. Two-phase output | Clean separation | Requires output format change |
| D. Wait for parent chain | Correct paths always | Delays some results |

**Recommended**: Approach C (Two-phase) for CLI, Approach D for GUI.

### Challenge 2: Duplicate Prevention

**Problem**: As we search incrementally, we must avoid outputting the same file multiple times.

**Solution**: Track searched FRS range or use a bitset of emitted records.

```cpp
class StreamingSearchState {
    size_t last_searched_frs;        // High-water mark
    Bitmap emitted_records;          // For dedup (optional)
    std::tvstring current_pattern;   // Cache compiled pattern
};
```

### Challenge 3: Thread Safety During Mutation

**Problem**: Index structures are being modified while we read them.

**Current Protection**: `recursive_mutex` already protects access.

**Enhancement**: Use read-write lock for better concurrency:
```cpp
// Current: exclusive lock for all access
lock_ptr<NtfsIndex> locked = lock(index);

// Proposed: shared lock for reads
shared_lock_ptr<NtfsIndex const> locked = shared_lock(index);
```

### Challenge 4: GUI Performance

**Problem**: Updating ListView too frequently causes flicker and slowdown.

**Solution**: Batch updates with throttling:
```cpp
class BatchedResultCollector {
    std::vector<Result> pending;
    DWORD last_flush_time;
    size_t batch_size = 1000;
    DWORD flush_interval_ms = 100;

    void add(Result r) {
        pending.push_back(r);
        if (should_flush()) flush();
    }

    bool should_flush() {
        return pending.size() >= batch_size ||
               GetTickCount() - last_flush_time >= flush_interval_ms;
    }
};
```

---

## Implementation Plan

### Phase 1: CLI Streaming (Priority: High)

**Goal**: `uffs.com --stream *.exe` outputs results as indexing progresses.

**Changes Required**:

1. **Add `--stream` command-line flag**
   ```cpp
   cl::opt<bool> StreamOutput("stream",
       cl::desc("Output results as they are found during indexing"),
       cl::init(false));
   ```

2. **Create `StreamingMatcher` class**
   ```cpp
   class StreamingMatcher {
       NtfsIndex volatile* index;
       Matcher& pattern;
       size_t last_records_count;

   public:
       // Search only newly-indexed records
       void search_incremental(OutputCallback callback);

       // Check if more records available
       bool has_new_records() const;
   };
   ```

3. **Modify main search loop**
   ```cpp
   if (StreamOutput) {
       StreamingMatcher sm(index, pattern);
       while (!index->is_finished() || sm.has_new_records()) {
           sm.search_incremental([](auto& result) {
               output_result(result);
           });
           Sleep(10);  // Yield to indexing thread
       }
   } else {
       // Existing behavior
       WaitForSingleObject(index->finished_event(), INFINITE);
       search_all(index, pattern);
   }
   ```

4. **Flat record iteration** (bypass tree traversal)
   ```cpp
   void search_flat(size_t from_frs, size_t to_frs, Callback cb) {
       for (size_t frs = from_frs; frs < to_frs; ++frs) {
           if (auto* record = index->find(frs)) {
               if (matches_pattern(record)) {
                   cb(frs, record);  // Path resolved later or on-demand
               }
           }
       }
   }
   ```

**Output Format** (streaming mode):
```
# Results appear incrementally:
C:\Windows\System32\notepad.exe
C:\Windows\System32\calc.exe
# ... more results as indexing continues ...
C:\Program Files\app.exe
# Search complete
```

### Phase 2: Path Resolution Optimization

**Goal**: Provide full paths even for files indexed before their parents.

**Approach**: Lazy path resolution with caching.

```cpp
class LazyPathResolver {
    NtfsIndex const* index;
    mutable std::unordered_map<uint32_t, std::tvstring> path_cache;

public:
    std::tvstring const& get_path(uint32_t frs) const {
        auto it = path_cache.find(frs);
        if (it != path_cache.end()) return it->second;

        // Build path by walking parent chain
        std::tvstring path;
        if (try_build_path(frs, path)) {
            return path_cache.emplace(frs, std::move(path)).first->second;
        }

        // Parent not yet indexed - return partial path
        return path_cache.emplace(frs, format_partial(frs)).first->second;
    }

private:
    bool try_build_path(uint32_t frs, std::tvstring& out) const;
    std::tvstring format_partial(uint32_t frs) const;
};
```

**Partial Path Format**:
```
[FRS:12345]\notepad.exe     # Parent not yet known
C:\Windows\System32\notepad.exe  # Updated when parent indexed
```

### Phase 3: GUI Streaming (Priority: Medium)

**Goal**: Results appear in ListView during indexing.

**Changes Required**:

1. **Add streaming checkbox to UI**
   - Location: Search options panel
   - Default: Off (preserve current behavior)

2. **Batched result collector**
   ```cpp
   class StreamingResultCollector {
       CMainDlg* dlg;
       Results pending_results;
       DWORD last_ui_update;

       static const size_t BATCH_SIZE = 500;
       static const DWORD UPDATE_INTERVAL_MS = 100;

   public:
       void add_result(Result r) {
           pending_results.push_back(r);
           maybe_flush_to_ui();
       }

   private:
       void maybe_flush_to_ui() {
           DWORD now = GetTickCount();
           if (pending_results.size() >= BATCH_SIZE ||
               now - last_ui_update >= UPDATE_INTERVAL_MS) {
               flush_to_ui();
           }
       }

       void flush_to_ui() {
           // Append to main results vector
           dlg->results.insert(dlg->results.end(),
               pending_results.begin(), pending_results.end());
           pending_results.clear();

           // Update ListView item count
           dlg->lvFiles.SetItemCountEx(dlg->results.size(),
               LVSICF_NOINVALIDATEALL);

           last_ui_update = GetTickCount();
       }
   };
   ```

3. **Progress indicator enhancement**
   - Show "Searching... (X results found, indexing Y%)"
   - Update result count in real-time

### Phase 4: Advanced Features (Priority: Low)

1. **Result deduplication** - Handle files with multiple hard links
2. **Incremental sorting** - Maintain sort order as results stream in
3. **Result streaming to file** - `--stream --output results.txt`
4. **WebSocket streaming** - For potential web UI integration

---

## API Design

### New Public Methods

```cpp
class NtfsIndex {
public:
    // Existing
    void matches(F func, ...) const;
    size_t records_so_far() const volatile;
    bool is_finished() const volatile;

    // New for streaming
    void matches_range(F func, size_t from_frs, size_t to_frs, ...) const;
    size_t safe_records_count() const volatile;  // Records with complete parent chains
};

// New streaming search coordinator
class StreamingSearch {
public:
    StreamingSearch(NtfsIndex volatile* index, Pattern const& pattern);

    void start();
    void stop();
    bool is_complete() const;

    // Callback receives results as they're found
    void set_result_callback(std::function<void(Result const&)> cb);

    // Statistics
    size_t results_found() const;
    size_t records_searched() const;
    double progress() const;  // 0.0 to 1.0
};
```

### Command-Line Interface

```bash
# Streaming mode (new)
uffs.com --stream "*.exe"
uffs.com --stream --no-paths "*.dll"    # FRS only, fastest
uffs.com --stream --resolve-paths "*.txt"  # Wait for full paths

# Existing (unchanged)
uffs.com "*.exe"                        # Wait for complete index
```

---

## Performance Considerations

### Expected Performance Characteristics

| Metric | Current | Streaming |
|--------|---------|-----------|
| Time to first result | 5-30s | <100ms |
| Total search time | T | T + ~5% overhead |
| Memory overhead | Baseline | +~1MB for tracking |
| CPU overhead | Baseline | +~2% for coordination |

### Optimization Opportunities

1. **Lock-free progress tracking** - Already using atomics
2. **Read-write locks** - Allow concurrent reads during indexing
3. **Prefetch hints** - Predict which records will be searched next
4. **SIMD pattern matching** - Batch multiple filenames

---

## Testing Strategy

### Unit Tests

```cpp
TEST(StreamingSearch, OutputsResultsDuringIndexing) {
    MockNtfsIndex index;
    index.set_records_so_far(100);
    index.set_finished(false);

    StreamingSearch search(&index, Pattern("*.exe"));
    std::vector<Result> results;
    search.set_result_callback([&](auto& r) { results.push_back(r); });

    search.start();
    EXPECT_GT(results.size(), 0);  // Results before indexing complete

    index.set_records_so_far(1000);
    index.set_finished(true);
    search.wait_complete();

    EXPECT_EQ(results.size(), expected_total);
}
```

### Integration Tests

1. **Correctness**: Streaming results == non-streaming results (same set)
2. **No duplicates**: Each file appears exactly once
3. **Performance**: Time-to-first-result < 100ms
4. **Cancellation**: Clean abort during streaming

---

## Rollout Plan

| Phase | Scope | Timeline | Risk |
|-------|-------|----------|------|
| 1 | CLI `--stream` flag | 1-2 weeks | Low |
| 2 | Path resolution | 1 week | Medium |
| 3 | GUI streaming | 2-3 weeks | Medium |
| 4 | Advanced features | Ongoing | Low |

### Feature Flags

```cpp
#define UFFS_STREAMING_SEARCH 1      // Master switch
#define UFFS_STREAMING_CLI 1         // CLI support
#define UFFS_STREAMING_GUI 0         // GUI support (disabled initially)
#define UFFS_STREAMING_LAZY_PATHS 1  // Lazy path resolution
```

---

## Open Questions

1. **Should streaming be the default?** - Probably not initially; opt-in via flag
2. **How to handle sorting in streaming mode?** - Disable sorting until complete, or use incremental sort
3. **Should we show "incomplete" indicator for partial paths?** - Yes, with option to hide
4. **Memory limit for result buffer?** - Consider pagination for very large result sets

---

## References

- [07-indexing-engine.md](07-indexing-engine.md) - Index structure details
- [08-search-algorithm.md](08-search-algorithm.md) - Current search implementation
- [MFT_READING_CONCURRENCY.md](MFT_READING_CONCURRENCY.md) - Async I/O details
- [BackgroundWorker.hpp](../../UltraFastFileSearch-code/BackgroundWorker.hpp) - Threading infrastructure

