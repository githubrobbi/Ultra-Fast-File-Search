# Concurrency Deep Dive

## Introduction

Ultra Fast File Search achieves its remarkable performance through sophisticated use of Windows concurrency primitives. This document provides an exhaustive analysis of every threading pattern, synchronization mechanism, and concurrent data structure used in the codebase.

A developer reading this document should be able to:
1. Understand every concurrency primitive used
2. Reason about thread safety of any code path
3. Extend the codebase without introducing race conditions
4. Debug concurrency issues effectively

---

## Threading Architecture Overview

### Thread Types in UFFS

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         UFFS Thread Architecture                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────┐                                                    │
│  │   Main UI       │  - Message loop (GetMessage/DispatchMessage)       │
│  │   Thread        │  - User interaction handling                       │
│  │                 │  - ListView updates                                │
│  └────────┬────────┘  - Progress display                                │
│           │                                                             │
│           │ PostMessage()                                               │
│           ▼                                                             │
│  ┌─────────────────┐                                                    │
│  │ BackgroundWorker│  - One per drive (C:, D:, etc.)                    │
│  │   Threads       │  - Icon loading                                    │
│  │   (N drives)    │  - Deferred operations                             │
│  └────────┬────────┘                                                    │
│           │                                                             │
│           │ IoCompletionPort                                            │
│           ▼                                                             │
│  ┌─────────────────┐                                                    │
│  │  IOCP Worker    │  - Number = CPU cores (or OMP_NUM_THREADS)         │
│  │   Threads       │  - MFT reading                                     │
│  │   (N cores)     │  - Async I/O completion handling                   │
│  └─────────────────┘  - Record parsing                                  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Thread Count Determination

```cpp
static unsigned long get_num_threads() {
    unsigned long num_threads = 0;
    
#ifdef _OPENMP
    // If OpenMP is available, use its thread count
    #pragma omp parallel
    #pragma omp atomic
    ++num_threads;
#else
    // Check OMP_NUM_THREADS environment variable
    TCHAR const* const omp_num_threads = _tgetenv(_T("OMP_NUM_THREADS"));
    if (int const n = omp_num_threads ? _ttoi(omp_num_threads) : 0) {
        num_threads = static_cast<int>(n);
    } else {
#ifdef _DEBUG
        num_threads = 1;  // Single thread in debug for easier debugging
#else
        // Use number of CPU cores
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        num_threads = sysinfo.dwNumberOfProcessors;
#endif
    }
#endif
    return num_threads;
}
```

**Key Design Decisions**:
- Debug builds use 1 thread for deterministic debugging
- Release builds scale to CPU core count
- `OMP_NUM_THREADS` environment variable allows override
- OpenMP integration when available

---

## I/O Completion Ports (IOCP) - The Core Async Engine

### Why IOCP?

I/O Completion Ports are the most scalable async I/O mechanism on Windows:
- **Single wait point** for multiple I/O operations
- **Automatic thread pool management** by the kernel
- **LIFO thread wake-up** for cache efficiency
- **Bounded concurrency** to prevent thread explosion

### The IoCompletionPort Class

```cpp
class IoCompletionPort {
    typedef IoCompletionPort this_type;
    
protected:
    // Pending I/O operations
    struct Task {
        HANDLE file;
        unsigned long issuing_thread_id;
        void* buffer;
        unsigned long cb;  // Byte count
        intrusive_ptr<Overlapped> overlapped;
    };
    
    // Worker thread wrapper with RAII cleanup
    struct WorkerThread {
        Handle handle;
        ~WorkerThread() {
            WaitForSingleObject(this->handle, INFINITE);
        }
    };
    
    // Core state
    Handle _handle;                              // IOCP handle
    std::vector<WorkerThread> _threads;          // Worker thread pool
    std::vector<Task> _pending;                  // Pending operations
    atomic_namespace::atomic<bool> _initialized; // Lazy init flag
    atomic_namespace::atomic<bool> _terminated;  // Shutdown flag
    mutable atomic_namespace::recursive_mutex _mutex;
    
public:
    IoCompletionPort() 
        : _handle(CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0))
        , _initialized(false)
        , _terminated(false) 
    {}
};
```

### Worker Thread Entry Point

```cpp
static unsigned int CALLBACK iocp_worker(void* me) {
    return static_cast<this_type volatile*>(me)->worker();
}

virtual unsigned int worker() volatile {
    return this->worker(INFINITE);
}

virtual unsigned int worker(unsigned long const timeout) volatile {
    unsigned int result = 0;
    ULONG_PTR key;
    OVERLAPPED* overlapped_ptr;
    Overlapped* p;
    
    try {
        // Main completion loop
        for (unsigned long nr; 
             GetQueuedCompletionStatus(this->_handle, &nr, &key, 
                                       &overlapped_ptr, timeout);) {
            
            p = static_cast<Overlapped*>(overlapped_ptr);
            intrusive_ptr<Overlapped> overlapped(p, false);  // Don't add ref
            
            if (overlapped.get()) {
                // Invoke the completion handler
                int r = (*overlapped)(static_cast<size_t>(nr), key);
                
                if (r > 0) {
                    // Handler requested re-queue
                    r = this->try_post(nr, key, overlapped) ? 0 : -1;
                }
                
                if (r >= 0) {
                    overlapped.detach();  // Don't release ref
                }
            }
            
            // Check for pending operations to issue
            // (key == 1 signals pending work)
            if (key == 1) {
                this->process_pending();
            }
        }
    }
    catch (std::exception& ex) {
        result = ERROR_UNHANDLED_EXCEPTION;
    }
    catch (CStructured_Exception& ex) {
        result = ex.GetSENumber();
    }

    return result;
}
```

### Associating Files with IOCP

```cpp
bool associate(HANDLE const file, ULONG_PTR const key) volatile {
    return CreateIoCompletionPort(file, this->_handle, key, 0) == this->_handle;
}
```

When a file handle is associated with an IOCP:
1. All async I/O on that handle completes through the IOCP
2. The `key` parameter is passed to completion handlers
3. Multiple files can share one IOCP

### Posting Completion Packets

```cpp
bool try_post(unsigned long const nr, ULONG_PTR const key,
              intrusive_ptr<Overlapped> const& overlapped) volatile {
    bool result = !!PostQueuedCompletionStatus(
        this->_handle, nr, key, overlapped.get());
    if (result) {
        overlapped.detach();  // IOCP now owns the reference
    }
    return result;
}
```

### Pending Operation Management

For operations that can't be issued immediately (e.g., too many outstanding):

```cpp
void pend(HANDLE const file, void* const buffer, unsigned long const cb,
          intrusive_ptr<Overlapped> const& overlapped) volatile {
    atomic_namespace::unique_lock<atomic_namespace::recursive_mutex> guard(this->_mutex);
    this->_pending.push_back(Task());
    Task& task = this->_pending.back();
    task.file = file;
    task.issuing_thread_id = GetCurrentThreadId();
    task.buffer = buffer;
    task.cb = cb;
    task.overlapped = overlapped;
}
```

### OleIoCompletionPort - COM-Aware Variant

For operations requiring COM (like shell icon extraction):

```cpp
class OleIoCompletionPort : public IoCompletionPort {
    virtual unsigned int worker() volatile {
        // Initialize COM for this thread
        HRESULT const hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

        // Call base worker
        unsigned int const result = this->IoCompletionPort::worker();

        // Cleanup COM
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
        return result;
    }
};
```

---

## Atomic Operations

### The atomic_namespace Implementation

UFFS provides its own atomic primitives for compatibility and control:

```cpp
namespace atomic_namespace {
    // Memory ordering constants
    enum memory_order {
        memory_order_relaxed,
        memory_order_consume,
        memory_order_acquire,
        memory_order_release,
        memory_order_acq_rel,
        memory_order_seq_cst
    };
}
```

### atomic<bool>

```cpp
template<>
class atomic<bool> {
    typedef bool value_type;
    typedef long storage_type;  // Windows interlocked requires long

    storage_type volatile _value;

public:
    atomic() : _value() {}
    atomic(value_type const value) : _value(value) {}

    // Atomic load
    value_type load(memory_order const order = memory_order_seq_cst) const volatile {
        storage_type value = this->_value;
        if (order != memory_order_relaxed) {
            _ReadWriteBarrier();  // Compiler barrier
        }
        return !!value;
    }

    // Atomic store
    void store(value_type const value,
               memory_order const order = memory_order_seq_cst) volatile {
        if (order == memory_order_seq_cst) {
            this->exchange(value);  // Full barrier
        } else {
            if (order != memory_order_relaxed) {
                _ReadWriteBarrier();
            }
            this->_value = value;
        }
    }

    // Atomic exchange
    value_type exchange(value_type const value,
                        memory_order const = memory_order_seq_cst) volatile {
        return !!_InterlockedExchange(&this->_value, value);
    }

    // Compare-and-swap
    bool compare_exchange_strong(value_type& expected, value_type const desired,
                                 memory_order const = memory_order_seq_cst) volatile {
        storage_type const old = _InterlockedCompareExchange(
            &this->_value, desired, expected);
        bool const succeeded = (!!old == expected);
        expected = !!old;
        return succeeded;
    }
};
```

### atomic<unsigned int>

```cpp
template<>
class atomic<unsigned int> {
    typedef unsigned int value_type;
    typedef long storage_type;

    storage_type volatile _value;

public:
    // Atomic increment
    value_type fetch_add(value_type const addend,
                         memory_order const = memory_order_seq_cst) volatile {
        return static_cast<value_type>(_InterlockedExchangeAdd(
            &this->_value, static_cast<storage_type>(addend)));
    }

    // Atomic decrement
    value_type fetch_sub(value_type const subtrahend,
                         memory_order const = memory_order_seq_cst) volatile {
        return static_cast<value_type>(_InterlockedExchangeAdd(
            &this->_value, -static_cast<storage_type>(subtrahend)));
    }

    // Prefix increment
    value_type operator++() volatile {
        return static_cast<value_type>(_InterlockedIncrement(&this->_value));
    }

    // Prefix decrement
    value_type operator--() volatile {
        return static_cast<value_type>(_InterlockedDecrement(&this->_value));
    }
};
```

### atomic<long long> (64-bit)

```cpp
template<>
class atomic<long long> {
    typedef long long value_type;

    value_type volatile _value;

public:
    value_type load(memory_order const order = memory_order_seq_cst) const volatile {
#if defined(_M_AMD64) || defined(_M_X64)
        // 64-bit: native load is atomic
        value_type value = this->_value;
        if (order != memory_order_relaxed) {
            _ReadWriteBarrier();
        }
        return value;
#else
        // 32-bit: use interlocked compare-exchange for atomicity
        return _InterlockedCompareExchange64(
            const_cast<value_type volatile*>(&this->_value), 0, 0);
#endif
    }

    value_type fetch_add(value_type const addend,
                         memory_order const = memory_order_seq_cst) volatile {
        return _InterlockedExchangeAdd64(&this->_value, addend);
    }
};
```

### atomic_flag - Spin Lock Primitive

```cpp
class atomic_flag {
    typedef long storage_type;
    storage_type volatile _value;

public:
    atomic_flag() : _value() {}

    // Test and set - returns previous value
    bool test_and_set(memory_order const = memory_order_seq_cst) volatile {
        return !!_InterlockedExchange(&this->_value, 1);
    }

    // Clear the flag
    void clear(memory_order const order = memory_order_seq_cst) volatile {
        if (order == memory_order_seq_cst) {
            _InterlockedExchange(&this->_value, 0);
        } else {
            if (order != memory_order_relaxed) {
                _ReadWriteBarrier();
            }
            this->_value = 0;
        }
    }
};
```

### Memory Ordering Explained

| Order | Meaning | Use Case |
|-------|---------|----------|
| `relaxed` | No ordering guarantees | Counters, statistics |
| `acquire` | Reads after this see writes before release | Lock acquisition |
| `release` | Writes before this visible after acquire | Lock release |
| `acq_rel` | Both acquire and release | Read-modify-write |
| `seq_cst` | Total ordering across all threads | Default, safest |

---

## Mutex and Lock Primitives

### recursive_mutex

UFFS provides multiple implementations based on available features:

```cpp
#if defined(_OPENMP)
// OpenMP implementation
class recursive_mutex {
    omp_lock_t _lock;
public:
    recursive_mutex() { omp_init_lock(&this->_lock); }
    ~recursive_mutex() { omp_destroy_lock(&this->_lock); }
    void lock() volatile { omp_set_lock(const_cast<omp_lock_t*>(&this->_lock)); }
    void unlock() volatile { omp_unset_lock(const_cast<omp_lock_t*>(&this->_lock)); }
};

#elif defined(_WIN32)
// Windows CRITICAL_SECTION implementation
class recursive_mutex {
    CRITICAL_SECTION _cs;
public:
    recursive_mutex() { InitializeCriticalSection(&this->_cs); }
    ~recursive_mutex() { DeleteCriticalSection(&this->_cs); }
    void lock() volatile {
        EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&this->_cs));
    }
    void unlock() volatile {
        LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&this->_cs));
    }
};

#else
// Standard library fallback
class recursive_mutex {
    std::recursive_mutex _mutex;
public:
    void lock() volatile {
        const_cast<std::recursive_mutex&>(this->_mutex).lock();
    }
    void unlock() volatile {
        const_cast<std::recursive_mutex&>(this->_mutex).unlock();
    }
};
#endif
```

**Why CRITICAL_SECTION?**
- Faster than std::mutex for uncontended locks
- Supports recursion (same thread can lock multiple times)
- No kernel transition for uncontended case

### spin_lock

For very short critical sections:

```cpp
class spin_lock {
    atomic_flag _flag;

public:
    void lock() volatile {
        while (this->_flag.test_and_set(memory_order_acquire)) {
            // Spin until we acquire the lock
            // Could add _mm_pause() for better performance
        }
    }

    void unlock() volatile {
        this->_flag.clear(memory_order_release);
    }
};
```

**When to use spin_lock vs recursive_mutex:**
- **spin_lock**: Very short critical sections (< 100 cycles)
- **recursive_mutex**: Longer operations, or when recursion needed

### unique_lock - RAII Lock Guard

```cpp
template<class Mutex>
class unique_lock {
    Mutex volatile* _mutex;
    bool _owns;

public:
    explicit unique_lock(Mutex volatile& mutex)
        : _mutex(&mutex), _owns(false) {
        this->lock();
    }

    ~unique_lock() {
        if (this->_owns) {
            this->unlock();
        }
    }

    void lock() {
        this->_mutex->lock();
        this->_owns = true;
    }

    void unlock() {
        this->_mutex->unlock();
        this->_owns = false;
    }

    // Move semantics
    unique_lock(unique_lock&& other)
        : _mutex(other._mutex), _owns(other._owns) {
        other._mutex = NULL;
        other._owns = false;
    }
};
```

### lock_ptr - Safe Object Access

Combines a pointer with its protecting mutex:

```cpp
template<class T, class Mutex = recursive_mutex>
class lock_ptr {
    T volatile* _ptr;
    unique_lock<Mutex> _lock;

public:
    lock_ptr(T volatile* ptr, Mutex volatile& mutex)
        : _ptr(ptr), _lock(mutex) {}

    T volatile* operator->() const { return this->_ptr; }
    T volatile& operator*() const { return *this->_ptr; }
};
```

Usage pattern:
```cpp
class ThreadSafeContainer {
    std::vector<int> _data;
    mutable recursive_mutex _mutex;

public:
    lock_ptr<std::vector<int>> get_data() {
        return lock_ptr<std::vector<int>>(&_data, _mutex);
    }
};

// Usage:
auto data = container.get_data();  // Lock acquired
data->push_back(42);               // Safe access
// Lock released when data goes out of scope
```

### spin_atomic<T> - Atomic Access to Complex Types

For types that can't use hardware atomics:

```cpp
template<class T>
class spin_atomic {
    T _value;
    mutable spin_lock _lock;

public:
    spin_atomic() : _value() {}
    spin_atomic(T const& value) : _value(value) {}

    T load() const volatile {
        spin_lock volatile& lock = const_cast<spin_lock volatile&>(this->_lock);
        lock.lock();
        T result = const_cast<T const&>(this->_value);
        lock.unlock();
        return result;
    }

    void store(T const& value) volatile {
        this->_lock.lock();
        const_cast<T&>(this->_value) = value;
        this->_lock.unlock();
    }

    T exchange(T const& value) volatile {
        this->_lock.lock();
        T result = const_cast<T const&>(this->_value);
        const_cast<T&>(this->_value) = value;
        this->_lock.unlock();
        return result;
    }
};
```

---

## Reference Counting for Async Operations

### The RefCounted<T> Base Class

Reference counting is essential for async operations where ownership is unclear:

```cpp
template<class T>
class RefCounted {
    atomic_namespace::atomic<unsigned int> _refs;

protected:
    RefCounted() : _refs(1) {}  // Born with refcount 1
    RefCounted(RefCounted const&) : _refs(1) {}  // Copies start fresh

    virtual ~RefCounted() {}

public:
    void AddRef() volatile {
        this->_refs.fetch_add(1, atomic_namespace::memory_order_relaxed);
    }

    void Release() volatile {
        // Use acq_rel: acquire to see all writes, release for destructor
        if (this->_refs.fetch_sub(1, atomic_namespace::memory_order_acq_rel) == 1) {
            delete const_cast<T*>(static_cast<T const volatile*>(this));
        }
    }

    unsigned int refs() const volatile {
        return this->_refs.load(atomic_namespace::memory_order_relaxed);
    }
};
```

**Memory Ordering Rationale:**
- `fetch_add` uses `relaxed`: just incrementing, no synchronization needed
- `fetch_sub` uses `acq_rel`:
  - **acquire**: see all writes made before other threads released
  - **release**: ensure destructor sees all prior writes

### intrusive_ptr<T> - Smart Pointer for RefCounted

```cpp
template<class T>
class intrusive_ptr {
    T* _ptr;

public:
    intrusive_ptr() : _ptr(NULL) {}

    // Take ownership, optionally add reference
    intrusive_ptr(T* ptr, bool add_ref = true) : _ptr(ptr) {
        if (this->_ptr && add_ref) {
            this->_ptr->AddRef();
        }
    }

    // Copy: add reference
    intrusive_ptr(intrusive_ptr const& other) : _ptr(other._ptr) {
        if (this->_ptr) {
            this->_ptr->AddRef();
        }
    }

    // Move: transfer ownership
    intrusive_ptr(intrusive_ptr&& other) : _ptr(other._ptr) {
        other._ptr = NULL;
    }

    ~intrusive_ptr() {
        if (this->_ptr) {
            this->_ptr->Release();
        }
    }

    // Release ownership without decrementing
    T* detach() {
        T* result = this->_ptr;
        this->_ptr = NULL;
        return result;
    }

    T* get() const { return this->_ptr; }
    T* operator->() const { return this->_ptr; }
    T& operator*() const { return *this->_ptr; }
};
```

### The Overlapped Class

Combines Windows OVERLAPPED with reference counting:

```cpp
class Overlapped : public OVERLAPPED, public RefCounted<Overlapped> {
public:
    Overlapped() {
        this->Internal = 0;
        this->InternalHigh = 0;
        this->Offset = 0;
        this->OffsetHigh = 0;
        this->hEvent = NULL;
    }

    // Set file offset for async I/O
    void set_offset(unsigned long long const offset) {
        this->Offset = static_cast<unsigned long>(offset);
        this->OffsetHigh = static_cast<unsigned long>(offset >> 32);
    }

    unsigned long long get_offset() const {
        return static_cast<unsigned long long>(this->Offset) |
               (static_cast<unsigned long long>(this->OffsetHigh) << 32);
    }

    // Completion callback - override in derived classes
    virtual int operator()(size_t const size, ULONG_PTR const key) {
        return 0;  // 0 = done, >0 = requeue, <0 = error
    }
};
```

**Lifecycle of an Overlapped operation:**

```
1. Create Overlapped (refcount = 1)
2. Wrap in intrusive_ptr (refcount = 1, no add)
3. Issue async I/O, detach from intrusive_ptr (refcount = 1, IOCP owns)
4. I/O completes, IOCP worker wraps in intrusive_ptr (refcount = 1, no add)
5. Handler executes
6. intrusive_ptr destructor calls Release (refcount = 0, deleted)
```

---

## BackgroundWorker - Task Queue Pattern

### Overview

BackgroundWorker provides a priority-ordered task queue with semaphore signaling:

```cpp
class BackgroundWorker {
    Handle _semaphore;           // Signals pending work
    Handle _thread;              // Worker thread handle
    atomic<bool> _stop;          // Shutdown flag
    recursive_mutex _mutex;      // Protects queue

    // Priority queue: higher priority = processed first
    std::vector<std::pair<int, Thunk>> _queue;

public:
    BackgroundWorker()
        : _semaphore(CreateSemaphore(NULL, 0, LONG_MAX, NULL))
        , _thread(NULL)
        , _stop(false) {}
};
```

### The Thunk Abstraction

A Thunk wraps any callable for deferred execution:

```cpp
class Thunk {
    struct Base {
        virtual ~Base() {}
        virtual void operator()() = 0;
    };

    template<class F>
    struct Impl : Base {
        F _func;
        Impl(F const& func) : _func(func) {}
        void operator()() override { this->_func(); }
    };

    std::unique_ptr<Base> _impl;

public:
    template<class F>
    Thunk(F const& func) : _impl(new Impl<F>(func)) {}

    void operator()() { (*this->_impl)(); }
};
```

### Posting Work

```cpp
void post(int priority, Thunk&& thunk) {
    {
        unique_lock<recursive_mutex> guard(this->_mutex);

        // Insert maintaining priority order (higher first)
        auto it = std::lower_bound(
            this->_queue.begin(), this->_queue.end(),
            priority,
            [](auto const& item, int p) { return item.first > p; }
        );
        this->_queue.insert(it, std::make_pair(priority, std::move(thunk)));
    }

    // Signal worker thread
    ReleaseSemaphore(this->_semaphore, 1, NULL);
}
```

### Worker Thread Loop

```cpp
static unsigned int CALLBACK worker_entry(void* arg) {
    return static_cast<BackgroundWorker*>(arg)->worker();
}

unsigned int worker() {
    while (!this->_stop.load()) {
        // Wait for work
        DWORD result = WaitForSingleObject(this->_semaphore, INFINITE);

        if (result != WAIT_OBJECT_0 || this->_stop.load()) {
            break;
        }

        // Get highest priority task
        Thunk thunk;
        {
            unique_lock<recursive_mutex> guard(this->_mutex);
            if (!this->_queue.empty()) {
                thunk = std::move(this->_queue.front().second);
                this->_queue.erase(this->_queue.begin());
            }
        }

        // Execute outside lock
        if (thunk) {
            thunk();
        }
    }
    return 0;
}
```

### Graceful Shutdown

```cpp
~BackgroundWorker() {
    this->_stop.store(true);

    // Wake up worker if waiting
    ReleaseSemaphore(this->_semaphore, 1, NULL);

    // Wait for thread to exit
    if (this->_thread) {
        WaitForSingleObject(this->_thread, INFINITE);
    }
}
```

---

## The MFT Reading Concurrency Model

### Two-Phase Reading Architecture

MFT reading uses a sophisticated two-phase approach:

```
Phase 1: Read MFT Bitmap (Sequential)
┌─────────────────────────────────────────────────────────────────┐
│  Read $MFT::$BITMAP to determine which records are in use       │
│  This is relatively small and read sequentially                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
Phase 2: Read MFT Records (Parallel)
┌─────────────────────────────────────────────────────────────────┐
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │
│  │ Worker  │  │ Worker  │  │ Worker  │  │ Worker  │            │
│  │   1     │  │   2     │  │   3     │  │   N     │            │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘            │
│       │            │            │            │                  │
│       ▼            ▼            ▼            ▼                  │
│  ┌─────────────────────────────────────────────────┐           │
│  │              I/O Completion Port                 │           │
│  │         (Async reads, completion dispatch)       │           │
│  └─────────────────────────────────────────────────┘           │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────┐           │
│  │              Buffer Pool (Mutex Protected)       │           │
│  │         Reusable buffers for MFT clusters        │           │
│  └─────────────────────────────────────────────────┘           │
└─────────────────────────────────────────────────────────────────┘
```

### Buffer Pool Management

```cpp
class MftReadContext {
    // Buffer pool for reading MFT clusters
    std::vector<std::unique_ptr<unsigned char[]>> _buffer_pool;
    recursive_mutex _buffer_mutex;

    // Progress tracking
    atomic<unsigned long long> _records_processed;
    atomic<unsigned long long> _bytes_read;

public:
    // Get a buffer from pool (or allocate new)
    unsigned char* acquire_buffer(size_t size) {
        unique_lock<recursive_mutex> guard(this->_buffer_mutex);

        if (!this->_buffer_pool.empty()) {
            auto buffer = std::move(this->_buffer_pool.back());
            this->_buffer_pool.pop_back();
            return buffer.release();
        }

        return new unsigned char[size];
    }

    // Return buffer to pool for reuse
    void release_buffer(unsigned char* buffer) {
        unique_lock<recursive_mutex> guard(this->_buffer_mutex);
        this->_buffer_pool.push_back(
            std::unique_ptr<unsigned char[]>(buffer));
    }

    // Atomic progress update
    void record_progress(unsigned long long records, unsigned long long bytes) {
        this->_records_processed.fetch_add(records, memory_order_relaxed);
        this->_bytes_read.fetch_add(bytes, memory_order_relaxed);
    }
};
```

### Concurrent Read Completion Handler

```cpp
class MftReadOverlapped : public Overlapped {
    MftReadContext* _context;
    unsigned char* _buffer;
    unsigned long long _vcn;  // Virtual cluster number
    size_t _clusters;

public:
    int operator()(size_t const bytes_read, ULONG_PTR const key) override {
        // Parse MFT records in this buffer
        size_t records = this->parse_records(this->_buffer, bytes_read);

        // Update progress atomically
        this->_context->record_progress(records, bytes_read);

        // Return buffer to pool
        this->_context->release_buffer(this->_buffer);
        this->_buffer = NULL;

        return 0;  // Done with this operation
    }

private:
    size_t parse_records(unsigned char* buffer, size_t size) {
        // Parse FILE records, extract names, build index
        // This runs on IOCP worker thread
        // ...
    }
};
```

### Issuing Concurrent Reads

```cpp
void read_mft_parallel(HANDLE volume, MftReadContext& context,
                       std::vector<Extent> const& extents) {
    IoCompletionPort iocp;
    iocp.associate(volume, 0);

    // Issue reads for all extents
    for (auto const& extent : extents) {
        unsigned char* buffer = context.acquire_buffer(extent.size);

        intrusive_ptr<MftReadOverlapped> overlapped(
            new MftReadOverlapped(&context, buffer, extent.vcn, extent.clusters));

        overlapped->set_offset(extent.lcn * cluster_size);

        DWORD bytes_read;
        BOOL result = ReadFile(volume, buffer,
                               static_cast<DWORD>(extent.size),
                               &bytes_read, overlapped.get());

        if (result || GetLastError() == ERROR_IO_PENDING) {
            overlapped.detach();  // IOCP owns it now
        } else {
            context.release_buffer(buffer);
            // Handle error
        }
    }

    // Wait for all completions
    iocp.wait_all();
}
```

---

## Memory Ordering and Volatile

### When to Use volatile

In UFFS, `volatile` is used extensively but carefully:

```cpp
// Correct: volatile for cross-thread visibility
class IoCompletionPort {
    atomic<bool> volatile _terminated;  // Modified by multiple threads

    // Methods are volatile-qualified to work with volatile this
    bool is_terminated() const volatile {
        return this->_terminated.load();
    }
};
```

**volatile does NOT provide:**
- Atomicity (use atomic<T>)
- Memory ordering (use memory barriers)
- Thread synchronization (use mutexes)

**volatile DOES provide:**
- Prevents compiler from caching in registers
- Prevents compiler from reordering around volatile accesses
- Required for memory-mapped I/O

### Acquire-Release Semantics

```cpp
// Producer thread
void producer() {
    // Prepare data
    this->_data = compute_data();

    // Release: all writes before this are visible to acquirers
    this->_ready.store(true, memory_order_release);
}

// Consumer thread
void consumer() {
    // Acquire: see all writes before the release
    while (!this->_ready.load(memory_order_acquire)) {
        // Spin
    }

    // Safe to read _data - guaranteed to see producer's write
    use(this->_data);
}
```

### Double-Checked Locking Pattern

Used for lazy initialization:

```cpp
class LazyResource {
    atomic<Resource*> _resource;
    recursive_mutex _mutex;

public:
    Resource* get() {
        // First check (no lock)
        Resource* r = this->_resource.load(memory_order_acquire);
        if (r) return r;

        // Lock and check again
        unique_lock<recursive_mutex> guard(this->_mutex);
        r = this->_resource.load(memory_order_relaxed);
        if (r) return r;

        // Initialize
        r = new Resource();
        this->_resource.store(r, memory_order_release);
        return r;
    }
};
```

---

## Thread Safety Patterns

### Pattern 1: Lock Acquisition Order

To prevent deadlocks, always acquire locks in consistent order:

```cpp
// WRONG: Can deadlock
void transfer(Account& from, Account& to, int amount) {
    unique_lock<recursive_mutex> lock1(from._mutex);
    unique_lock<recursive_mutex> lock2(to._mutex);  // Deadlock if another thread does to->from
    // ...
}

// CORRECT: Order by address
void transfer(Account& from, Account& to, int amount) {
    Account* first = &from < &to ? &from : &to;
    Account* second = &from < &to ? &to : &from;

    unique_lock<recursive_mutex> lock1(first->_mutex);
    unique_lock<recursive_mutex> lock2(second->_mutex);
    // ...
}
```

### Pattern 2: UI Thread Communication

Never block the UI thread. Use PostMessage for cross-thread communication:

```cpp
// Worker thread
void on_search_complete(std::vector<Result> results) {
    // Allocate on heap for transfer
    auto* data = new std::vector<Result>(std::move(results));

    // Post to UI thread (non-blocking)
    PostMessage(hwnd, WM_SEARCH_COMPLETE, 0, reinterpret_cast<LPARAM>(data));
}

// UI thread message handler
case WM_SEARCH_COMPLETE: {
    auto* results = reinterpret_cast<std::vector<Result>*>(lParam);
    update_listview(*results);
    delete results;  // UI thread owns it now
    return 0;
}
```

### Pattern 3: Read-Copy-Update (RCU) Style

For read-heavy data structures:

```cpp
class SearchIndex {
    atomic<IndexData*> _current;

public:
    // Readers: lock-free
    void search(Query const& query) {
        IndexData* data = this->_current.load(memory_order_acquire);
        // Use data - guaranteed valid during this call
        data->search(query);
    }

    // Writers: exclusive
    void update(IndexData* new_data) {
        IndexData* old = this->_current.exchange(new_data, memory_order_acq_rel);

        // Defer deletion until all readers done
        // (In practice, use epoch-based reclamation or hazard pointers)
        schedule_delete(old);
    }
};
```

### Pattern 4: Condition Variable Alternative

UFFS uses semaphores instead of condition variables:

```cpp
class WorkQueue {
    Handle _semaphore;
    recursive_mutex _mutex;
    std::deque<Work> _queue;

public:
    void push(Work work) {
        {
            unique_lock<recursive_mutex> guard(this->_mutex);
            this->_queue.push_back(std::move(work));
        }
        ReleaseSemaphore(this->_semaphore, 1, NULL);
    }

    Work pop() {
        WaitForSingleObject(this->_semaphore, INFINITE);

        unique_lock<recursive_mutex> guard(this->_mutex);
        Work work = std::move(this->_queue.front());
        this->_queue.pop_front();
        return work;
    }
};
```

---

## Performance Considerations

### Lock Contention

Symptoms of high contention:
- CPU usage high but throughput low
- Threads spending time in lock acquisition

Solutions:
1. **Reduce critical section size**
2. **Use reader-writer locks** for read-heavy workloads
3. **Partition data** to reduce sharing
4. **Use lock-free algorithms** where appropriate

### False Sharing

When unrelated data shares a cache line:

```cpp
// BAD: Counters on same cache line
struct Counters {
    atomic<int> counter1;  // Thread 1 updates
    atomic<int> counter2;  // Thread 2 updates
    // Both invalidate each other's cache line!
};

// GOOD: Pad to separate cache lines
struct Counters {
    alignas(64) atomic<int> counter1;
    alignas(64) atomic<int> counter2;
};
```

### Cache Line Alignment

```cpp
// Ensure structure is cache-line aligned
struct alignas(64) PerThreadData {
    atomic<unsigned long long> bytes_processed;
    atomic<unsigned long long> records_found;
    // Padding to fill cache line
    char _padding[64 - 2 * sizeof(atomic<unsigned long long>)];
};
```

---

## Common Pitfalls and Solutions

### Pitfall 1: Forgetting Memory Barriers

```cpp
// WRONG: No synchronization
bool ready = false;
int data;

void producer() {
    data = 42;
    ready = true;  // Compiler/CPU may reorder!
}

void consumer() {
    while (!ready);  // May see ready=true but data=0!
    use(data);
}

// CORRECT: Use atomics with proper ordering
atomic<bool> ready(false);
int data;

void producer() {
    data = 42;
    ready.store(true, memory_order_release);
}

void consumer() {
    while (!ready.load(memory_order_acquire));
    use(data);  // Guaranteed to see data=42
}
```

### Pitfall 2: Lock Scope Too Large

```cpp
// WRONG: Holding lock during I/O
void process() {
    unique_lock<recursive_mutex> guard(this->_mutex);
    auto data = this->_queue.front();
    this->_queue.pop_front();

    // Still holding lock during slow I/O!
    write_to_disk(data);
}

// CORRECT: Minimize lock scope
void process() {
    Data data;
    {
        unique_lock<recursive_mutex> guard(this->_mutex);
        data = std::move(this->_queue.front());
        this->_queue.pop_front();
    }
    // Lock released before I/O
    write_to_disk(data);
}
```

### Pitfall 3: Reference Counting Errors

```cpp
// WRONG: Double-release
void bad_code() {
    Overlapped* p = new Overlapped();  // refcount = 1
    intrusive_ptr<Overlapped> ptr1(p);  // refcount = 2 (adds ref)
    intrusive_ptr<Overlapped> ptr2(p);  // refcount = 3 (adds ref)
}  // Both release, refcount = 1, LEAK!

// CORRECT: Use intrusive_ptr from the start
void good_code() {
    intrusive_ptr<Overlapped> ptr1(new Overlapped());  // refcount = 1
    intrusive_ptr<Overlapped> ptr2(ptr1);  // refcount = 2 (copy adds ref)
}  // Both release, refcount = 0, deleted
```

### Pitfall 4: Async Lifetime Issues

```cpp
// WRONG: Stack buffer for async I/O
void bad_async() {
    char buffer[4096];  // Stack buffer
    OVERLAPPED ov = {};
    ReadFile(file, buffer, sizeof(buffer), NULL, &ov);
    // Function returns, buffer destroyed, I/O writes to garbage!
}

// CORRECT: Heap buffer with reference counting
void good_async() {
    intrusive_ptr<MyOverlapped> ov(new MyOverlapped());
    ov->buffer = new char[4096];  // Heap buffer
    ReadFile(file, ov->buffer, 4096, NULL, ov.get());
    ov.detach();  // IOCP owns it, will clean up on completion
}
```

---

## Summary

UFFS's concurrency model is built on these key principles:

1. **IOCP for scalable async I/O** - Single wait point, kernel-managed thread pool
2. **Custom atomics for control** - Platform-specific optimizations
3. **Reference counting for async lifetimes** - Clear ownership semantics
4. **Mutex hierarchy** - CRITICAL_SECTION for performance, spin locks for short sections
5. **UI thread isolation** - PostMessage for cross-thread communication
6. **Memory ordering awareness** - Explicit acquire/release semantics

Understanding these patterns is essential for:
- Debugging race conditions
- Adding new concurrent features
- Optimizing performance
- Maintaining thread safety invariants

